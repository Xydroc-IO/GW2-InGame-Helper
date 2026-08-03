#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/* Loads TacO/Blish .taco packs from this addon's pathing/ folder (Tekkit +
   any custom packs the user drops in). Categories default OFF — only
   user-enabled paths draw. Compass + world overlays consume CurrentTrails /
   NearbyWorldGps. Other pathing addons are optional fallbacks. */

namespace TekkitTrails
{
	struct Point
	{
		float x = 0.f;
		float y = 0.f;
	};

	struct WorldPoint
	{
		float x = 0.f;
		float y = 0.f; /* up */
		float z = 0.f;
	};

	struct Trail
	{
		uint32_t mapId = 0;
		uint32_t color = 0xFF00FFFF; /* ARGB */
		char     label[96]{};
		char     textureId[160]{}; /* pack trail texture, uploaded through Nexus */
		bool     minimapVisible = true;
		bool     inGameVisible = true;
		float    alpha = 1.f;
		float    trailScale = 1.f;
		float    fadeNear = -1.f; /* pack units (inches) */
		float    fadeFar = -1.f;
		std::vector<Point> points;           /* continent coords, decimated */
		std::vector<WorldPoint> worldPoints; /* meters (Mumble / .trl space) */
	};

	/* Point marker from Tekkit <POI> (hearts, signs, caches, etc.). */
	struct Marker
	{
		uint32_t mapId = 0;
		uint32_t color = 0xFFFFCC33; /* ARGB */
		char     label[96]{};
		char     iconId[160]{}; /* Nexus texture id when icon loaded */
		Point    pos;           /* continent */
		WorldPoint world;
		bool     minimapVisible = true;
		bool     inGameVisible = true;
		float    mapDisplaySize = 20.f;
		float    minSize = 5.f;
		float    maxSize = 2048.f;
		float    iconSize = 1.f;
		float    heightOffset = 1.5f;
		float    fadeNear = -1.f;
		float    fadeFar = -1.f;
		float    alpha = 1.f;
	};

	struct Category
	{
		std::string path;   /* e.g. "tw_guides.tw_mc" — enable prefix */
		std::string label;  /* DisplayName from Tekkit menu when known */
		int         trails = 0; /* trails + POIs under this prefix */
		bool        enabled = false; /* opt-in: on only if user enabled this/ancestor */
		bool        separator = false; /* section header, not toggleable */
		bool        hidden = false;
		std::vector<Category> children;
	};

	void Init();
	void Shutdown();

	/* Call each frame with the player's map id (from Mumble). Always indexes
	   packs so search routing works even with no categories enabled. */
	void Update(uint32_t mapId);

	bool MasterEnabled();
	void SetMasterEnabled(bool on);

	bool IsLoading();
	int  PackCount();
	std::vector<std::string> LoadedPackNames();
	std::string PathingFolderHint();
	void ReloadPacks(); /* re-scan pathing/ after adding/removing .taco files */
	/* Force re-download curated packs (Lady Elyssa + Tekkit), then ReloadPacks. */
	void UpdateCuratedPacks();
	int  TrailCount();          /* visible (category-enabled) trails */
	int  TrailCountAllOnMap();  /* all loaded for this map (for routing) */
	int  MarkerCount();         /* visible (category-enabled) POIs */
	/* Bumps when current-map trail/marker set changes — world GPS cache key. */
	uint64_t ContentRevision();

	/* Category tree — Tekkit menu order / DisplayNames (like the official overlay). */
	std::vector<Category> CategoryTree();
	void SetCategoryEnabled(const std::string& path, bool enabled);
	/* Map-completion hearts/POIs/vistas + one route edition (not both). */
	enum class MapCompletionRoutes
	{
		None     = -1,
		Barefoot = 0,
		Griffon  = 1,
		Skyscale = 2,
	};
	void EnableMapCompletionPreset(MapCompletionRoutes routes);
	void ClearMapCompletionCategories();
	/* Which route edition is currently selected via the MC preset (-1 = none/mixed). */
	MapCompletionRoutes ActiveMapCompletionRoutes();
	void EnableAllTekkitCategories();
	void EnableAllLadyCategories(); /* Lady Elyssa Guides (legs) + Achievements (leag) */
	void DisableAllCategories();
	/* Exact category paths the user turned on (prefix enables descendants). */
	std::vector<std::string> EnabledPaths();
	void SetEnabledPaths(const std::vector<std::string>& paths);
	/* Persist helpers — '|' separated category paths (Blish-style remember). */
	void SerializeEnabledPaths(char* out, size_t outLen);
	void ParseEnabledPaths(const char* pipeList);
	/* Open addons/.../pathing in Explorer / file manager. */
	bool OpenPathingFolder();

	/* Visible trails / markers for the active map (respects category toggles). */
	std::vector<Trail> CurrentTrails();
	std::vector<Marker> CurrentMarkers();
	std::vector<Marker> CurrentMarkersInBounds(
		float minX, float minY, float maxX, float maxY);
	std::vector<Marker> NearbyWorldMarkers(
		float x, float y, float z, float maxDistance, size_t maxMarkers);
	void BeginFrame(); /* upload pending Tekkit icon textures */

	/* Lightweight nearby world polylines for in-world GPS (no full-map copy). */
	struct WorldSnippet
	{
		uint32_t color = 0xFF00FFFF;
		char textureId[160]{};
		float alpha = 1.f;
		float trailScale = 1.f;
		float fadeNear = -1.f;
		float fadeFar = -1.f;
		std::vector<WorldPoint> points;
	};
	std::vector<WorldSnippet> NearbyWorldSnippets(
		float avatarX, float avatarY, float avatarZ,
		float maxDistMeters, int maxTrails, int maxPointTests);

	/* Render-thread nearby GPS. Returns false on lock miss — keep prior frame. */
	bool TryNearbyWorldGps(
		float avatarX, float avatarY, float avatarZ, float maxDistMeters,
		std::vector<WorldSnippet>& outSnippets,
		std::vector<Marker>& outMarkers);

	/* Orange search-route GPS in world meters (independent of category toggles). */
	WorldSnippet SearchGuideWorldSnippet();

	/* Search: pick a Tekkit trail sub-path toward this continent point. */
	void SetSearchDestination(float continentX, float continentY);
	void ClearSearchGuide();
	bool HasSearchGuide();
	Trail SearchGuide(); /* empty if none found yet */

	/* Settings panel (ImGui). Returns true if settings changed. */
	bool DrawSettings();
}
