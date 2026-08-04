#include "PathingTrails.h"

#include "Globals.h"
#include "MarkerBehaviors.h"
#include "MumbleIdentity.h"
#include "PathingIndex.h"
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

uint64_t PathingTrails::ContentRevision()
{
	return gContentRevision.load(std::memory_order_acquire);
}

bool PathingTrails::HasDrawableWorldGps()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return true; /* unknown — do not wipe sticky cache */
	for (const Trail& t : gCurrentAll)
	{
		/* Match CurrentTrails / compass: either visibility flag is enough. */
		if (t.worldPoints.size() >= 2 && TypeEnabledLocked(t.label) &&
			(t.inGameVisible || t.minimapVisible))
			return true;
	}
	for (const Marker& m : gCurrentMarkers)
	{
		if (!TypeEnabledLocked(m.label))
			continue;
		if (!MarkerShownInWorld(m) && !m.minimapVisible)
			continue;
		return true;
	}
	return false;
}

int PathingTrails::TrailCountAllOnMap()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return 0;
	return static_cast<int>(gCurrentAll.size());
}

int PathingTrails::TrailCount()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return 0;
	int n = 0;
	for (const Trail& t : gCurrentAll)
	{
		if (TypeEnabledLocked(t.label))
			++n;
	}
	return n;
}

std::vector<PathingTrails::Trail> PathingTrails::CurrentTrails()
{
	/* Snapshot: keep last only while the pack worker holds the mutex. */
	static std::vector<Trail> sLast;
	static uint32_t sLastMap = 0;

	std::unique_lock<std::mutex> lock(gMutex, std::defer_lock);
	if (gLoading.load(std::memory_order_acquire))
	{
		if (!lock.try_lock())
			return (sLastMap == gActiveMap) ? sLast : std::vector<Trail>{};
	}
	else
		lock.lock();

	std::vector<Trail> out;
	/* Compass needs continent polylines — never deep-copy worldPoints. */
	constexpr int kMaxDraw = 128;
	out.reserve(static_cast<size_t>(kMaxDraw));
	for (const Trail& t : gCurrentAll)
	{
		if (static_cast<int>(out.size()) >= kMaxDraw)
			break;
		if (!TypeEnabledLocked(t.label) || t.points.size() < 2)
			continue;
		/* Show in-game pathing routes on our compass even when the pack sets
		   miniMapVisibility=0 (common for Lady map-completion / WP Only). */
		if (!t.minimapVisible && !t.inGameVisible)
			continue;
		Trail slim{};
		slim.mapId = t.mapId;
		slim.color = t.color;
		std::snprintf(slim.textureId, sizeof(slim.textureId), "%s", t.textureId);
		slim.minimapVisible = true;
		slim.inGameVisible = t.inGameVisible;
		slim.alpha = t.alpha;
		slim.trailScale = t.trailScale;
		slim.fadeNear = t.fadeNear;
		slim.fadeFar = t.fadeFar;
		std::snprintf(slim.label, sizeof(slim.label), "%s", t.label);
		slim.points = t.points;
		out.push_back(std::move(slim));
	}
	/* Always accept the filtered result — including empty — so toggles turn off. */
	sLast = out;
	sLastMap = gActiveMap;
	return out;
}

int PathingTrails::MarkerCount()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return 0;
	return static_cast<int>(gCurrentMarkers.size());
}

std::vector<PathingTrails::Marker> PathingTrails::CurrentMarkers()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return {};
	return gCurrentMarkers;
}

std::vector<PathingTrails::Marker> PathingTrails::CurrentMarkersInBounds(
	float minX, float minY, float maxX, float maxY)
{
	std::vector<Marker> out;
	if (!(minX <= maxX && minY <= maxY))
		return out;
	static std::vector<Marker> sLast;
	static float sMinX = 0.f, sMinY = 0.f, sMaxX = 0.f, sMaxY = 0.f;
	static uint32_t sLastMap = 0;
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
	{
		if (sLastMap == gActiveMap &&
			std::fabs(minX - sMinX) < 50.f && std::fabs(maxX - sMaxX) < 50.f &&
			std::fabs(minY - sMinY) < 50.f && std::fabs(maxY - sMaxY) < 50.f)
			return sLast;
		return out;
	}
	out.reserve(std::min<size_t>(gCurrentMarkers.size(), kMaxMinimapMarkers));
	for (const Marker& marker : gCurrentMarkers)
	{
		if (!TypeEnabledLocked(marker.label))
			continue;
		if (!MarkerShownOnCompass(marker))
			continue;
		if (!MarkerBehaviorVisible(marker))
			continue;
		if (marker.pos.x < minX || marker.pos.x > maxX ||
			marker.pos.y < minY || marker.pos.y > maxY)
			continue;
		out.push_back(marker);
		if (out.size() >= kMaxMinimapMarkers)
			break;
	}
	sLast = out;
	sLastMap = gActiveMap;
	sMinX = minX; sMinY = minY; sMaxX = maxX; sMaxY = maxY;
	return out;
}

std::vector<PathingTrails::Marker> PathingTrails::NearbyWorldMarkers(
	float x, float y, float z, float maxDistance, size_t maxMarkers)
{
	std::vector<Marker> out;
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
		maxDistance <= 0.f || maxMarkers == 0)
		return out;
	const float maxD2 = maxDistance * maxDistance;
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return out;
	out.reserve(std::min(maxMarkers, gCurrentMarkers.size()));
	for (const Marker& marker : gCurrentMarkers)
	{
		if (!TypeEnabledLocked(marker.label))
			continue;
		if (!MarkerShownInWorld(marker))
			continue;
		if (!MarkerBehaviorVisible(marker))
			continue;
		const float dx = marker.world.x - x;
		const float dy = marker.world.y - y;
		const float dz = marker.world.z - z;
		const float d2 = dx * dx + dy * dy + dz * dz;
		if (!std::isfinite(d2) || d2 > maxD2)
			continue;
		out.push_back(marker);
		if (out.size() >= maxMarkers)
			break;
	}
	return out;
}

