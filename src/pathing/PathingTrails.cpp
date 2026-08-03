#include "PathingTrails.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "MarkerBehaviors.h"
#include "MumbleIdentity.h"
#include "PathingPacks.h"
#include "PathingIndex.h"
#include "PathingParse.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace PathingDetail
{

	std::mutex gMutex;
	std::atomic<uint32_t> gEpoch{0};
	std::atomic<uint32_t> gLoadGen{0}; /* increments each spawn; clears stuck loading */
	std::atomic<bool> gLoading{false};
	std::atomic<bool> gForceReload{false};
	std::atomic<bool> gIndexStarted{false};
	/* Bumped whenever gEnabledPaths changes. LoadMapTrails records the gen it
	   applied; Update retries until they match — a bool force-flag alone could
	   be cleared before a failed load, leaving trails empty until Reload packs. */
	std::atomic<uint32_t> gEnabledGen{1};
	uint32_t gLoadedEnabledGen = 0; /* under gMutex; 0 = never applied */
	std::atomic<int> gPackCount{0};
	std::vector<std::string> gPackNames; /* loaded .taco basenames (under gMutex) */
	std::thread gWorker;

	std::atomic<HINTERNET> gLiveSession{nullptr};
	std::atomic<HINTERNET> gLiveRequest{nullptr};




	std::vector<IndexedTrail> gIndex;
	std::vector<IndexedPoi> gPoiIndex;
	std::unordered_map<std::string, MarkerStyle> gCategoryStyles;
	std::vector<PathingTrails::Category> gMenu; /* Tekkit overlay menu order */
	std::atomic<uint64_t> gMenuRevision{1};
	std::atomic<uint64_t> gContentRevision{1};
	uint32_t gActiveMap = 0;
	std::vector<PathingTrails::Trail> gCurrentAll; /* all trails for map */
	std::vector<PathingTrails::Marker> gCurrentMarkers;
	/* Maps that have Lady barefoot edition trails (for Mounts-off fallback). */
	std::unordered_set<uint32_t> gMapsWithLadyBarefoot;
	/* Opt-in: empty = nothing draws. Enabling a path shows that category and
	   all descendants (TacO-style prefix). */ 
	std::vector<std::string> gEnabledPaths;

	std::mutex gIconMutex;
	std::vector<PendingIcon> gPendingIcons;
	std::unordered_map<std::string, bool> gIconQueued; /* iconFile → queued */
	std::unordered_map<std::string, std::vector<uint8_t>> gIconRetain;
	bool gGuideActive = false;
	float gGuideDestX = 0.f;
	float gGuideDestY = 0.f;
	PathingTrails::Trail gGuide{};
	float gGuidePlayerX = 0.f;
	float gGuidePlayerY = 0.f;
	bool  gGuideHavePlayer = false;

	std::unordered_map<uint32_t, bool> gMapRectsReady;
	std::unordered_map<uint32_t, Rects> gRects;

} // namespace PathingDetail

using namespace PathingDetail;

void PathingTrails::Init()
{
	MarkerBehaviors::Init();
	gEpoch.fetch_add(1, std::memory_order_acq_rel);
	gLoadGen.fetch_add(1, std::memory_order_acq_rel);
	AbortHttp();
	if (gWorker.joinable())
		gWorker.detach();
	gIndexStarted.store(false, std::memory_order_release);
	gLoading.store(false, std::memory_order_release);
	gForceReload.store(false, std::memory_order_release);
	gEnabledGen.store(1, std::memory_order_release);
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gIndex.clear();
		gPoiIndex.clear();
		gCategoryStyles.clear();
		gMenu.clear();
		gCurrentAll.clear();
		gCurrentMarkers.clear();
		gGuide = {};
		gGuideActive = false;
		gActiveMap = 0;
		gLoadedEnabledGen = 0;
		/* Keep gEnabledPaths — Blish/TacO remember category toggles across reloads. */
		gPackNames.clear();
	}
	{
		std::lock_guard<std::mutex> lock(gIconMutex);
		gPendingIcons.clear();
		gIconQueued.clear();
		gIconRetain.clear();
	}
	gPackCount.store(0, std::memory_order_release);
}

void PathingTrails::Shutdown()
{
	MarkerBehaviors::Shutdown();
	gEpoch.fetch_add(1, std::memory_order_acq_rel);
	gLoadGen.fetch_add(1, std::memory_order_acq_rel);
	PathingPacks::RequestCancel();
	AbortHttp();
	/* Join when possible so Nexus Disable can FreeLibrary safely. Detach only
	   if join throws (should be rare after cancel + epoch bump). */
	if (gWorker.joinable())
	{
		try
		{
			gWorker.join();
		}
		catch (...)
		{
			if (gWorker.joinable())
				gWorker.detach();
		}
	}
	{
		std::lock_guard<std::mutex> lock(gIconMutex);
		gPendingIcons.clear();
		gIconQueued.clear();
		gIconRetain.clear();
	}
	std::lock_guard<std::mutex> lock(gMutex);
	gIndex.clear();
	gPoiIndex.clear();
	gCategoryStyles.clear();
	gMenu.clear();
	gCurrentAll.clear();
	gCurrentMarkers.clear();
	gGuide = {};
	gGuideActive = false;
	gIndexStarted.store(false, std::memory_order_release);
	gLoading.store(false, std::memory_order_release);
}

