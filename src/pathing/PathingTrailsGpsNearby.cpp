#include "PathingTrails.h"

#include "PathingIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <vector>

using namespace PathingDetail;

std::vector<PathingTrails::WorldSnippet> PathingTrails::NearbyWorldSnippets(
	float avatarX, float avatarY, float avatarZ,
	float maxDistMeters, int maxTrails, int maxPointTests)
{
	std::vector<WorldSnippet> out;
	if (maxTrails < 1 || maxPointTests < 1)
		return out;
	/* Nearby slice only — short range so GPS does not paint through walls/map. */
	const float maxDist = std::clamp(maxDistMeters, 10.f, 200.f);
	const float softDist = maxDist * 1.55f;
	const float softDist2 = softDist * softDist;

	if (!std::isfinite(avatarX) || !std::isfinite(avatarY) || !std::isfinite(avatarZ))
		return out;

	auto dist2 = [&](float x, float y, float z) {
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
			return 1.0e30f;
		const float dx = avatarX - x;
		const float dy = avatarY - y;
		const float dz = avatarZ - z;
		const float d = dx * dx + dy * dy * 0.25f + dz * dz;
		return std::isfinite(d) ? d : 1.0e30f;
	};

	/* Never block the render thread — a held worker lock froze/crashed Wine. */
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return out;
	if (gCurrentAll.empty())
		return out;

	struct Cand
	{
		size_t idx = 0;
		size_t nearest = 0;
		float nearestD2 = 1.0e30f;
	};
	std::vector<Cand> cands;
	cands.reserve(32);

	int pointTests = 0;
	for (size_t ti = 0; ti < gCurrentAll.size(); ++ti)
	{
		const Trail& tr = gCurrentAll[ti];
		if (tr.worldPoints.size() < 2 || !TypeEnabledLocked(tr.label))
			continue;
		if (!tr.inGameVisible && !tr.minimapVisible)
			continue;
		const size_t n = tr.worldPoints.size();
		size_t bestI = 0;
		float bestD = 1.0e30f;
		const size_t step = std::max<size_t>(1, n / 20);
		for (size_t i = 0; i < n; i += step)
		{
			if (++pointTests > maxPointTests)
				break;
			const WorldPoint& p = tr.worldPoints[i];
			if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
				continue;
			const float d = dist2(p.x, p.y, p.z);
			if (d < bestD)
			{
				bestD = d;
				bestI = i;
			}
		}
		if (bestD <= softDist2)
			cands.push_back({ti, bestI, bestD});
		if (pointTests > maxPointTests)
			break;
	}

	std::sort(cands.begin(), cands.end(),
		[](const Cand& a, const Cand& b) { return a.nearestD2 < b.nearestD2; });

	out.reserve(static_cast<size_t>(std::min(maxTrails, 16)));
	for (const Cand& c : cands)
	{
		if (static_cast<int>(out.size()) >= maxTrails)
			break;
		const Trail& tr = gCurrentAll[c.idx];
		const auto& pts = tr.worldPoints;
		const size_t n = pts.size();
		if (c.nearest >= n || !std::isfinite(pts[c.nearest].x))
			continue;
		/* Stay inside one TacO section — never expand across NaN breaks. */
		size_t a = c.nearest;
		size_t b = c.nearest;
		while (a > 0 &&
			std::isfinite(pts[a - 1].x) && std::isfinite(pts[a - 1].y) &&
			std::isfinite(pts[a - 1].z) &&
			dist2(pts[a - 1].x, pts[a - 1].y, pts[a - 1].z) <= softDist2)
		{
			--a;
			if (++pointTests > maxPointTests)
				break;
		}
		while (b + 1 < n &&
			std::isfinite(pts[b + 1].x) && std::isfinite(pts[b + 1].y) &&
			std::isfinite(pts[b + 1].z) &&
			dist2(pts[b + 1].x, pts[b + 1].y, pts[b + 1].z) <= softDist2)
		{
			++b;
			if (++pointTests > maxPointTests)
				break;
		}
		if (b <= a)
			continue;

		WorldSnippet snip;
		snip.color = tr.color;
		std::snprintf(snip.textureId, sizeof(snip.textureId), "%s", tr.textureId);
		std::snprintf(snip.label, sizeof(snip.label), "%s", tr.label);
		snip.alpha = tr.alpha;
		snip.trailScale = tr.trailScale;
		snip.fadeNear = tr.fadeNear;
		snip.fadeFar = tr.fadeFar;
		constexpr size_t kMaxPts = 192;
		snip.points.reserve(std::min(b - a + 1, kMaxPts));
		const size_t span = b - a;
		const size_t stride = (span > kMaxPts) ? (span / kMaxPts) : 1;
		size_t firstIdx = a;
		bool first = true;
		for (size_t i = a; i <= b; i += std::max<size_t>(1, stride))
		{
			const WorldPoint& wp = pts[i];
			if (!std::isfinite(wp.x) || !std::isfinite(wp.y) || !std::isfinite(wp.z))
				break; /* section end — do not skip and stitch */
			if (first)
			{
				firstIdx = i;
				first = false;
			}
			snip.points.push_back(wp);
			if (snip.points.size() >= kMaxPts)
				break;
		}
		if (snip.points.size() >= 2)
		{
			size_t sec0 = firstIdx;
			while (sec0 > 0 &&
				std::isfinite(pts[sec0 - 1].x) && std::isfinite(pts[sec0 - 1].y) &&
				std::isfinite(pts[sec0 - 1].z))
				--sec0;
			float uv0 = 0.f;
			for (size_t i = sec0; i < firstIdx; ++i)
			{
				const float dx = pts[i].x - pts[i + 1].x;
				const float dy = pts[i].y - pts[i + 1].y;
				const float dz = pts[i].z - pts[i + 1].z;
				const float L = std::sqrt(dx * dx + dy * dy + dz * dz);
				if (std::isfinite(L) && L < 160.f)
					uv0 += L;
			}
			snip.uvAlong0 = uv0;
			out.push_back(std::move(snip));
		}
		if (pointTests > maxPointTests)
			break;
	}
	return out;
}
