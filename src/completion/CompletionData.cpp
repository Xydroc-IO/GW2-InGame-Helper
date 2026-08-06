#include "CompletionShared.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "UiAscii.h"
#include "WaypointsData.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace CompletionDetail
{
	bool gFocus = false;
	bool gPlaceOnce = false;
	int gTab = 0;
	bool gTabSelectOnce = false;
	RouteMode gRouteMode = RouteMode::Nearest;
	char gAtlasFilter[96]{};
	char gStatus[192]{};
	uint32_t gFocusMapId = 0;
	int gFocusObjective = -1;
	bool gAutoArrive = true;
	bool gShowGpsArrow = true;
	float gArriveRadius = 120.f;

	namespace
	{
		std::recursive_mutex gMu;
		std::vector<MapInfo> gMaps;
		std::vector<Objective> gObjs;
		std::unordered_set<uint32_t> gDoneIds;
		bool gReady = false;
		bool gChecklistLoaded = false;
		uint32_t gLiveSyncedMap = 0;

		std::wstring ChecklistPath()
		{
			return AddonPaths::ConfigDir() + L"\\completion-checklist.txt";
		}

		ObjKind KindFromApiType(const std::string& type)
		{
			if (type == "waypoint") return ObjKind::Waypoint;
			if (type == "vista") return ObjKind::Vista;
			if (type == "unlock") return ObjKind::Mastery;
			return ObjKind::Poi;
		}

		void UpsertMap(uint32_t id, const char* name, const char* region)
		{
			char safeName[96]{};
			char safeRegion[64]{};
			if (name && name[0])
				UiAscii::SanitizeForUi(safeName, sizeof(safeName), name);
			if (region && region[0])
				UiAscii::SanitizeForUi(safeRegion, sizeof(safeRegion), region);
			for (MapInfo& m : gMaps)
			{
				if (m.id == id)
				{
					if (safeName[0] && !m.name[0])
						std::snprintf(m.name, sizeof(m.name), "%s", safeName);
					if (safeRegion[0] && !m.region[0])
						std::snprintf(m.region, sizeof(m.region), "%s", safeRegion);
					ApplyHierarchy(m);
					return;
				}
			}
			MapInfo m{};
			m.id = id;
			if (safeName[0]) std::snprintf(m.name, sizeof(m.name), "%s", safeName);
			if (safeRegion[0]) std::snprintf(m.region, sizeof(m.region), "%s", safeRegion);
			ApplyHierarchy(m);
			gMaps.push_back(m);
		}

		void AddObj(uint32_t id, uint32_t mapId, ObjKind kind, const char* name,
			float cx, float cy, bool hasCoord)
		{
			for (Objective& o : gObjs)
			{
				if (o.id == id)
				{
					o.done = gDoneIds.count(id) != 0;
					return;
				}
			}
			Objective o{};
			o.id = id;
			o.mapId = mapId;
			o.kind = kind;
			if (name && name[0])
				UiAscii::SanitizeForUi(o.name, sizeof(o.name), name);
			o.continentX = cx;
			o.continentY = cy;
			o.hasCoord = hasCoord;
			o.done = gDoneIds.count(id) != 0;
			gObjs.push_back(o);
		}

		void SeedCurated()
		{
			/* Official map ids (Public). Hierarchy fills release/region. */
			UpsertMap(15, "Queensdale", "Kryta");
			UpsertMap(34, "Caledon Forest", "Maguuma Jungle");
			UpsertMap(50, "Lion's Arch", "Kryta");
			UpsertMap(28, "Wayfarer Foothills", "Shiverpeak Mountains");
			UpsertMap(1015, "The Silverwastes", "Maguuma Wastes");
			AddObj(900001, 15, ObjKind::Heart, "Help Farmer Diah", 0, 0, false);
			AddObj(900002, 15, ObjKind::Hero, "Shaemoor Garrison HP", 0, 0, false);
			AddObj(900003, 34, ObjKind::Heart, "Help the Soundless", 0, 0, false);
			AddObj(900004, 28, ObjKind::Hero, "Hangrammr Climb", 0, 0, false);
			AddObj(900005, 15, ObjKind::Mastery, "Gliding Mastery Insight", 0, 0, false);

			/* Seed every curated Public zone so Atlas / hierarchy are complete offline. */
			VisitHierarchy([](uint32_t mapId, const char* /*release*/, const char* region,
				const char* name, void* /*ctx*/) {
					UpsertMap(mapId, name, region);
				}, nullptr);
		}

		void MergeLivePois(uint32_t mapId)
		{
			WaypointsData::EnsureLoaded(false);
			if (!WaypointsData::Ready())
				return;
			std::vector<WaypointsData::Poi> pois;
			WaypointsData::ListForMap(static_cast<int>(mapId), false, pois);
			for (const auto& p : pois)
			{
				if (p.id <= 0) continue;
				const ObjKind k = KindFromApiType(p.type);
				AddObj(static_cast<uint32_t>(p.id), static_cast<uint32_t>(p.mapId), k,
					p.name.c_str(), p.continentX, p.continentY, p.hasCoord);
			}
			std::vector<WaypointsData::MapRow> maps;
			WaypointsData::ListMaps(nullptr, maps, 400);
			for (const auto& mr : maps)
			{
				if (mr.id <= 0) continue;
				UpsertMap(static_cast<uint32_t>(mr.id), mr.name.c_str(), "");
			}
			std::sort(gMaps.begin(), gMaps.end(),
				[](const MapInfo& a, const MapInfo& b) {
					return std::strcmp(a.name, b.name) < 0;
				});
		}
	}

	void EnsureCatalog()
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		if (!gReady)
		{
			gMaps.clear();
			gObjs.clear();
			SeedCurated();
			gReady = true;
		}
		const uint32_t cur = static_cast<uint32_t>(WaypointsData::CurrentMapId());
		if (cur != 0 && cur != gLiveSyncedMap)
		{
			MergeLivePois(cur);
			gLiveSyncedMap = cur;
			if (gFocusMapId == 0)
				gFocusMapId = cur;
		}
		else if (gFocusMapId != 0 && gFocusMapId != gLiveSyncedMap)
		{
			MergeLivePois(gFocusMapId);
			gLiveSyncedMap = gFocusMapId;
		}
	}

	size_t MapCount()
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		return gMaps.size();
	}

	const MapInfo* MapAt(size_t i)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		return i < gMaps.size() ? &gMaps[i] : nullptr;
	}

	const MapInfo* FindMap(uint32_t mapId)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		for (const MapInfo& m : gMaps)
			if (m.id == mapId) return &m;
		return nullptr;
	}

	size_t ObjectiveCount()
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		return gObjs.size();
	}

	Objective* ObjectiveAt(size_t i)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		return i < gObjs.size() ? &gObjs[i] : nullptr;
	}

	void ObjectivesForMap(uint32_t mapId, std::vector<size_t>& out)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		out.clear();
		for (size_t i = 0; i < gObjs.size(); ++i)
			if (gObjs[i].mapId == mapId)
				out.push_back(i);
	}

	void ToggleDone(size_t idx)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		if (idx >= gObjs.size()) return;
		Objective& o = gObjs[idx];
		o.done = !o.done;
		if (o.done) gDoneIds.insert(o.id);
		else gDoneIds.erase(o.id);
		SaveChecklist();
	}

	void ClearDoneForMap(uint32_t mapId)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		for (Objective& o : gObjs)
		{
			if (o.mapId != mapId) continue;
			o.done = false;
			gDoneIds.erase(o.id);
		}
		SaveChecklist();
		std::snprintf(gStatus, sizeof(gStatus), "Cleared ticks for this map.");
	}

	int CountDone(uint32_t mapId)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		int n = 0;
		for (const Objective& o : gObjs)
			if (o.mapId == mapId && o.done) ++n;
		return n;
	}

	int CountTotal(uint32_t mapId)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		int n = 0;
		for (const Objective& o : gObjs)
			if (o.mapId == mapId) ++n;
		return n;
	}

	int CountDoneKind(uint32_t mapId, ObjKind k)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		int n = 0;
		for (const Objective& o : gObjs)
			if (o.mapId == mapId && o.kind == k && o.done) ++n;
		return n;
	}

	int CountTotalKind(uint32_t mapId, ObjKind k)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		int n = 0;
		for (const Objective& o : gObjs)
			if (o.mapId == mapId && o.kind == k) ++n;
		return n;
	}

	void SetFocusMap(uint32_t mapId)
	{
		gFocusMapId = mapId;
		gFocusObjective = -1;
		EnsureCatalog();
	}

	bool GuideToObjective(size_t idx)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		if (idx >= gObjs.size()) return false;
		const Objective& o = gObjs[idx];
		if (!o.hasCoord)
		{
			std::snprintf(gStatus, sizeof(gStatus), "No coordinates for \"%s\".", o.name);
			return false;
		}
		gFocusObjective = static_cast<int>(idx);
		/* Orange search guide only - direct line or nearby pack snap.
		   Never enables pathing categories / map-completion packs. */
		PathingTrails::SetSearchDestination(o.continentX, o.continentY);
		if (PathingTrails::HasSearchGuide())
			std::snprintf(gStatus, sizeof(gStatus), "GPS trail -> %s", o.name);
		else if (PathingTrails::HasSearchGuideActive())
			std::snprintf(gStatus, sizeof(gStatus),
				"GPS -> %s (waiting for position...)", o.name);
		else
			std::snprintf(gStatus, sizeof(gStatus), "GPS -> %s", o.name);
		Settings::SetDirty();
		return true;
	}

	bool GuideNearestRemaining()
	{
		EnsureCatalog();
		std::lock_guard<std::recursive_mutex> lock(gMu);
		float px = 0.f, py = 0.f;
		bool havePlayer = false;
		if (G::Mumble && G::Mumble->context_len >= sizeof(MumbleContext))
		{
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			px = ctx->playerX;
			py = ctx->playerY;
			havePlayer = true;
		}
		int best = -1;
		float bestD = 1.0e30f;
		for (size_t i = 0; i < gObjs.size(); ++i)
		{
			const Objective& o = gObjs[i];
			if (o.done || !o.hasCoord) continue;
			if (gFocusMapId != 0 && o.mapId != gFocusMapId) continue;
			float d = 0.f;
			if (havePlayer)
			{
				const float dx = o.continentX - px;
				const float dy = o.continentY - py;
				d = dx * dx + dy * dy;
			}
			else
				d = static_cast<float>(i);
			if (d < bestD) { bestD = d; best = static_cast<int>(i); }
		}
		if (best < 0)
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"No remaining guided objectives on this map (tick or wait for waypoint index).");
			return false;
		}
		return GuideToObjective(static_cast<size_t>(best));
	}

	bool GuideZoneLoopNext()
	{
		EnsureCatalog();
		/* Catalog order on this map only - never fall back to Nearest. */
		if (gFocusMapId == 0)
		{
			const int cur = WaypointsData::CurrentMapId();
			if (cur > 0)
				SetFocusMap(static_cast<uint32_t>(cur));
		}
		if (gFocusMapId == 0)
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Zone loop needs a focus map (open Checklist or enter a zone).");
			return false;
		}
		std::vector<size_t> idxs;
		ObjectivesForMap(gFocusMapId, idxs);
		if (idxs.empty())
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Zone loop - no objectives catalogued for this map.");
			return false;
		}
		size_t start = 0;
		if (gFocusObjective >= 0)
		{
			for (size_t i = 0; i < idxs.size(); ++i)
				if (static_cast<int>(idxs[i]) == gFocusObjective)
				{ start = i + 1; break; }
		}
		for (size_t n = 0; n < idxs.size(); ++n)
		{
			const size_t i = idxs[(start + n) % idxs.size()];
			Objective* o = ObjectiveAt(i);
			if (o && !o->done && o->hasCoord)
				return GuideToObjective(i);
		}
		std::snprintf(gStatus, sizeof(gStatus), "Zone loop complete - no remaining coords.");
		return false;
	}

	bool OpenLadyMapCompletionPathing()
	{
		/* Lady Elyssa Map Completion only - not Tekkit MC presets / whole pack. */
		G::LadyBarefoot = true;
		G::LadyWithMounts = false;
		G::LadyWpOnly = false;
		G::LadyHearts = true;
		G::LadyHeroPointTrain = false;
		G::ShowPathingTrails = true;
		G::ShowWorldTrails = true;
		PathingTrails::EnableLadyMapCompletionCategories();
		PathingTrails::NotifyVisibilityFilterChanged();
		PathingGuidesPad::Open();
		std::snprintf(gStatus, sizeof(gStatus),
			"Lady Elyssa MC (Barefoot) + hearts - Pathing Features to tune.");
		Settings::SetDirty();
		return true;
	}

	void LoadChecklist()
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		if (gChecklistLoaded) return;
		gDoneIds.clear();
		const std::wstring path = ChecklistPath();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER sz{};
			if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < 2 * 1024 * 1024)
			{
				std::string raw(static_cast<size_t>(sz.QuadPart), '\0');
				DWORD rd = 0;
				if (ReadFile(h, raw.data(), static_cast<DWORD>(raw.size()), &rd, nullptr))
				{
					raw.resize(rd);
					size_t i = 0;
					while (i < raw.size())
					{
						while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\r' || raw[i] == '\n'))
							++i;
						size_t s = i;
						while (i < raw.size() && raw[i] != '\r' && raw[i] != '\n')
							++i;
						if (s < i)
						{
							const uint32_t id = static_cast<uint32_t>(
								std::strtoul(raw.c_str() + s, nullptr, 10));
							if (id) gDoneIds.insert(id);
						}
					}
				}
			}
			CloseHandle(h);
		}
		for (Objective& o : gObjs)
			o.done = gDoneIds.count(o.id) != 0;
		gChecklistLoaded = true;
	}

	void SaveChecklist()
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		const std::wstring path = ChecklistPath();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return;
		for (uint32_t id : gDoneIds)
		{
			char line[32];
			const int n = std::snprintf(line, sizeof(line), "%u\n", id);
			DWORD w = 0;
			if (n > 0) WriteFile(h, line, static_cast<DWORD>(n), &w, nullptr);
		}
		CloseHandle(h);
	}

	void TickAutoArrive()
	{
		if (!gAutoArrive || gFocusObjective < 0) return;
		if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext)) return;
		Objective* o = ObjectiveAt(static_cast<size_t>(gFocusObjective));
		if (!o || o->done || !o->hasCoord) return;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		const float dx = o->continentX - ctx->playerX;
		const float dy = o->continentY - ctx->playerY;
		if (dx * dx + dy * dy > gArriveRadius * gArriveRadius) return;
		ToggleDone(static_cast<size_t>(gFocusObjective));
		std::snprintf(gStatus, sizeof(gStatus), "Arrived - marked \"%s\" done.", o->name);
		if (gRouteMode == RouteMode::ZoneLoop)
			GuideZoneLoopNext();
		else if (gRouteMode == RouteMode::Nearest)
			GuideNearestRemaining();
	}
}
