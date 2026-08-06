#include "PathingTrails.h"

#include "Globals.h"
#include "MarkerBehaviors.h"
#include "MumbleIdentity.h"
#include "PathingIndex.h"
#include "PathingLua.h"
#include "PathingLuaInternal.h"
#include "PathingSchedule.h"
#include "Settings.h"

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

void PathingTrails::ResetMarkerBehaviorStates()
{
	MarkerBehaviors::ResetAllStates();
}

void PathingTrails::RequestMarkerInteract()
{
	MarkerBehaviors::RequestInteract();
}

void PathingTrails::DrawMarkerBehaviorOverlay()
{
	MarkerBehaviors::DrawOverlay();
}

void PathingTrails::TickMarkerBehaviors()
{
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return;
	const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	if (!ctx || ctx->mapId == 0)
		return;
	MumbleIdentity::Tick();
	std::vector<Marker> markers;
	{
		std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		markers = gCurrentMarkers;
		PathingLuaDetail::gTickTrails = &gCurrentAll;
	}
	MarkerBehaviors::Tick(
		ctx->mapId, ctx->shardId, ctx->instance,
		G::Mumble->fAvatarPosition[0],
		G::Mumble->fAvatarPosition[1],
		G::Mumble->fAvatarPosition[2],
		MumbleIdentity::CharacterName(),
		markers);
	PathingLua::Tick(markers);
	/* Write back Lua mutations. */
	{
		std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
		if (lock.owns_lock())
		{
			PathingLuaDetail::gTickTrails = nullptr;
			if (markers.size() == gCurrentMarkers.size())
			{
				for (size_t i = 0; i < markers.size(); ++i)
				{
					gCurrentMarkers[i].color = markers[i].color;
					gCurrentMarkers[i].luaHidden = markers[i].luaHidden;
					gCurrentMarkers[i].luaRemoved = markers[i].luaRemoved;
					gCurrentMarkers[i].world = markers[i].world;
					gCurrentMarkers[i].alpha = markers[i].alpha;
					gCurrentMarkers[i].iconSize = markers[i].iconSize;
					gCurrentMarkers[i].heightOffset = markers[i].heightOffset;
					gCurrentMarkers[i].triggerRange = markers[i].triggerRange;
					gCurrentMarkers[i].autoTrigger = markers[i].autoTrigger;
					std::memcpy(gCurrentMarkers[i].tipName, markers[i].tipName,
						sizeof(gCurrentMarkers[i].tipName));
					std::memcpy(gCurrentMarkers[i].tipDescription, markers[i].tipDescription,
						sizeof(gCurrentMarkers[i].tipDescription));
					std::memcpy(gCurrentMarkers[i].iconId, markers[i].iconId,
						sizeof(gCurrentMarkers[i].iconId));
				}
			}
		}
		else
			PathingLuaDetail::gTickTrails = nullptr;
	}
}

namespace PathingDetail
{
bool MarkerBehaviorVisible(const PathingTrails::Marker& m)
{
	if (m.luaHidden || m.luaRemoved)
		return false;
	if (!PathingSchedule::MarkerActive(m.schedule, m.scheduleDuration,
		PathingSchedule::NowUnixUtc()))
		return false;
	uint32_t mapId = 0, shard = 0, inst = 0;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
		{
			mapId = ctx->mapId;
			shard = ctx->shardId;
			inst = ctx->instance;
		}
	}
	return MarkerBehaviors::ShouldDisplay(m, mapId, shard, inst, MumbleIdentity::CharacterName());
}

} // namespace PathingDetail
