#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingLua.h"
#include "PathingParse.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include "miniz/miniz.h"

namespace PathingDetail
{

	void LoadMapTrails(uint32_t mapId, uint32_t epoch)
	{
		std::vector<IndexedTrail> indexCopy;
		std::vector<IndexedPoi> poiCopy;
		std::vector<std::string> enabledCopy;
		std::unordered_map<std::string, MarkerStyle> styleCopy;
		uint32_t enabledGen = 0;
		bool guideActive = false;
		{
			std::lock_guard<std::mutex> lock(gMutex);
			indexCopy = gIndex;
			poiCopy = gPoiIndex;
			enabledCopy = gEnabledPaths;
			styleCopy = gCategoryStyles;
			enabledGen = gEnabledGen.load(std::memory_order_acquire);
			guideActive = gGuideActive;
		}

		/* Nothing opted in and no active search -> skip opening the ~100MB pack.
		   This was a common Wine OOM path when every category defaulted on. */
		const bool needPack = !enabledCopy.empty() || guideActive;
		if (!needPack)
		{
			std::lock_guard<std::mutex> lock(gMutex);
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			gActiveMap = mapId;
			gLoadedEnabledGen = enabledGen;
			gCurrentAll.clear();
			gCurrentMarkers.clear();
			gMapsWithLadyBarefoot.erase(mapId);
			gGuide = {};
			gContentRevision.fetch_add(1, std::memory_order_release);
			return;
		}

		Rects rects{};
		{
			std::lock_guard<std::mutex> lock(gMutex);
			auto it = gRects.find(mapId);
			if (it != gRects.end() && it->second.valid)
				rects = it->second;
		}
		if (!rects.valid)
		{
			if (!FetchMapRects(mapId, rects) || gEpoch.load(std::memory_order_acquire) != epoch)
			{
				/* Stop Update from hammering a failing fetch every frame. */
				std::lock_guard<std::mutex> lock(gMutex);
				if (gEpoch.load(std::memory_order_acquire) == epoch)
				{
					gActiveMap = mapId;
					gLoadedEnabledGen = enabledGen;
				}
				return;
			}
			std::lock_guard<std::mutex> lock(gMutex);
			gRects[mapId] = rects;
		}

		/* Only trails for this map (mapId resolved at index time). Rank:
		   0 = category-enabled (drawn), 1 = map-completion, 2 = other on-map
		   (available for search routing). Group by pack so we open each big
		   zip at most once and hold only one in memory at a time. */
		struct Cand
		{
			const IndexedTrail* it;
			int rank;
		};
		std::vector<Cand> cands;
		cands.reserve(256);
		bool mapHasBarefoot = false;
		for (const IndexedTrail& it : indexCopy)
		{
			if (it.mapId != mapId)
				continue; /* header-resolved; unknown (0) trails are skipped */
			int rank;
			/* Category enable only - Lady Barefoot/WP/Mounts filter at draw time. */
			if (TypeCategoryEnabled(it.type, enabledCopy))
				rank = 0;
			else if (it.mapCompletion)
				rank = 1;
			else
				rank = 2;
			std::string ed;
			if (rank == 0 && LadyMapRouteEdition(ToLower(it.type), ed) && ed == "barefoot")
				mapHasBarefoot = true;
			cands.push_back({&it, rank});
		}

		/* Prefer active Features editions so the per-map trail cap cannot drop
		   WP / Hearts / HP train while Barefoot/Main still fill the budget. */
		auto ladyEdPrio = [](const std::string& type) -> int
		{
			const std::string low = ToLower(type);
			if (G::LadyHeroPointTrain &&
				(low == "legs.hp" || low == "leag.hp" ||
					(low.size() > 8 && (low.compare(0, 8, "legs.hp.") == 0 ||
						low.compare(0, 8, "leag.hp.") == 0))))
				return 0;
			if (G::LadyHearts && low.find("heartpath") != std::string::npos)
				return 0;
			std::string ed;
			if (!LadyMapRouteEdition(low, ed))
				return 50;
			if (G::LadyWpOnly && ed == "wp")
				return 0;
			if (G::LadyBarefoot &&
				(ed == "barefoot" || low.find(".bfs") != std::string::npos))
				return 0;
			if (G::LadyWithMounts && IsLadyWithMountsEdition(ed))
				return 0;
			if (ed == "barefoot")
				return 10;
			if (ed == "wp")
				return 11;
			if (IsLadyWithMountsEdition(ed))
				return 12;
			return 40;
		};
		std::stable_sort(cands.begin(), cands.end(),
			[&](const Cand& a, const Cand& b) {
				if (a.rank != b.rank)
					return a.rank < b.rank;
				const int pa = (a.rank == 0) ? ladyEdPrio(a.it->type) : 50;
				const int pb = (b.rank == 0) ? ladyEdPrio(b.it->type) : 50;
				if (pa != pb)
					return pa < pb;
				return a.it->packPath < b.it->packPath;
			});

		std::vector<PathingTrails::Trail> loaded;
		loaded.reserve(64);
		std::unordered_map<std::string, std::wstring> assetsNeeded;
		std::unordered_set<std::string> seenTrailFiles;
		size_t otherCount = 0;

		OpenPack pack;
		std::wstring openPath;

		for (const Cand& c : cands)
		{
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			if (loaded.size() >= kMaxTrailsPerMap)
				break;
			/* Cap non-enabled trails kept only for search routing. */
			if (c.rank != 0 && otherCount >= 80)
				continue;

			const IndexedTrail& it = *c.it;
			/* Same .trl from two packs (duplicate Tekkit AIO copies) -> skip. */
			const std::string trailKey = ToLower(it.entryName) + "#" +
				std::to_string(static_cast<unsigned>(mapId));
			if (!seenTrailFiles.insert(trailKey).second)
				continue;
			if (it.packPath != openPath)
			{
				pack.Close();
				openPath = it.packPath;
				if (!pack.Open(openPath))
					continue;
			}

			std::vector<uint8_t> bytes;
			const int fi = (it.fileIndex >= 0) ? it.fileIndex
				: ZipLocate(pack.zip, it.entryName);
			if (!ZipExtractIndex(pack.zip, fi, bytes, kMaxTrailFile))
				continue;

			uint32_t trailMap = 0;
			std::vector<PathingTrails::WorldPoint> world;
			if (!ParseTrl(bytes, trailMap, world) || trailMap != mapId)
				continue;

			PathingTrails::Trail trail{};
			const MarkerStyle style = ResolveStyle(it.type, it.style, styleCopy);
			trail.mapId = mapId;
			trail.color = style.hasColor ? style.color : it.color;
			trail.minimapVisible = style.minimapVisible;
			trail.inGameVisible = style.inGameVisible;
			/* Blish: mapVisibility is the fullscreen map only - never copy it into
			   inGameVisible (that hid world GPS while the compass still drew). */
			{
				const std::string typeLow = ToLower(it.type);
				if (typeLow.compare(0, 9, "legs.map.") == 0 ||
					typeLow.compare(0, 9, "leag.map.") == 0)
				{
					/* Lady map routes: always drawable on our compass + world GPS. */
					if (!style.hasInGameVisible)
						trail.inGameVisible = true;
					trail.minimapVisible = true;
				}
			}
			trail.alpha = std::clamp(style.alpha, 0.f, 1.f);
			trail.trailScale = std::clamp(style.trailScale, 0.1f, 8.f);
			trail.fadeNear = style.fadeNear;
			trail.fadeFar = style.fadeFar;
			if (style.hasSchedule)
				std::snprintf(trail.schedule, sizeof(trail.schedule), "%s",
					style.schedule.c_str());
			if (style.hasScheduleDuration)
				trail.scheduleDuration = style.scheduleDuration;
			if (style.hasTexture && !style.texture.empty())
			{
				const std::string tid = IconTextureId(style.texture);
				std::snprintf(trail.textureId, sizeof(trail.textureId), "%s", tid.c_str());
				/* Always queue trail textures (any rank) - missing -> solid yellow ribbon. */
				assetsNeeded.emplace(style.texture, it.packPath);
			}
			std::snprintf(trail.label, sizeof(trail.label), "%s",
				it.type.empty() ? "trail" : it.type.c_str());
			trail.points.reserve(world.size());
			trail.worldPoints.reserve(world.size());
			for (const PathingTrails::WorldPoint& w : world)
			{
				if (!std::isfinite(w.x) || !std::isfinite(w.y) || !std::isfinite(w.z))
				{
					/* Preserve TacO section break so compass/world don't stitch segments. */
					if (!trail.points.empty() && std::isfinite(trail.points.back().x))
					{
						trail.points.push_back({NAN, NAN});
						trail.worldPoints.push_back({NAN, NAN, NAN});
					}
					continue;
				}
				PathingTrails::Point cc{};
				WorldToContinent(rects, w.x, w.z, cc.x, cc.y);
				if (!std::isfinite(cc.x) || !std::isfinite(cc.y))
					continue;
				trail.points.push_back(cc);
				trail.worldPoints.push_back(w);
			}
			if (trail.points.size() < 2)
				continue;
			if (c.rank != 0)
				++otherCount;
			loaded.push_back(std::move(trail));
		}

		/* POI markers for this map - same TacO prefix enable rules as trails.
		   Prefer Barefoot Shortcuts / Mounts icons so the per-map cap cannot
		   drop them behind heart/festival POI spam. */
		std::vector<const IndexedPoi*> poiCands;
		poiCands.reserve(512);
		for (const IndexedPoi& poi : poiCopy)
		{
			if (poi.mapId != mapId)
				continue;
			if (!TypeEnabledWithEnabled(poi.type, enabledCopy))
				continue;
			poiCands.push_back(&poi);
		}
		std::stable_sort(poiCands.begin(), poiCands.end(),
			[](const IndexedPoi* a, const IndexedPoi* b) {
				auto prio = [](const IndexedPoi& p) -> int {
					const std::string low = ToLower(p.type);
					if (low.find(".bfs.") != std::string::npos)
						return 0;
					if (TypeHasLadyMountShortcut(low))
						return 1;
					return 2;
				};
				return prio(*a) < prio(*b);
			});

		std::vector<PathingTrails::Marker> markers;
		markers.reserve(std::min(poiCands.size(), kMaxMarkersPerMap));
		for (const IndexedPoi* poiPtr : poiCands)
		{
			const IndexedPoi& poi = *poiPtr;
			const MarkerStyle style = ResolveStyle(poi.type, poi.style, styleCopy);
			PathingTrails::Point cc{};
			WorldToContinent(rects, poi.wx, poi.wz, cc.x, cc.y);
			if (!std::isfinite(cc.x) || !std::isfinite(cc.y))
				continue;
			PathingTrails::Marker m{};
			m.mapId = mapId;
			m.color = style.hasColor ? style.color : 0xFFFFCC33u;
			m.pos = cc;
			m.world = {poi.wx, poi.wy, poi.wz};
			m.minimapVisible = style.minimapVisible;
			m.inGameVisible = style.inGameVisible;
			/* Blish separates mapVisibility (fullscreen map) from inGameVisibility.
			   Do NOT copy mapVisibility=0 onto in-world - Lady Mounts categories set
			   mapVisibility/miniMapVisibility to 0 but still draw in the world. */
			{
				const std::string typeLow = ToLower(poi.type);
				if (typeLow.compare(0, 9, "legs.map.") == 0 ||
					typeLow.compare(0, 9, "leag.map.") == 0)
				{
					/* Same as trails - Lady map POIs stay drawable in-world even when
					   the pack zeroes map/minimap visibility on Mounts categories. */
					if (!style.hasInGameVisible)
						m.inGameVisible = true;
					if (m.inGameVisible)
						m.minimapVisible = true;
				}
			}
			m.mapDisplaySize = std::max(1.f, style.mapDisplaySize);
			m.minSize = std::max(1.f, style.minSize);
			m.maxSize = std::max(m.minSize, style.maxSize);
			m.iconSize = std::max(0.05f, style.iconSize);
			m.heightOffset = style.heightOffset;
			m.fadeNear = style.fadeNear;
			m.fadeFar = style.fadeFar;
			m.alpha = std::clamp(style.alpha, 0.f, 1.f);
			std::snprintf(m.label, sizeof(m.label), "%s", poi.type.c_str());
			if (!poi.guid.empty())
				std::snprintf(m.guid, sizeof(m.guid), "%s", poi.guid.c_str());
			m.behavior = style.behavior;
			m.autoTrigger = style.autoTrigger;
			m.triggerRange = style.hasTriggerRange ? style.triggerRange : 2.f;
			m.resetLength = style.resetLength;
			m.invertBehavior = style.invertBehavior;
			if (style.hasHide)
				std::snprintf(m.hide, sizeof(m.hide), "%s", style.hide.c_str());
			if (style.hasShow)
				std::snprintf(m.show, sizeof(m.show), "%s", style.show.c_str());
			if (style.hasTipName)
				std::snprintf(m.tipName, sizeof(m.tipName), "%s", style.tipName.c_str());
			if (style.hasTipDescription)
				std::snprintf(m.tipDescription, sizeof(m.tipDescription), "%s",
					style.tipDescription.c_str());
			if (style.hasInfo)
				std::snprintf(m.info, sizeof(m.info), "%s", style.info.c_str());
			if (style.hasCopy)
				std::snprintf(m.copy, sizeof(m.copy), "%s", style.copy.c_str());
			if (style.hasCopyMessage)
				std::snprintf(m.copyMessage, sizeof(m.copyMessage), "%s",
					style.copyMessage.c_str());
			if (style.hasSchedule)
				std::snprintf(m.schedule, sizeof(m.schedule), "%s", style.schedule.c_str());
			if (style.hasScheduleDuration)
				m.scheduleDuration = style.scheduleDuration;
			if (style.hasScriptOnce)
				m.scriptOnce = style.scriptOnce;
			if (style.hasScriptTrigger)
				m.scriptTrigger = style.scriptTrigger;
			if (style.hasScriptFilter)
				m.scriptFilter = style.scriptFilter;
			if (style.hasScriptTick)
				m.scriptTick = style.scriptTick;
			if (style.hasScriptFocus)
				m.scriptFocus = style.scriptFocus;
			const std::string& icon = style.iconFile;
			if (!icon.empty())
			{
				const std::string tid = IconTextureId(icon);
				std::snprintf(m.iconId, sizeof(m.iconId), "%s", tid.c_str());
				assetsNeeded.emplace(icon, poi.packPath);
				const std::string iconLow = ToLower(icon);
				/* Same rules for Numbers and Mounts PNGs - Lady often sets
				   mapVisibility=0; that is the fullscreen map, not in-world/compass. */
				if (iconLow.find("images/mounts/") != std::string::npos ||
					iconLow.find("images/numbers/") != std::string::npos)
				{
					m.minimapVisible = true;
					m.inGameVisible = true;
					m.mapDisplaySize = std::max(m.mapDisplaySize, 24.f);
					m.minSize = std::max(m.minSize, 16.f);
					m.maxSize = std::max(m.maxSize, 48.f);
					m.iconSize = std::max(m.iconSize, 1.f);
					if (!m.tipName[0] &&
						iconLow.find("images/mounts/") != std::string::npos)
					{
						std::string leaf = icon;
						const size_t slash = leaf.find_last_of("/\\");
						if (slash != std::string::npos)
							leaf = leaf.substr(slash + 1);
						const size_t dot = leaf.find_last_of('.');
						if (dot != std::string::npos)
							leaf = leaf.substr(0, dot);
						if (leaf.rfind("Mount_", 0) == 0)
							leaf = leaf.substr(6);
						for (char& ch : leaf)
						{
							if (ch == '_' || ch == '-')
								ch = ' ';
						}
						std::snprintf(m.tipName, sizeof(m.tipName), "%s", leaf.c_str());
					}
				}
			}
			markers.push_back(std::move(m));
			if (markers.size() >= kMaxMarkersPerMap)
				break;
		}

		/* Free trail zip before icon pass - Wine cannot hold two 45MB packs. */
		pack.Close();
		openPath.clear();

		std::wstring mountPack;
		for (const Cand& c : cands)
		{
			if (c.rank == 0 && c.it && !c.it->packPath.empty())
			{
				mountPack = c.it->packPath;
				break;
			}
		}
		for (const IndexedPoi* poiPtr : poiCands)
		{
			if (poiPtr && !poiPtr->packPath.empty())
			{
				if (mountPack.empty())
					mountPack = poiPtr->packPath;
				break;
			}
		}
		QueueMapIcons(assetsNeeded, mountPack, epoch);

		std::lock_guard<std::mutex> lock(gMutex);
		if (gEpoch.load(std::memory_order_acquire) != epoch)
			return;
		gActiveMap = mapId;
		/* Always settle the gen we intended - perpetual mismatch was reloading
		   the pack every ~1s and blanking GPS. Another toggle bumps gen again. */
		gLoadedEnabledGen = gEnabledGen.load(std::memory_order_acquire);
		gCurrentAll = std::move(loaded);
		gCurrentMarkers = std::move(markers);
		PathingLua::OnMarkersLoaded(gCurrentMarkers);
		if (mapHasBarefoot)
			gMapsWithLadyBarefoot.insert(mapId);
		else
			gMapsWithLadyBarefoot.erase(mapId);
		gContentRevision.fetch_add(1, std::memory_order_release);
		RebuildSearchGuideLocked();
	}


} // namespace PathingDetail
