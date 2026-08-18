#pragma once

/* Internal catalog + runtime storage shared by Sites*.cpp / SitesLoad*.cpp. */

#include "Sites.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace SitesDetail
{
	extern SiteDef* gSites;
	extern int gSiteCount;

	/* Load/extract DataDir/sites.json into gSites. Returns false if only fallback. */
	bool LoadCatalog();
	void ClearCatalog();

	const char* const* BrowseSectionsFor(const char* category, size_t* outCount);
}

namespace SitesRuntimeDetail
{
	extern int gActive;

	constexpr int kMaxCategories = 32;
	extern const char* gCategories[kMaxCategories];
	extern int gCategoryCounts[kMaxCategories];
	extern int gCategoryCount;

	void ResetCategoryCache();
	void EnsureCategories();
	bool ContainsIgnoreCase(const char* haystack, const char* needle);

	constexpr int kMaxFavorites = 48;
	constexpr int kMaxFavoriteFolders = 16;
	constexpr int kUnfiledFavoriteFolderId = 0;
	constexpr int kMaxFavoriteUrl = 512;
	constexpr int kMaxFavoriteTitle = 64;

	struct FavoriteFolder
	{
		int id = 0;
		char name[48] = {};
	};

	extern char gFavoriteUrls[kMaxFavorites][kMaxFavoriteUrl];
	extern char gFavoriteTitles[kMaxFavorites][kMaxFavoriteTitle];
	extern int gFavoriteFolderIds[kMaxFavorites];
	extern int gFavoriteCount;
	extern FavoriteFolder gFavoriteFolders[kMaxFavoriteFolders];
	extern int gFavoriteFolderCount;
	extern int gFavoriteNextFolderId;
	extern unsigned gFavoriteGeneration;

	bool FolderIdKnown(int folderId);
	bool FavoriteUrlsMatch(const char* a, const char* b);
	void FavoriteTitleFromUrl(char* dst, size_t dstLen, const char* url);
	void MigrateFavoriteSiteIds();
	void MarkFavoritesChanged(bool saveNow);

	struct SiteUrlKey
	{
		std::string path;
		std::string host;
		std::string pathSlash; /* path + "/" */
		bool http = false;
	};

	constexpr int kMaxUrlKeys = 4096;
	extern SiteUrlKey gUrlKeys[kMaxUrlKeys];
	extern std::unordered_map<std::string, std::vector<int>> gUrlKeysByHost;
	extern std::unordered_map<std::string, int> gExactBuiltin; /* about:/file: homeUrl → index */
	extern bool gUrlKeysReady;
	extern bool gUrlKeysStarted;
	extern int  gUrlKeysBuildIndex;

	void ResetUrlKeys();
	void FinalizeUrlKeys();
	void StartUrlKeysBuild();
	void TickUrlKeysBuild(int chunk);
	void EnsureUrlKeys();
	std::string UrlHostPath(const std::string& url, bool hostOnly);

	void OnCatalogReloaded();
}