void PathingTrails::BeginFrame()
{
	if (!G::API || !G::API->Textures_GetOrCreateFromMemory)
		return;

	/* Drop retain buffers once Nexus has a live Resource. */
	std::vector<std::string> dropIds;
	{
		std::lock_guard<std::mutex> lock(gIconMutex);
		dropIds.reserve(gIconRetain.size());
		for (const auto& kv : gIconRetain)
			dropIds.push_back(kv.first);
	}
	for (const std::string& id : dropIds)
	{
		Texture_t* tex = G::API->Textures_Get
			? G::API->Textures_Get(id.c_str()) : nullptr;
		if (!(tex && tex->Resource))
			continue;
		std::lock_guard<std::mutex> lock(gIconMutex);
		gIconRetain.erase(id);
	}

	for (int n = 0; n < 72; ++n)
	{
		PendingIcon icon;
		{
			std::lock_guard<std::mutex> lock(gIconMutex);
			if (gPendingIcons.empty())
				break;
			icon = std::move(gPendingIcons.front());
			gPendingIcons.erase(gPendingIcons.begin());
		}
		if (icon.bytes.empty() || icon.id.empty())
			continue;
		if (G::API->Textures_Get(icon.id.c_str()) &&
			G::API->Textures_Get(icon.id.c_str())->Resource)
			continue;
		/* Keep PNG bytes alive across async Nexus decode (Wine often deferred).
		   QuickAccess icons work because their buffers are static duration. */
		const uint8_t* data = nullptr;
		uint64_t size = 0;
		{
			std::lock_guard<std::mutex> lock(gIconMutex);
			auto& slot = gIconRetain[icon.id];
			if (slot.empty())
				slot = std::move(icon.bytes);
			data = slot.data();
			size = static_cast<uint64_t>(slot.size());
		}
		if (!data || size == 0)
			continue;
		G::API->Textures_GetOrCreateFromMemory(
			icon.id.c_str(), const_cast<uint8_t*>(data), size);
	}
}

std::vector<PathingTrails::Category> PathingTrails::CategoryTree()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return {};
	if (!gMenu.empty())
	{
		MarkEnabled(gMenu);
		return gMenu;
	}

	std::vector<Category> roots;
	for (const IndexedTrail& it : gIndex)
	{
		if (!it.type.empty())
			InsertCatPath(roots, it.type);
	}
	MarkEnabled(roots);
	return roots;
}

void PathingTrails::SetCategoryEnabled(const std::string& path, bool enabled)
{
	ApplyCategoryShowHide(
		enabled ? std::vector<std::string>{path} : std::vector<std::string>{},
		enabled ? std::vector<std::string>{} : std::vector<std::string>{path});

	/* Categories → Hero Points is the same tree as Features → Hero Point Train.
	   Sync the Features gate so the Categories checkbox actually shows/hides content
	   when parent legs is still enabled. */
	const std::string low = ToLower(path);
	if (low == "legs.hp" || low == "leag.hp" ||
		(low.size() > 8 && low.compare(0, 8, "legs.hp.") == 0) ||
		(low.size() > 8 && low.compare(0, 8, "leag.hp.") == 0))
	{
		if (G::LadyHeroPointTrain != enabled)
		{
			G::LadyHeroPointTrain = enabled;
			Settings::SetDirty();
		}
		gContentRevision.fetch_add(1, std::memory_order_release);
		gForceReload.store(true, std::memory_order_release);
	}
}

void PathingTrails::ApplyCategoryShowHide(
	const std::vector<std::string>& showPaths,
	const std::vector<std::string>& hidePaths)
{
	if (showPaths.empty() && hidePaths.empty())
		return;
	std::lock_guard<std::mutex> lock(gMutex);
	auto stripPath = [&](const std::string& path) {
		const std::string low = ToLower(path);
		gEnabledPaths.erase(
			std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
				[&](const std::string& p) {
					const std::string el = ToLower(p);
					return PrefixMatchesType(el, low) || PrefixMatchesType(low, el);
				}),
			gEnabledPaths.end());
	};
	for (const std::string& path : hidePaths)
	{
		if (!path.empty())
			stripPath(path);
	}
	for (const std::string& path : showPaths)
	{
		if (path.empty())
			continue;
		stripPath(path);
		gEnabledPaths.push_back(path);
	}
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

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
	}
	MarkerBehaviors::Tick(
		ctx->mapId, ctx->shardId, ctx->instance,
		G::Mumble->fAvatarPosition[0],
		G::Mumble->fAvatarPosition[1],
		G::Mumble->fAvatarPosition[2],
		MumbleIdentity::CharacterName(),
		markers);
}

namespace PathingDetail
{
bool MarkerBehaviorVisible(const PathingTrails::Marker& m)
{
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
	std::lock_guard<std::mutex> lock(gMutex);
	gGuideActive = true;
	gGuideDestX = continentX;
	gGuideDestY = continentY;
	/* Drop the previous ribbon so clipboard / Find don't keep showing an old route. */
	gGuide = {};
	RebuildSearchGuideLocked();
	/* Empty geometry → load/reload map trails (search-rank) until a snap succeeds. */
	if (gGuide.points.size() < 2)
		gForceReload.store(true, std::memory_order_release);
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
