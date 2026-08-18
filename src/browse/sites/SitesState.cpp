#include "SitesInternal.h"

#include <algorithm>
#include <cctype>
#include <cstring>

using SitesDetail::gSites;
using SitesDetail::gSiteCount;

namespace SitesRuntimeDetail
{
	int gActive = 0;

	const char* gCategories[kMaxCategories] = {};
	int gCategoryCounts[kMaxCategories] = {};
	int gCategoryCount = -1;

	char gFavoriteUrls[kMaxFavorites][kMaxFavoriteUrl] = {};
	char gFavoriteTitles[kMaxFavorites][kMaxFavoriteTitle] = {};
	int gFavoriteFolderIds[kMaxFavorites] = {};
	int gFavoriteCount = 0;
	FavoriteFolder gFavoriteFolders[kMaxFavoriteFolders] = {};
	int gFavoriteFolderCount = 0;
	int gFavoriteNextFolderId = 1;
	unsigned gFavoriteGeneration = 1;

	bool FolderIdKnown(int folderId)
	{
		if (folderId == kUnfiledFavoriteFolderId)
			return true;
		for (int i = 0; i < gFavoriteFolderCount; ++i)
		{
			if (gFavoriteFolders[i].id == folderId)
				return true;
		}
		return false;
	}

	SiteUrlKey gUrlKeys[kMaxUrlKeys];
	std::unordered_map<std::string, std::vector<int>> gUrlKeysByHost;
	std::unordered_map<std::string, int> gExactBuiltin;
	bool gUrlKeysReady = false;
	bool gUrlKeysStarted = false;
	int  gUrlKeysBuildIndex = 0;

	void ResetCategoryCache()
	{
		gCategoryCount = -1;
	}

	void EnsureCategories()
	{
		if (gCategoryCount >= 0)
			return;
		gCategoryCount = 0;
		if (!gSites || gSiteCount <= 0)
			return;
		const char* last = nullptr;
		int lastIdx = -1;
		for (int i = 0; i < gSiteCount && gCategoryCount < kMaxCategories; ++i)
		{
			const char* cat = gSites[i].category ? gSites[i].category : "";
			if (!last || std::strcmp(last, cat) != 0)
			{
				lastIdx = gCategoryCount;
				gCategories[gCategoryCount] = cat;
				gCategoryCounts[gCategoryCount] = 1;
				++gCategoryCount;
				last = cat;
			}
			else if (lastIdx >= 0)
				++gCategoryCounts[lastIdx];
		}
	}

	bool ContainsIgnoreCase(const char* haystack, const char* needle)
	{
		if (!needle || !needle[0])
			return true;
		if (!haystack || !haystack[0])
			return false;
		const size_t nlen = std::strlen(needle);
		for (const char* p = haystack; *p; ++p)
		{
			size_t i = 0;
			while (i < nlen)
			{
				const unsigned char a = static_cast<unsigned char>(p[i]);
				const unsigned char b = static_cast<unsigned char>(needle[i]);
				if (!a)
					return false;
				if (std::tolower(a) != std::tolower(b))
					break;
				++i;
			}
			if (i == nlen)
				return true;
		}
		return false;
	}

	void ResetUrlKeys()
	{
		gUrlKeysReady = false;
		gUrlKeysStarted = false;
		gUrlKeysBuildIndex = 0;
		gUrlKeysByHost.clear();
		gExactBuiltin.clear();
	}

	void FinalizeUrlKeys()
	{
		for (auto& kv : gUrlKeysByHost)
		{
			std::vector<int>& idxs = kv.second;
			std::sort(idxs.begin(), idxs.end(), [](int a, int b) {
				return gUrlKeys[a].path.size() > gUrlKeys[b].path.size();
			});
		}
		gUrlKeysReady = true;
	}

	void StartUrlKeysBuild()
	{
		if (gUrlKeysStarted || gUrlKeysReady)
			return;
		if (gSiteCount > kMaxUrlKeys)
			return; /* catalog too large — leave URL index cold */
		gUrlKeysByHost.clear();
		gUrlKeysByHost.reserve(512);
		gExactBuiltin.clear();
		gExactBuiltin.reserve(64);
		gUrlKeysBuildIndex = 0;
		gUrlKeysStarted = true;
	}

	void TickUrlKeysBuild(int chunk)
	{
		if (gUrlKeysReady)
			return;
		StartUrlKeysBuild();
		if (!gSites || gSiteCount <= 0)
			return;
		if (chunk < 1)
			chunk = 1;
		const int end = (gUrlKeysBuildIndex + chunk < gSiteCount)
			? (gUrlKeysBuildIndex + chunk) : gSiteCount;
		for (int i = gUrlKeysBuildIndex; i < end; ++i)
		{
			SiteUrlKey& k = gUrlKeys[i];
			k = SiteUrlKey{};
			const char* home = gSites[i].homeUrl;
			if (!home || !home[0])
				continue;
			if (std::strncmp(home, "about:", 6) == 0 || std::strncmp(home, "file:", 5) == 0)
			{
				gExactBuiltin.emplace(home, i);
				continue;
			}
			if (std::strncmp(home, "http", 4) != 0)
				continue;
			k.path = UrlHostPath(home, false);
			k.host = UrlHostPath(home, true);
			if (k.path.empty())
				continue;
			k.pathSlash = k.path + "/";
			k.http = true;
			if (!k.host.empty())
				gUrlKeysByHost[k.host].push_back(i);
		}
		gUrlKeysBuildIndex = end;
		if (gUrlKeysBuildIndex >= gSiteCount)
			FinalizeUrlKeys();
	}

	void EnsureUrlKeys()
	{
		if (gUrlKeysReady)
			return;
		/* Never finish the full ~2600-site build on the render thread.
		   Callers must tolerate a miss until TickWarmUrlKeys completes. */
		TickUrlKeysBuild(96);
	}

	std::string UrlHostPath(const std::string& url, bool hostOnly)
	{
		std::string u = url;
		const size_t scheme = u.find("://");
		if (scheme != std::string::npos)
			u = u.substr(scheme + 3);
		if (u.rfind("www.", 0) == 0)
			u = u.substr(4);
		if (hostOnly)
		{
			const size_t slash = u.find('/');
			if (slash != std::string::npos)
				u = u.substr(0, slash);
			const size_t q = u.find('?');
			if (q != std::string::npos)
				u = u.substr(0, q);
		}
		else
		{
			const size_t q = u.find('?');
			if (q != std::string::npos)
				u = u.substr(0, q);
			while (!u.empty() && u.back() == '/')
				u.pop_back();
		}
		return u;
	}

	void OnCatalogReloaded()
	{
		gActive = 0;
		ResetCategoryCache();
		ResetUrlKeys();
	}
}
