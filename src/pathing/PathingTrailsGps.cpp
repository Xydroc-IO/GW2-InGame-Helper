#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
	/* Nearby slice only — short range so GPS does not paint through walls/map. */
	const float maxDist = std::clamp(maxDistMeters, 10.f, 120.f);
	const float softDist = maxDist * 1.35f;
	const float softDist2 = softDist * softDist;

	if (!std::isfinite(avatarX) || !std::isfinite(avatarY) || !std::isfinite(avatarZ))
		return out;

	auto dist2 = [&](float x, float y, float z) {
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
			return 1.0e30f;
		const float dx = avatarX - x;
		const float dy = avatarY - y;
		const float dz = avatarZ - z;
		const float d = dx * dx + dy * dy + dz * dz;
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
		snip.alpha = tr.alpha;
		snip.trailScale = tr.trailScale;
		snip.widthBias = LadyGpsWidthBias(tr.label);
		snip.fadeNear = tr.fadeNear;
		snip.fadeFar = tr.fadeFar;
		/* Local slice — keep enough points for long HP sections near the player. */
		constexpr size_t kMaxPts = 192;
		snip.points.reserve(std::min(b - a + 1, kMaxPts));
		const size_t span = b - a;
		const size_t stride = (span > kMaxPts) ? (span / kMaxPts) : 1;
		for (size_t i = a; i <= b; i += std::max<size_t>(1, stride))
		{
			const WorldPoint& wp = pts[i];
			if (!std::isfinite(wp.x) || !std::isfinite(wp.y) || !std::isfinite(wp.z))
				break; /* section end — do not skip and stitch */
			snip.points.push_back(wp);
			if (snip.points.size() >= kMaxPts)
				break;
		}
		if (snip.points.size() >= 2)
			out.push_back(std::move(snip));
		if (pointTests > maxPointTests)
			break;
	}
	return out;
}

