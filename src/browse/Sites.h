#pragma once

#include <cstddef>
#include <string>

/* One entry per site the helper can open.
   Canonical catalog: data/sites.json (git) → extracted at runtime to
   addons/<ADDON_NAME>/sites.json (schema v2 + browsePath / browseSections).
   Keep sites grouped by category (contiguous). Run: make validate-sites. */
struct SiteDef
{
	const char* id;              /* stable settings key, e.g. "wiki" */
	const char* category;        /* picker group header, e.g. "Builds" */
	const char* label;           /* short picker label */
	const char* title;           /* window / status title */
	const char* homeUrl;         /* Home button + initial load */
	const char* searchUrlPrefix; /* nullptr = Search just opens home */
	const char* searchUrlSuffix; /* appended after UrlEncode(query) */
	/* Browse hierarchy under category (may be empty). Owned by Sites catalog. */
	const char* const* browsePath;
	int browsePathCount;
};

namespace Sites
{
	/* Extract default catalog if needed, then parse DataDir/sites.json.
	   Soft-fails to a minimal in-memory home entry. */
	void Init();
	void Shutdown();

	const SiteDef* All(size_t* outCount);
	const SiteDef& Active();
	int            ActiveIndex();
	const char*    ActiveId();

	/* Unique category names in registry order. Pointers into SiteDef::category. */
	const char* const* Categories(size_t* outCount);

	/* Ordered Browse section titles for a category (from browseSections). */
	const char* const* BrowseSections(const char* category, size_t* outCount);

	/* How many sites belong to category (exact string match). */
	int CountInCategory(const char* category);

	/* True if site matches a case-insensitive substring filter. */
	bool MatchesFilter(const SiteDef& site, const char* query);

	/* Switch site; returns true if the active site changed. */
	bool SetActiveIndex(int index);
	bool SetActiveById(const char* id);

	/* Build a search URL for the active site (falls back to home). */
	std::string SearchUrl(const std::string& query);

	/* True for the built-in how-to page. */
	bool IsHelpSite(const SiteDef& site);
	bool ActiveIsHelp();

	/* Resolve the URL to open for a site (help page or homeUrl). */
	std::string ResolveUrl(const SiteDef& site);

	/* Favorites — persisted site ids (order preserved) + optional folders. */
	bool IsFavorite(const char* id);
	bool ToggleFavorite(const char* id); /* returns true if now favorited */
	int  FavoriteCount();
	int  FavoriteSiteIndex(int favSlot); /* registry index, or -1 */
	/* Bumps when favorites add/remove/reorder/load — Browse cache invalidation. */
	unsigned FavoritesGeneration();
	int  IndexOfId(const char* id);      /* registry index, or -1 */
	/* Best registry site for a live URL (−1 if none). Prefers longest homeUrl match. */
	int  BestMatchForUrl(const std::string& url);
	/* Start URL-match index build (chunked — finish via TickWarmUrlKeys). */
	void WarmUrlKeys();
	/* True when URL-match indexes are fully built. */
	bool UrlKeysReady();
	/* Continue index build on the render thread (returns true when ready). */
	bool TickWarmUrlKeys(int sitesPerTick = 128);

	void ParseFavorites(const char* csv);
	void SerializeFavorites(char* out, size_t outLen);
	void PruneFavorites(); /* drop unknown / empty ids */
	bool MoveFavorite(int fromSlot, int toSlot); /* reorder; returns true if moved */

	/* Favorite folders (id 0 = Unfiled). Stored in config/favorites.json. */
	int  FavoriteFolderCount(); /* user folders only (excludes Unfiled) */
	int  FavoriteFolderIdAt(int index);
	const char* FavoriteFolderName(int folderId);
	int  FavoriteFolderOf(const char* siteId);
	int  FavoriteCountInFolder(int folderId);
	int  FavoriteSiteIndexInFolder(int folderId, int slotInFolder);
	bool CreateFavoriteFolder(const char* name);
	bool RenameFavoriteFolder(int folderId, const char* name);
	bool DeleteFavoriteFolder(int folderId); /* items move to Unfiled */
	bool SetFavoriteFolder(const char* siteId, int folderId);
	bool MoveFavoriteInFolder(int folderId, int fromSlot, int toSlot);
	void LoadFavoritesStore();
	void SaveFavoritesStore();
}