void PathingTrails::Update(uint32_t mapId)
{
	/* Index / curated downloads can run with overlays off (Pathing panel open).
	   Compass / world callers only invoke Update when overlays are enabled. */
	if (mapId == 0)
		return;

	/* Keep search routing locked to the live player continent position, but only
	   rebuild when the player has actually moved a bit — rebuilding scans every
	   trail on the map, so doing it every frame would stutter. */
	if (G::Mumble)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx && ctx->mapId != 0)
		{
			std::lock_guard<std::mutex> lock(gMutex);
			const float dx = ctx->playerX - gGuidePlayerX;
			const float dy = ctx->playerY - gGuidePlayerY;
			/* Larger hysteresis — rebuilds used to flip the orange guide on/off. */
			const bool moved = (dx * dx + dy * dy) > (280.f * 280.f);
			gGuidePlayerX = ctx->playerX;
			gGuidePlayerY = ctx->playerY;
			const bool first = !gGuideHavePlayer;
			gGuideHavePlayer = true;
			if (gGuideActive && (moved || first))
				RebuildSearchGuideLocked();
		}
	}

	if (!gIndexStarted.load(std::memory_order_acquire))
	{
		gIndexStarted.store(true, std::memory_order_release);
		const uint32_t epoch = gEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
		const uint32_t loadGen = gLoadGen.fetch_add(1, std::memory_order_acq_rel) + 1;
		gLoading.store(true, std::memory_order_release);
		if (gWorker.joinable())
			gWorker.detach();
		gWorker = std::thread([epoch, loadGen, mapId]() {
			struct Guard
			{
				uint32_t loadGen = 0;
				~Guard()
				{
					if (gLoadGen.load(std::memory_order_acquire) == loadGen)
						gLoading.store(false, std::memory_order_release);
				}
			} guard{loadGen};
			WorkerLoop(epoch, mapId);
		});
		return;
	}

	if (gLoading.load(std::memory_order_acquire))
		return;

	uint32_t active = 0;
	uint32_t loadedGen = 0;
	{
		std::lock_guard<std::mutex> lock(gMutex);
		active = gActiveMap;
		loadedGen = gLoadedEnabledGen;
	}
	const uint32_t wantGen = gEnabledGen.load(std::memory_order_acquire);
	const bool force = gForceReload.exchange(false, std::memory_order_acq_rel);

	if (mapId == active && wantGen == loadedGen && !force)
		return;

	/* Invalidate any still-running detached LoadMapTrails before spawning. */
	const uint32_t epoch = gEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
	const uint32_t loadGen = gLoadGen.fetch_add(1, std::memory_order_acq_rel) + 1;
	gLoading.store(true, std::memory_order_release);
	if (gWorker.joinable())
		gWorker.detach();
	gWorker = std::thread([epoch, loadGen, mapId]() {
		struct Guard
		{
			uint32_t loadGen = 0;
			~Guard()
			{
				if (gLoadGen.load(std::memory_order_acquire) == loadGen)
					gLoading.store(false, std::memory_order_release);
			}
		} guard{loadGen};
		try
		{
			LoadMapTrails(mapId, epoch);
		}
		catch (...)
		{
		}
	});
}

bool PathingTrails::MasterEnabled() { return G::ShowPathingTrails; }
void PathingTrails::SetMasterEnabled(bool on) { G::ShowPathingTrails = on; }

bool PathingTrails::IsLoading() { return gLoading.load(std::memory_order_acquire); }
int PathingTrails::PackCount() { return gPackCount.load(std::memory_order_acquire); }

std::vector<std::string> PathingTrails::LoadedPackNames()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gPackNames;
}

std::string PathingTrails::PathingFolderHint()
{
	if (G::API && G::API->Paths_GetAddonDirectory)
	{
		const char* ad = G::API->Paths_GetAddonDirectory(ADDON_NAME);
		if (ad && ad[0])
		{
			std::string p(ad);
			while (!p.empty() && (p.back() == '\\' || p.back() == '/'))
				p.pop_back();
			return p + "\\pathing";
		}
	}
	return "addons\\GW2-InGame-Helper\\pathing";
}

void PathingTrails::ReloadPacks()
{
	/* Invalidate in-flight work and ask Update() to re-index. Do NOT clear
	   multi-MB trail/index vectors here — that ran on the UI thread and froze
	   the game under Wine. The worker clears and rebuilds. */
	gEpoch.fetch_add(1, std::memory_order_acq_rel);
	gLoadGen.fetch_add(1, std::memory_order_acq_rel);
	AbortHttp();
	if (gWorker.joinable())
		gWorker.detach();
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gActiveMap = 0;
		gLoadedEnabledGen = 0;
	}
	gLoading.store(false, std::memory_order_release);
	gForceReload.store(false, std::memory_order_release);
	gIndexStarted.store(false, std::memory_order_release);
}

void PathingTrails::UpdateCuratedPacks()
{
	PathingPacks::RequestForceUpdate();
	ReloadPacks();
}

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
		if (t.worldPoints.size() >= 2 && t.inGameVisible && TypeEnabledLocked(t.label))
			return true;
	}
	for (const Marker& m : gCurrentMarkers)
	{
		if (!TypeEnabledLocked(m.label))
			continue;
		if (!MarkerShownInWorld(m))
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

	for (int n = 0; n < 48; ++n)
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