bool PathingTrails::TryNearbyWorldGps(
	float avatarX, float avatarY, float avatarZ, float maxDistMeters,
	std::vector<WorldSnippet>& outSnippets,
	std::vector<Marker>& outMarkers)
{
	outSnippets.clear();
	outMarkers.clear();
	std::unique_lock<std::mutex> lock(gMutex, std::defer_lock);
	/* Prefer a real lock so GPS does not blank every frame; only try_lock while
	   the pack worker holds the mutex (avoids hitching the render thread). */
	if (gLoading.load(std::memory_order_acquire))
	{
		if (!lock.try_lock())
			return false;
	}
	else
		lock.lock();

	if (!std::isfinite(avatarX) || !std::isfinite(avatarY) || !std::isfinite(avatarZ))
		return true;

	/* Activation + draw window — slightly generous so coarse sampling misses
	   do not drop the route while walking between .trl vertices. */
	float maxDist = std::clamp(maxDistMeters, 20.f, 160.f);
	float activateDist = std::max(maxDist * 1.55f, 110.f);
	float softDist = activateDist;
	size_t maxPts = 192;
	/* WP Only chains are long and sparse — pull a much longer window ahead. */
	if (G::LadyWpOnly)
	{
		maxDist = std::clamp(std::max(maxDistMeters, 180.f) * 1.85f, 160.f, 340.f);
		activateDist = std::max(maxDist * 1.4f, 220.f);
		softDist = activateDist * 1.35f;
		maxPts = 420;
	}
	const float activateDist2 = activateDist * activateDist;
	const float softDist2 = softDist * softDist;
	auto dist2 = [&](float x, float y, float z) {
		const float dx = avatarX - x;
		const float dy = avatarY - y;
		const float dz = avatarZ - z;
		return dx * dx + dy * dy + dz * dz;
	};

	struct Cand
	{
		size_t idx = 0;
		size_t nearest = 0;
		float nearestD2 = 1.0e30f;
	};
	std::vector<Cand> cands;
	cands.reserve(32);
	int pointTests = 0;
	constexpr int kMaxPointTests = 12000;
	for (size_t ti = 0; ti < gCurrentAll.size(); ++ti)
	{
		const Trail& tr = gCurrentAll[ti];
		if (tr.worldPoints.size() < 2 || !tr.inGameVisible || !TypeEnabledLocked(tr.label))
			continue;
		const size_t n = tr.worldPoints.size();
		size_t bestI = 0;
		float bestD = 1.0e30f;
		/* Denser than n/40 — sparse samples missed the player between vertices. */
		const size_t step = std::max<size_t>(1, n / 96);
		for (size_t i = 0; i < n; i += step)
		{
			if (++pointTests > kMaxPointTests)
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
		/* Refine near the coarse hit (same section only). */
		const size_t refine = std::max<size_t>(step, 8);
		const size_t lo = bestI > refine ? bestI - refine : 0;
		const size_t hi = std::min(n, bestI + refine + 1);
		for (size_t i = lo; i < hi; ++i)
		{
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
		if (bestD <= activateDist2)
			cands.push_back({ti, bestI, bestD});
		if (pointTests > kMaxPointTests)
			break;
	}
	std::sort(cands.begin(), cands.end(),
		[](const Cand& a, const Cand& b) { return a.nearestD2 < b.nearestD2; });

	auto samplePt = [](const std::vector<WorldPoint>& pts, float t) -> WorldPoint
	{
		if (pts.empty())
			return {};
		const float u = std::clamp(t, 0.f, 1.f);
		const size_t i = static_cast<size_t>(u * static_cast<float>(pts.size() - 1));
		return pts[std::min(i, pts.size() - 1)];
	};
	auto distPts2 = [](const WorldPoint& p, const WorldPoint& q) -> float
	{
		const float dx = p.x - q.x;
		const float dy = p.y - q.y;
		const float dz = p.z - q.z;
		return dx * dx + dy * dy + dz * dz;
	};
	/* Same corridor = Foot/Griffon/pack clone. Mid samples catch parallels. */
	auto roughlySamePath = [&](const WorldSnippet& a, const WorldSnippet& b) -> bool
	{
		if (a.points.size() < 2 || b.points.size() < 2)
			return false;
		constexpr float r2 = 5.f * 5.f;
		int hits = 0;
		for (float t = 0.f; t <= 1.001f; t += 0.25f)
		{
			const WorldPoint pa = samplePt(a.points, t);
			float best = 1.0e30f;
			for (float u = 0.f; u <= 1.001f; u += 0.2f)
				best = std::min(best, distPts2(pa, samplePt(b.points, u)));
			if (best <= r2)
				++hits;
		}
		return hits >= 3;
	};

	outSnippets.reserve(5);
	for (const Cand& c : cands)
	{
		if (outSnippets.size() >= 5)
			break;
		const Trail& tr = gCurrentAll[c.idx];
		const auto& pts = tr.worldPoints;
		const size_t n = pts.size();
		if (c.nearest >= n || !std::isfinite(pts[c.nearest].x))
			continue;
		/* One TacO section only — expanding to index 0 across NaN breaks caused
		   screen-long gold stretch lines on Lady HP (and similar) packs. */
		size_t a = c.nearest;
		size_t b = c.nearest;
		while (a > 0 &&
			std::isfinite(pts[a - 1].x) && std::isfinite(pts[a - 1].y) &&
			std::isfinite(pts[a - 1].z) &&
			dist2(pts[a - 1].x, pts[a - 1].y, pts[a - 1].z) <= softDist2)
			--a;
		while (b + 1 < n &&
			std::isfinite(pts[b + 1].x) && std::isfinite(pts[b + 1].y) &&
			std::isfinite(pts[b + 1].z) &&
			dist2(pts[b + 1].x, pts[b + 1].y, pts[b + 1].z) <= softDist2)
			++b;
		if (b <= a)
			continue;

		WorldSnippet snip;
		snip.color = tr.color;
		std::snprintf(snip.textureId, sizeof(snip.textureId), "%s", tr.textureId);
		snip.alpha = tr.alpha;
		snip.trailScale = tr.trailScale;
		snip.widthBias = LadyGpsWidthBias(tr.label);
		snip.fadeNear = tr.fadeNear;
		snip.fadeFar = tr.fadeFar;
		/* ≥1 m spacing — Blish GPU strip hides micro-samples; ImGui cannot. */
		constexpr float kMinSp2 = 1.0f * 1.0f;
		constexpr float kMaxKeepGap2 = 40.f * 40.f;
		snip.points.reserve(std::min(b - a + 1, maxPts));
		WorldPoint lastKept{};
		bool haveKept = false;
		for (size_t i = a; i <= b; ++i)
		{
			const WorldPoint& wp = pts[i];
			if (!std::isfinite(wp.x) || !std::isfinite(wp.y) || !std::isfinite(wp.z))
				break; /* do not skip breaks and stitch sections */
			if (haveKept)
			{
				const float gap2 = distPts2(wp, lastKept);
				if (gap2 < kMinSp2 && i != b)
					continue;
				/* Hard break — stitching across a big jump is the gold stretch. */
				if (gap2 > kMaxKeepGap2)
					break;
			}
			snip.points.push_back(wp);
			lastKept = wp;
			haveKept = true;
			if (snip.points.size() >= maxPts)
				break;
		}
		if (snip.points.size() < 2)
			continue;
		bool dupPath = false;
		for (const WorldSnippet& prev : outSnippets)
		{
			if (roughlySamePath(prev, snip))
			{
				dupPath = true;
				break;
			}
		}
		if (dupPath)
			continue;
		outSnippets.push_back(std::move(snip));
	}

	const float markDist2 = (activateDist * 1.55f) * (activateDist * 1.55f);
	outMarkers.reserve(64);
	/* Prefer Mounts/shortcut icons so they are not starved by heart/POI spam. */
	std::vector<const Marker*> mountFirst;
	mountFirst.reserve(std::min<size_t>(gCurrentMarkers.size(), 256));
	for (const Marker& marker : gCurrentMarkers)
	{
		if (!TypeEnabledLocked(marker.label))
			continue;
		if (!MarkerShownInWorld(marker))
			continue;
		if (!MarkerBehaviorVisible(marker))
			continue;
		const float d = dist2(marker.world.x, marker.world.y, marker.world.z);
		if (d > markDist2)
			continue;
		mountFirst.push_back(&marker);
	}
	std::stable_sort(mountFirst.begin(), mountFirst.end(),
		[&](const Marker* a, const Marker* b) {
			const bool am = IsMountShortcutMarker(*a);
			const bool bm = IsMountShortcutMarker(*b);
			if (am != bm)
				return am && !bm;
			const float da = dist2(a->world.x, a->world.y, a->world.z);
			const float db = dist2(b->world.x, b->world.y, b->world.z);
			return da < db;
		});
	for (const Marker* marker : mountFirst)
	{
		outMarkers.push_back(*marker);
		if (outMarkers.size() >= 120)
			break;
	}
	return true;
}

PathingTrails::WorldSnippet PathingTrails::SearchGuideWorldSnippet()
{
	/* Blocking lock — try_lock returned empty mid-Update and blinked the guide. */
	std::lock_guard<std::mutex> lock(gMutex);
	WorldSnippet snip;
	if (!gGuideActive || gGuide.worldPoints.size() < 2)
		return snip;
	snip.color = gGuide.color ? gGuide.color : 0xFFFFAA20u;
	snip.alpha = 1.f;
	snip.trailScale = 1.f;
	snip.points = gGuide.worldPoints;
	return snip;
}
