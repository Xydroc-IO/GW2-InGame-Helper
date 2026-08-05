#include "PathingTrails.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "MarkerBehaviors.h"
#include "PathingLua.h"
#include "PathingPacks.h"
#include "PathingIndex.h"

#include <atomic>
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
	PathingLua::Init();
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
	PathingLua::Shutdown();
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
