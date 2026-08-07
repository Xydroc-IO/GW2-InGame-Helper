#include "PathingTrails.h"

#include "Globals.h"
#include "MarkerBehaviors.h"
#include "MumbleIdentity.h"
#include "PathingIndex.h"
#include "PathingLua.h"
#include "PathingLuaInternal.h"
#include "PathingSchedule.h"
#include "Settings.h"
#include "WaypointsData.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

using namespace PathingDetail;

namespace
{
	void RefreshGuideWpCache(uint32_t mapId)
	{
		std::vector<PathingTrails::Point> pts;
		if (mapId != 0)
		{
			WaypointsData::EnsureLoaded(false);
			std::vector<WaypointsData::Poi> pois;
			WaypointsData::ListForMap(static_cast<int>(mapId), true, pois);
			pts.reserve(pois.size());
			for (const WaypointsData::Poi& p : pois)
			{
				if (!p.hasCoord || !std::isfinite(p.continentX) || !std::isfinite(p.continentY))
					continue;
				pts.push_back({p.continentX, p.continentY});
			}
		}
		std::lock_guard<std::mutex> lock(gMutex);
		gGuideWpCache = std::move(pts);
	}
}

bool PathingTrails::TryTrailStartContinent(float* outX, float* outY,
	char* labelOut, size_t labelLen, bool preferEnabled)
{
	if (!outX || !outY)
		return false;
	std::lock_guard<std::mutex> lock(gMutex);
	auto tryPick = [&](bool enabledOnly) -> bool {
		for (const Trail& t : gCurrentAll)
		{
			if (enabledOnly && !TypeEnabledLocked(t.label))
				continue;
			for (const Point& p : t.points)
			{
				if (!std::isfinite(p.x) || !std::isfinite(p.y))
					continue;
				if (p.x == 0.f && p.y == 0.f)
					continue;
				*outX = p.x;
				*outY = p.y;
				if (labelOut && labelLen)
					std::snprintf(labelOut, labelLen, "%s", t.label);
				return true;
			}
		}
		return false;
	};
	if (preferEnabled && tryPick(true))
		return true;
	return tryPick(false);
}

void PathingTrails::SetSearchDestination(float continentX, float continentY)
{
	uint32_t mapId = 0;
	bool needRects = false;
	{
		if (G::Mumble && G::Mumble->context_len >= sizeof(MumbleContext))
		{
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			if (ctx && ctx->mapId != 0)
				mapId = ctx->mapId;
		}
		/* WP index outside pathing lock — pathfinder needs it. */
		RefreshGuideWpCache(mapId);

		std::lock_guard<std::mutex> lock(gMutex);
		gGuideActive = true;
		gGuideDestX = continentX;
		gGuideDestY = continentY;
		gGuide = {};
		if (G::Mumble && G::Mumble->context_len >= sizeof(MumbleContext))
		{
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			if (ctx && ctx->mapId != 0)
			{
				gGuidePlayerX = ctx->playerX;
				gGuidePlayerY = ctx->playerY;
				gGuideHavePlayer = true;
				if (gActiveMap == 0)
					gActiveMap = ctx->mapId;
				mapId = gActiveMap ? gActiveMap : ctx->mapId;
			}
		}
		if (gActiveMap != 0)
			mapId = gActiveMap;
		const auto rit = gRects.find(mapId);
		needRects = mapId != 0 && (rit == gRects.end() || !rit->second.valid);
		RebuildSearchGuideLocked();
		if (gGuide.worldPoints.size() >= 2)
			return;
	}

	if (needRects && mapId != 0)
	{
		Rects rects{};
		if (FetchMapRects(mapId, rects) && rects.valid)
		{
			std::lock_guard<std::mutex> lock(gMutex);
			if (gActiveMap == mapId || gActiveMap == 0)
			{
				gRects[mapId] = rects;
				if (gGuideActive)
					RebuildSearchGuideLocked();
			}
		}
	}
}

void PathingTrails::ClearSearchGuide()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gGuideActive = false;
	gGuide = {};
}

bool PathingTrails::HasSearchGuide()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gGuideActive && gGuide.points.size() >= 2;
}

bool PathingTrails::HasSearchGuideActive()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gGuideActive;
}

PathingTrails::Trail PathingTrails::SearchGuide()
{
	std::lock_guard<std::mutex> lock(gMutex);
	Trail slim{};
	slim.mapId = gGuide.mapId;
	slim.color = gGuide.color;
	std::snprintf(slim.label, sizeof(slim.label), "%s", gGuide.label);
	slim.points = gGuide.points; /* continent only for minimap */
	return slim;
}
