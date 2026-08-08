#include "CompletionShared.h"
#include "CompletionInternal.h"

#include "AddonPaths.h"
#include "PathingTrails.h"
#include "UiAscii.h"
#include "WaypointsData.h"

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
	int  gDeferHeavy = 0;
	int gTab = 0;
	bool gTabSelectOnce = false;
	RouteMode gRouteMode = RouteMode::Nearest;
	char gAtlasFilter[96]{};
	AtlasScope gAtlasScope = AtlasScope::Public;
	char gStatus[192]{};
	uint32_t gFocusMapId = 0;
	int gFocusObjective = -1;
	bool gAutoArrive = true;
	bool gShowGpsArrow = true;
	float gArriveRadius = 120.f;
	char gApCategoryPath[160]{};

	namespace
	{
		std::recursive_mutex gMu;
		std::vector<MapInfo> gMaps;
		std::vector<Objective> gObjs;
		std::unordered_set<uint32_t> gDoneIds;
		bool gReady = false;
		bool gChecklistLoaded = false;
		uint32_t gLiveSyncedMap = 0;
		size_t gPackMergeMarkerCount = 0;
		uint32_t gPackMergeFetchMap = 0;

		std::wstring ChecklistPath()
		{
			return AddonPaths::ConfigDir() + L"\\completion-checklist.txt";
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
			float cx, float cy, bool hasCoord, const char* packType)
		{
			for (Objective& o : gObjs)
			{
				if (o.id == id)
				{
					o.done = gDoneIds.count(id) != 0;
					if (hasCoord && !o.hasCoord)
					{
						o.continentX = cx;
						o.continentY = cy;
						o.hasCoord = true;
					}
					if (name && name[0] && !o.name[0])
						UiAscii::SanitizeForUi(o.name, sizeof(o.name), name);
					if (packType && packType[0] && !o.packType[0])
						std::snprintf(o.packType, sizeof(o.packType), "%s", packType);
					return;
				}
			}
			Objective o{};
			o.id = id;
			o.mapId = mapId;
			o.kind = kind;
			if (name && name[0])
				UiAscii::SanitizeForUi(o.name, sizeof(o.name), name);
			if (packType && packType[0])
				std::snprintf(o.packType, sizeof(o.packType), "%s", packType);
			o.continentX = cx;
			o.continentY = cy;
			o.hasCoord = hasCoord;
			o.done = gDoneIds.count(id) != 0;
			gObjs.push_back(o);
		}
	}

	void EnsureCatalog()
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		if (!gReady)
		{
			gMaps.clear();
			gObjs.clear();
			ResetFloorMergeState();
			SeedCatalogMaps();
			gReady = true;
		}

		WaypointsData::EnsureLoaded(false);
		WaypointsData::Tick();

		const uint32_t cur = static_cast<uint32_t>(WaypointsData::CurrentMapId());
		uint32_t want = gFocusMapId != 0 ? gFocusMapId : cur;
		if (want == 0)
			want = cur;

		if (!WaypointsData::Ready())
		{
			if (gLiveSyncedMap != 0 && want != 0 && want != gLiveSyncedMap)
				gLiveSyncedMap = 0;
			return;
		}

		EnrichMapNamesFromFloors();

		if (want != 0 && !FloorPoisMerged(want))
		{
			MergeFloorPois(want);
			gLiveSyncedMap = want;
			if (gFocusMapId == 0 && cur != 0)
				gFocusMapId = cur;
		}
		else if (want != 0)
		{
			gLiveSyncedMap = want;
		}

		MergeFloorPoisBackground();

		const size_t packN = PathingTrails::IndexedMarkerCount();
		uint32_t fetchMap = gFocusMapId != 0 ? gFocusMapId : cur;
		if (packN != gPackMergeMarkerCount || fetchMap != gPackMergeFetchMap)
		{
			MergePackMarkers();
			gPackMergeMarkerCount = packN;
			gPackMergeFetchMap = fetchMap;
		}
	}

	void UpsertMapEntry(uint32_t id, const char* name, const char* region)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		UpsertMap(id, name, region);
	}

	void UpsertObjective(uint32_t id, uint32_t mapId, ObjKind kind, const char* name,
		float cx, float cy, bool hasCoord, const char* packType)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		AddObj(id, mapId, kind, name, cx, cy, hasCoord, packType);
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
			if (o.mapId == mapId && o.done && IsMapCompletionRouteKind(o.kind)) ++n;
		return n;
	}

	int CountTotal(uint32_t mapId)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		int n = 0;
		for (const Objective& o : gObjs)
			if (o.mapId == mapId && IsMapCompletionRouteKind(o.kind)) ++n;
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
}
