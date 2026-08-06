#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace CompletionDetail
{
	enum class ObjKind : int
	{
		Waypoint = 0,
		Poi,
		Vista,
		Hero,
		Heart,
		Mastery,
		Count
	};

	inline const char* ObjKindName(ObjKind k)
	{
		switch (k)
		{
		case ObjKind::Waypoint: return "Waypoint";
		case ObjKind::Poi: return "POI";
		case ObjKind::Vista: return "Vista";
		case ObjKind::Hero: return "Hero";
		case ObjKind::Heart: return "Heart";
		case ObjKind::Mastery: return "Mastery";
		default: return "?";
		}
	}

	inline const char* ObjKindChip(ObjKind k)
	{
		switch (k)
		{
		case ObjKind::Waypoint: return "WP";
		case ObjKind::Poi: return "POI";
		case ObjKind::Vista: return "Vista";
		case ObjKind::Hero: return "HP";
		case ObjKind::Heart: return "Heart";
		case ObjKind::Mastery: return "Mast";
		default: return "?";
		}
	}

	struct Objective
	{
		uint32_t id = 0;
		uint32_t mapId = 0;
		ObjKind kind = ObjKind::Waypoint;
		char name[96]{};
		float continentX = 0.f;
		float continentY = 0.f;
		bool hasCoord = false;
		bool done = false;
	};

	struct MapInfo
	{
		uint32_t id = 0;
		char name[96]{};
		char region[64]{};
		char release[48]{}; /* Core Tyria, HoT, PoF, ... */
	};

	enum class RouteMode : int
	{
		Nearest = 0,
		ZoneLoop,
		Count
	};

	inline const char* RouteModeName(RouteMode m)
	{
		switch (m)
		{
		case RouteMode::Nearest: return "Nearest";
		case RouteMode::ZoneLoop: return "Zone loop";
		default: return "?";
		}
	}

	extern bool gFocus;
	extern bool gPlaceOnce;
	extern int gTab; /* 0 checklist | 1 atlas | 2 route */
	extern bool gTabSelectOnce; /* apply SetSelected once then clear */
	extern RouteMode gRouteMode;
	extern char gAtlasFilter[96];
	extern char gStatus[192];
	extern uint32_t gFocusMapId;
	extern int gFocusObjective;
	extern bool gAutoArrive;
	extern bool gShowGpsArrow;
	extern float gArriveRadius;
	/* Bit i set = show ObjKind i. Default all bits. */
	extern uint32_t gKindMask;
	extern bool gAtlasFavOnly;

	void EnsureCatalog();
	size_t MapCount();
	const MapInfo* MapAt(size_t i);
	const MapInfo* FindMap(uint32_t mapId);
	size_t ObjectiveCount();
	Objective* ObjectiveAt(size_t i);
	void ObjectivesForMap(uint32_t mapId, std::vector<size_t>& out);
	void ToggleDone(size_t idx);
	void ClearDoneForMap(uint32_t mapId);
	int CountDone(uint32_t mapId);
	int CountTotal(uint32_t mapId);
	int CountDoneKind(uint32_t mapId, ObjKind k);
	int CountTotalKind(uint32_t mapId, ObjKind k);
	void SetFocusMap(uint32_t mapId);
	bool GuideToObjective(size_t idx);
	bool GuideNearestRemaining();
	bool GuideZoneLoopNext();
	/* One-shot: Lady Elyssa MC Barefoot + open Pathing (not a route mode). */
	bool OpenLadyMapCompletionPathing();
	void LoadChecklist();
	void SaveChecklist();
	bool RunRouteModeAction();
	void ClearGpsGuide();
	void TickAutoArrive();

	bool KindVisible(ObjKind k);
	void SetKindVisible(ObjKind k, bool on);
	void SetAllKindsVisible(bool on);

	bool IsFavorite(uint32_t mapId);
	void ToggleFavorite(uint32_t mapId);
	void LoadFavorites();
	void SaveFavorites();

	/* Fill release/region from curated table when empty. */
	void ApplyHierarchy(MapInfo& m);
	const char* DefaultRelease();
	const char* DefaultRegion();
	/* Enumerate curated Public map rows (for catalog seed). */
	using HierVisitFn = void (*)(uint32_t mapId, const char* release, const char* region,
		const char* name, void* ctx);
	void VisitHierarchy(HierVisitFn fn, void* ctx);

	void DrawChecklistTab();
	void DrawAtlasTab();
	void DrawRouteTab();
}
