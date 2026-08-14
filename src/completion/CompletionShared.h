#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
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
		Achievement,
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
		case ObjKind::Achievement: return "Achievement";
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
		case ObjKind::Achievement: return "AP";
		default: return "?";
		}
	}

	struct Objective
	{
		uint32_t id = 0;
		uint32_t mapId = 0;
		ObjKind kind = ObjKind::Waypoint;
		char name[96]{};
		char packType[160]{}; /* pack type path; empty for floors API */
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

	enum class AtlasScope : int
	{
		Public = 0, /* open-world releases — default */
		Strikes,
		Festival,
		All,
		Count
	};

	inline const char* AtlasScopeName(AtlasScope s)
	{
		switch (s)
		{
		case AtlasScope::Public: return "Public";
		case AtlasScope::Strikes: return "Strikes";
		case AtlasScope::Festival: return "Festival";
		case AtlasScope::All: return "All";
		default: return "?";
		}
	}

	inline bool MapInAtlasScope(const MapInfo& m, AtlasScope scope)
	{
		if (scope == AtlasScope::All) return true;
		const bool strike = std::strncmp(m.release, "Strikes", 7) == 0;
		const bool fest = std::strncmp(m.release, "Festival", 8) == 0;
		if (scope == AtlasScope::Strikes) return strike;
		if (scope == AtlasScope::Festival) return fest;
		/* Public: everything except Strikes / Festival groups */
		return !strike && !fest;
	}

	extern bool gFocus;
	extern bool gPlaceOnce;
	extern int  gDeferHeavy;
	extern int gTab; /* 0 checklist | 1 atlas | 2 route | 3 achievements */
	extern bool gTabSelectOnce; /* apply SetSelected once then clear */
	extern RouteMode gRouteMode;
	extern char gAtlasFilter[96];
	extern AtlasScope gAtlasScope;
	extern char gStatus[192];
	extern uint32_t gFocusMapId;
	extern int gFocusObjective;
	extern bool gAutoArrive;
	extern bool gShowGpsArrow;
	extern float gArriveRadius;
	/* Bit i set = show ObjKind i. Default all bits. */
	extern uint32_t gKindMask;
	extern bool gAtlasFavOnly;
	/* Achievements tab: 0 = account API | 1 = Lady pack GPS. */
	extern int gApPane;
	extern char gApSearch[64];
	extern int gApFilter; /* 0 all | 1 done | 2 open */
	extern int gApSelCatId;
	extern int gApSelAchId;
	/* Achievements tab: selected Lady AP category path (prefix filter). */
	extern char gApCategoryPath[160];

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
	/* One-shot: enable one Lady AP category path + open Pathing. */
	bool OpenLadyAchievementPathing(const char* categoryPath);
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

	constexpr int kMaxAchPins = 5;
	void LoadAchPins();
	const std::vector<int>& AchPins();
	bool IsAchPinned(int achievementId);
	bool ToggleAchPin(int achievementId); /* false if already at kMaxAchPins */
	void FocusAchPin(int achievementId);

	/* Fill release/region from curated table when empty. */
	void ApplyHierarchy(MapInfo& m);
	const char* DefaultRelease();
	const char* DefaultRegion();
	/* Enumerate curated map rows — Public + Strikes + Festival/(Public) clones. */
	using HierVisitFn = void (*)(uint32_t mapId, const char* release, const char* region,
		const char* name, void* ctx);
	void VisitHierarchy(HierVisitFn fn, void* ctx);

	bool IsMapCompletionRouteKind(ObjKind k);

	void DrawChecklistTab();
	void DrawAtlasTab();
	void DrawRouteTab();
	void DrawAchievementsTab();

	/* Account achievement overlay (Explorer / curated ids — not map %). */
	struct ApProgress
	{
		uint32_t achievementId = 0;
		bool known = false;
		bool done = false;
		int current = 0;
		int max = 0;
		/* Completed bit indices from /v2/account/achievements "bits". */
		std::vector<int> bits;
	};
	void BeginApOverlayRefresh();
	void ApplyApOverlayResult();
	bool ApOverlayBusy();
	bool LookupApProgress(uint32_t achievementId, ApProgress& out);
	size_t ApProgressCount();
	bool FormatApOverlayLine(uint32_t mapId, const char* packType, char* out, size_t outLen);

	struct AchGroup
	{
		std::string id;
		std::string name;
		int order = 0;
		std::vector<int> categoryIds;
	};
	struct AchCategory
	{
		int id = 0;
		std::string name;
		int order = 0;
		std::vector<int> achievementIds;
	};
	enum class AchBitKind : int
	{
		Text = 0,
		Item,
		Skin,
		Mini,
		Achievement,
		Other
	};
	struct AchBit
	{
		std::string text;
		AchBitKind kind = AchBitKind::Text;
		int targetId = 0;
	};
	struct AchDef
	{
		int id = 0;
		std::string name;
		std::string requirement;
		std::string description;
		std::string lockedText;
		std::vector<AchBit> bits;
		int points = 0;
		bool hidden = false;
	};
	void BeginAchCatalogRefresh(bool force);
	void ApplyAchCatalogResult();
	bool AchCatalogBusy();
	bool AchCatalogReady();
	const std::vector<AchGroup>& AchGroups();
	const AchCategory* FindAchCategory(int id);
	int CategoryIdContainingAchievement(int achievementId);
	void BeginAchDefsRefresh(int categoryId);
	void BeginAchDefsForIds(const std::vector<int>& ids);
	void ApplyAchDefsResult();
	bool AchDefsBusy();
	const AchDef* FindAchDef(int id);

	void BeginAchWikiThumb(int achievementId, const char* name);
	void ApplyAchWikiThumbResult();
	bool LookupAchWikiThumbUrl(int achievementId, std::string& outUrl);
}
