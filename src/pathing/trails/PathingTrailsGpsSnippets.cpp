#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingSchedule.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

using namespace PathingDetail;

std::vector<PathingTrails::WorldSnippet> PathingTrails::NearbyWorldSnippets(
	float avatarX, float avatarY, float avatarZ,
	float maxDistMeters, int maxTrails, int maxPointTests)
{
	std::vector<WorldSnippet> out;
	if (maxTrails < 1 || maxPointTests < 1)
		return out;
	/* Nearby slice only - short range so GPS does not paint through walls/map. */
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

	/* Never block the render thread - a held worker lock froze/crashed Wine. */
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
		if (tr.luaHidden || tr.luaRemoved)
			continue;
		if (!tr.inGameVisible && !tr.minimapVisible)
			continue;
		if (!PathingSchedule::MarkerActive(tr.schedule, tr.scheduleDuration,
			PathingSchedule::NowUnixUtc()))
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
		/* Soft-bridge tiny TacO authoring cuts (≤25 m); keep hard portal gaps. */
		constexpr float kSoftBridgeM = 25.f;
		auto finitePt = [&](size_t i) -> bool {
			return i < n && std::isfinite(pts[i].x) && std::isfinite(pts[i].y) &&
				std::isfinite(pts[i].z);
		};
		size_t a = c.nearest;
		size_t b = c.nearest;
		while (a > 0)
		{
			size_t j = a - 1;
			bool crossed = false;
			if (!finitePt(j))
			{
				crossed = true;
				while (j > 0 && !finitePt(j))
					--j;
				if (!finitePt(j))
					break;
			}
			const float dx = pts[a].x - pts[j].x;
			const float dy = pts[a].y - pts[j].y;
			const float dz = pts[a].z - pts[j].z;
			const float L = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (crossed && !(L <= kSoftBridgeM))
				break;
			if (!std::isfinite(L) || dist2(pts[j].x, pts[j].y, pts[j].z) > softDist2)
				break;
			a = j;
			if (++pointTests > maxPointTests)
				break;
		}
		while (b + 1 < n)
		{
			size_t j = b + 1;
			bool crossed = false;
			if (!finitePt(j))
			{
				crossed = true;
				while (j + 1 < n && !finitePt(j))
					++j;
				if (!finitePt(j))
					break;
			}
			const float dx = pts[b].x - pts[j].x;
			const float dy = pts[b].y - pts[j].y;
			const float dz = pts[b].z - pts[j].z;
			const float L = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (crossed && !(L <= kSoftBridgeM))
				break;
			if (!std::isfinite(L) || dist2(pts[j].x, pts[j].y, pts[j].z) > softDist2)
				break;
			b = j;
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
		constexpr float kSoftBridge2 = kSoftBridgeM * kSoftBridgeM;
		snip.points.reserve(std::min(b - a + 1, kMaxPts));
		const size_t span = b - a;
		const size_t stride = (span > kMaxPts) ? (span / kMaxPts) : 1;
		size_t firstIdx = a;
		bool first = true;
		WorldPoint lastKept{};
		bool haveKept = false;
		for (size_t i = a; i <= b; i += std::max<size_t>(1, stride))
		{
			if (!finitePt(i))
			{
				size_t j = i + 1;
				while (j <= b && !finitePt(j))
					++j;
				if (j > b || !finitePt(j) || !haveKept)
					break;
				const float gdx = pts[j].x - lastKept.x;
				const float gdy = pts[j].y - lastKept.y;
				const float gdz = pts[j].z - lastKept.z;
				if (gdx * gdx + gdy * gdy + gdz * gdz > kSoftBridge2)
					break;
				i = j;
			}
			const WorldPoint& wp = pts[i];
			if (first)
			{
				firstIdx = i;
				first = false;
			}
			snip.points.push_back(wp);
			lastKept = wp;
			haveKept = true;
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

PathingTrails::WorldSnippet PathingTrails::SearchGuideWorldSnippet()
{
	/* Blocking lock - try_lock returned empty mid-Update and blinked the guide. */
	std::lock_guard<std::mutex> lock(gMutex);
	WorldSnippet snip;
	if (!gGuideActive || gGuide.worldPoints.size() < 2)
		return snip;
	snip.color = gGuide.color ? gGuide.color : 0xFFFFAA20u;
	snip.alpha = 1.f;
	snip.trailScale = (gGuide.trailScale >= 0.05f && gGuide.trailScale <= 8.f)
		? gGuide.trailScale : 1.45f;
	if (gGuide.textureId[0])
		std::snprintf(snip.textureId, sizeof(snip.textureId), "%s", gGuide.textureId);
	std::snprintf(snip.label, sizeof(snip.label), "%s", gGuide.label);
	snip.points = gGuide.worldPoints;
	return snip;
}
