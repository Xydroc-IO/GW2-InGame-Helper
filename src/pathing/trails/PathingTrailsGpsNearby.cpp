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

	/* Activation + along-path window track the Overview "GPS range" slider.
	   Earlier floors (320m / 520m) made 40-200m adjustments look broken. */
	float maxDist = std::clamp(maxDistMeters, 40.f, 200.f);
	float activateDist = maxDist * 1.35f;
	float alongBudget = maxDist * 2.25f;
	size_t maxPts = 1200;
	const bool wpMode = G::LadyWpOnly;
	const bool hpMode = G::LadyHeroPointTrain;
	if (wpMode || hpMode)
	{
		/* Sparse WP/HP corridors need a bit more reach, still slider-scaled. */
		maxDist = std::clamp(maxDistMeters * 1.35f, 55.f, 280.f);
		activateDist = maxDist * 1.55f;
		alongBudget = maxDist * 2.75f;
		maxPts = 1400;
	}
	const float activateDist2 = activateDist * activateDist;
	auto dist2 = [&](float x, float y, float z) {
		const float dx = avatarX - x;
		const float dy = avatarY - y;
		const float dz = avatarZ - z;
		return dx * dx + dy * dy * 0.25f + dz * dz;
	};
	auto segLen = [](const WorldPoint& p, const WorldPoint& q) -> float {
		const float dx = p.x - q.x;
		const float dy = p.y - q.y;
		const float dz = p.z - q.z;
		const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
		return std::isfinite(d) ? d : 1.0e30f;
	};

	struct Cand
	{
		size_t idx = 0;
		size_t nearest = 0;
		float nearestD2 = 1.0e30f;
	};
	std::vector<Cand> cands;
	cands.reserve(48);
	int pointTests = 0;
	constexpr int kMaxPointTests = 48000;
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
		/* Dense enough that long Lady/Tekkit polylines still catch the player. */
		const size_t step = std::max<size_t>(1, n / 192);
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
		const size_t refine = std::max<size_t>(step * 2, 16);
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
		/* Also accept if a segment straddles the player (vertex sample miss). */
		if (bestD > activateDist2 && step > 1)
		{
			for (size_t i = 0; i + step < n; i += step)
			{
				const WorldPoint& p0 = tr.worldPoints[i];
				const WorldPoint& p1 = tr.worldPoints[std::min(n - 1, i + step)];
				if (!std::isfinite(p0.x) || !std::isfinite(p1.x))
					continue;
				const float mx = (p0.x + p1.x) * 0.5f;
				const float my = (p0.y + p1.y) * 0.5f;
				const float mz = (p0.z + p1.z) * 0.5f;
				const float d = dist2(mx, my, mz);
				if (d < bestD)
				{
					bestD = d;
					bestI = i;
				}
			}
		}
		if (bestD <= activateDist2)
			cands.push_back({ti, bestI, bestD});
		if (pointTests > kMaxPointTests)
			break;
	}
	std::sort(cands.begin(), cands.end(),
		[&](const Cand& a, const Cand& b) {
			auto prio = [&](size_t idx) -> int {
				const char* lab = gCurrentAll[idx].label;
				if (wpMode && std::strstr(lab, ".wp") != nullptr)
					return 0;
				if (hpMode && std::strstr(lab, "legs.hp") != nullptr)
					return 0;
				if (std::strstr(lab, "heartpath") != nullptr)
					return 1;
				return 2;
			};
			const int pa = prio(a.idx);
			const int pb = prio(b.idx);
			if (pa != pb && a.nearestD2 <= activateDist2 && b.nearestD2 <= activateDist2)
				return pa < pb;
			return a.nearestD2 < b.nearestD2;
		});

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
	/* Only collapse true duplicates (same corridor + same tint/tex). */
	auto roughlySamePath = [&](const WorldSnippet& a, const WorldSnippet& b) -> bool
	{
		if (a.points.size() < 2 || b.points.size() < 2)
			return false;
		if (a.color != b.color)
			return false;
		if (std::strncmp(a.textureId, b.textureId, sizeof(a.textureId)) != 0)
			return false;
		constexpr float r2 = 3.0f * 3.0f;
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
		return hits >= 4;
	};

	outSnippets.reserve(40);
	for (const Cand& c : cands)
	{
		if (outSnippets.size() >= 40)
			break;
		const Trail& tr = gCurrentAll[c.idx];
		const auto& pts = tr.worldPoints;
		const size_t n = pts.size();
		if (c.nearest >= n || !std::isfinite(pts[c.nearest].x))
			continue;

		/* Heart / WP / HP train: full TacO section - sparse waypoint gaps break
		   along-budget windows (compass showed full path; world GPS looked cut). */
		const char* lab = tr.label;
		const size_t labN = std::strlen(lab);
		const bool isWpTrail =
			(labN >= 3 && std::strcmp(lab + labN - 3, ".wp") == 0 &&
				std::strstr(lab, ".wp.") == nullptr);
		const bool fullSection =
			std::strstr(lab, "heartpath") != nullptr ||
			std::strstr(tr.textureId, "Heart") != nullptr ||
			std::strstr(lab, "legs.hp.") != nullptr ||
			std::strstr(lab, "leag.hp.") != nullptr ||
			std::strcmp(lab, "legs.hp") == 0 ||
			std::strcmp(lab, "leag.hp") == 0 ||
			isWpTrail;
		const float budget = fullSection ? 1.0e9f : alongBudget;
		size_t a = c.nearest, b = c.nearest;
		for (float used = 0.f; a > 0; )
		{
			if (!std::isfinite(pts[a - 1].x) || !std::isfinite(pts[a - 1].y) ||
				!std::isfinite(pts[a - 1].z))
				break;
			const float L = segLen(pts[a], pts[a - 1]);
			if (!(L < 160.f) || used + L > budget)
				break;
			used += L;
			--a;
		}
		for (float used = 0.f; b + 1 < n; )
		{
			if (!std::isfinite(pts[b + 1].x) || !std::isfinite(pts[b + 1].y) ||
				!std::isfinite(pts[b + 1].z))
				break;
			const float L = segLen(pts[b], pts[b + 1]);
			if (!(L < 160.f) || used + L > budget)
				break;
			used += L;
			++b;
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
		constexpr float kMinSp2 = 0.35f * 0.35f;
		/* WP segments can span hundreds of meters between waypoints. */
		const float maxGap2 = fullSection ? (1200.f * 1200.f) : (160.f * 160.f);
		const size_t ptCap = fullSection ? std::max(maxPts, size_t{2800}) : maxPts;
		snip.points.reserve(std::min(b - a + 1, ptCap));
		WorldPoint lastKept{};
		bool haveKept = false;
		size_t firstIdx = a;
		for (size_t i = a; i <= b; ++i)
		{
			const WorldPoint& wp = pts[i];
			if (!std::isfinite(wp.x) || !std::isfinite(wp.y) || !std::isfinite(wp.z))
				break;
			if (haveKept)
			{
				const float gap2 = distPts2(wp, lastKept);
				if (gap2 < kMinSp2 && i != b)
					continue;
				if (gap2 > maxGap2)
					break;
			}
			else
				firstIdx = i;
			snip.points.push_back(wp);
			lastKept = wp;
			haveKept = true;
			if (snip.points.size() >= ptCap)
				break;
		}
		if (snip.points.size() < 2)
			continue;
		/* UV along0 from section start. */
		size_t sec0 = firstIdx;
		while (sec0 > 0 &&
			std::isfinite(pts[sec0 - 1].x) && std::isfinite(pts[sec0 - 1].y) &&
			std::isfinite(pts[sec0 - 1].z))
			--sec0;
		float uv0 = 0.f;
		for (size_t i = sec0; i < firstIdx; ++i)
		{
			const float L = segLen(pts[i], pts[i + 1]);
			if (L < 160.f)
				uv0 += L;
		}
		snip.uvAlong0 = uv0;
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
		/* Match trail gate - compass-visible markers belong in world GPS too. */
		if (!MarkerShownInWorld(marker) && !marker.minimapVisible)
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
