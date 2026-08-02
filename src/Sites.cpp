#include "Sites.h"
#include "SitesInternal.h"

#include "Settings.h"
#include "WikiBrowser.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using SitesDetail::gSites;
using SitesDetail::gSiteCount;

namespace
{
	int gActive = 0;

	constexpr int kMaxCategories = 32;
	const char* gCategories[kMaxCategories] = {};
	int gCategoryCounts[kMaxCategories] = {};
	int gCategoryCount = -1;

	void ResetCategoryCache()
	{
		gCategoryCount = -1;
	}

	constexpr int kMaxFavorites = 48;
	char gFavoriteIds[kMaxFavorites][64] = {};
	int gFavoriteCount = 0;
	unsigned gFavoriteGeneration = 1;

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
}

const SiteDef* Sites::All(size_t* outCount)
{
	if (outCount)
		*outCount = static_cast<size_t>(gSiteCount > 0 ? gSiteCount : 0);
	return gSites;
}

const SiteDef& Sites::Active()
{
	if (!gSites || gSiteCount <= 0)
	{
		static SiteDef empty{};
		return empty;
	}
	if (gActive < 0 || gActive >= gSiteCount)
		gActive = 0;
	return gSites[gActive];
}

int Sites::ActiveIndex()
{
	return gActive;
}

const char* Sites::ActiveId()
{
	return Active().id;
}

const char* const* Sites::Categories(size_t* outCount)
{
	EnsureCategories();
	if (outCount)
		*outCount = static_cast<size_t>(gCategoryCount > 0 ? gCategoryCount : 0);
	return gCategories;
}

int Sites::CountInCategory(const char* category)
{
	EnsureCategories();
	if (!category)
		category = "";
	for (int i = 0; i < gCategoryCount; ++i)
	{
		const char* cat = gCategories[i] ? gCategories[i] : "";
		if (std::strcmp(cat, category) == 0)
			return gCategoryCounts[i];
	}
	return 0;
}

bool Sites::MatchesFilter(const SiteDef& site, const char* query)
{
	if (!query || !query[0])
		return true;
	return ContainsIgnoreCase(site.label, query) ||
		ContainsIgnoreCase(site.title, query) ||
		ContainsIgnoreCase(site.category, query) ||
		ContainsIgnoreCase(site.id, query);
}

bool Sites::SetActiveIndex(int index)
{
	if (!gSites || index < 0 || index >= gSiteCount || index == gActive)
		return false;
	gActive = index;
	return true;
}

bool Sites::SetActiveById(const char* id)
{
	if (!id || !id[0] || !gSites)
		return false;
	for (int i = 0; i < gSiteCount; ++i)
	{
		if (gSites[i].id && std::strcmp(gSites[i].id, id) == 0)
		{
			if (i == gActive)
				return true;
			gActive = i;
			return true;
		}
	}
	return false;
}

std::string Sites::SearchUrl(const std::string& query)
{
	const SiteDef& site = Active();
	/* DuckDuckGo tolerates the embedded OSR browser; Google serves
	   /sorry/index "unusual traffic" captchas that cannot be solved in OSR
	   (worse for Windows users whose %TEMP% cookies get cleaned). Google stays
	   available as an explicit Browse choice. */
	if (IsHelpSite(site))
	{
		if (query.empty())
			return "about:helper-home";
		return std::string("https://duckduckgo.com/?q=") + WikiBrowser::UrlEncode(query);
	}
	if (query.empty())
		return site.homeUrl ? site.homeUrl : "";
	if (!site.searchUrlPrefix)
		return std::string("https://duckduckgo.com/?q=") + WikiBrowser::UrlEncode(query);

	std::string url = site.searchUrlPrefix;
	url += WikiBrowser::UrlEncode(query);
	if (site.searchUrlSuffix)
		url += site.searchUrlSuffix;
	return url;
}

bool Sites::IsHelpSite(const SiteDef& site)
{
	return site.id && std::strcmp(site.id, "home") == 0;
}

bool Sites::ActiveIsHelp()
{
	return IsHelpSite(Active());
}

std::string Sites::ResolveUrl(const SiteDef& site)
{
	/* Help is resolved in WikiBrowser (needs addon dir for file URL). */
	if (IsHelpSite(site) ||
		(site.homeUrl && std::strcmp(site.homeUrl, "about:helper-home") == 0))
		return "about:helper-home";
	if (site.homeUrl && site.homeUrl[0])
		return site.homeUrl;
	return "about:helper-home";
}

int Sites::IndexOfId(const char* id)
{
	if (!id || !id[0] || !gSites)
		return -1;
	for (int i = 0; i < gSiteCount; ++i)
	{
		if (gSites[i].id && std::strcmp(gSites[i].id, id) == 0)
			return i;
	}
	return -1;
}

namespace
{
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

	/* Precomputed home URL keys + host→indices map (chunked warm).
	   BestMatchForUrl used to allocate ~2 strings per site on every navigate. */
	struct SiteUrlKey
	{
		std::string path;
		std::string host;
		std::string pathSlash; /* path + "/" */
		bool http = false;
	};
	constexpr int kMaxUrlKeys = 4096;
	SiteUrlKey gUrlKeys[kMaxUrlKeys];
	std::unordered_map<std::string, std::vector<int>> gUrlKeysByHost;
	std::unordered_map<std::string, int> gExactBuiltin; /* about:/file: homeUrl → index */
	bool gUrlKeysReady = false;
	bool gUrlKeysStarted = false;
	int  gUrlKeysBuildIndex = 0;

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
}

void Sites::WarmUrlKeys()
{
	/* Kick off only — do not build all ~2600 keys on AddonLoad. */
	StartUrlKeysBuild();
	TickUrlKeysBuild(64);
}

bool Sites::TickWarmUrlKeys(int sitesPerTick)
{
	if (gUrlKeysReady)
		return true;
	TickUrlKeysBuild(sitesPerTick > 0 ? sitesPerTick : 128);
	return gUrlKeysReady;
}

bool Sites::UrlKeysReady()
{
	return gUrlKeysReady;
}

int Sites::BestMatchForUrl(const std::string& url)
{
	if (url.empty())
		return -1;

	if (url.rfind("about:", 0) == 0 || url.rfind("file:", 0) == 0)
	{
		if (gUrlKeysReady)
		{
			const auto exact = gExactBuiltin.find(url);
			if (exact != gExactBuiltin.end())
				return exact->second;
		}
		else
			EnsureUrlKeys(); /* one chunk only */

		/* file:///…/helper-home.html etc. (resolved paths, not about: keys). */
		auto fileHit = [&](const char* needle, const char* id) -> int {
			if (url.find(needle) != std::string::npos)
				return IndexOfId(id);
			return -1;
		};
		int hit = fileHit("helper-home", "home");
		if (hit >= 0) return hit;
		hit = fileHit("dps-log-setup", "dpsloghelp");
		if (hit >= 0) return hit;
		hit = fileHit("api-key-setup", "apikeyhelp");
		if (hit >= 0) return hit;
		hit = fileHit("raid-food", "raidfood");
		if (hit >= 0) return hit;
		hit = fileHit("ubers-all-in-one", "ubersaio");
		if (hit >= 0) return hit;
		hit = fileHit("raid-utilities", "raidutils");
		if (hit >= 0) return hit;
		hit = fileHit("fractal-consumables", "fractalcons");
		if (hit >= 0) return hit;
		hit = fileHit("sigils-runes", "sigilsrunes");
		if (hit >= 0) return hit;
		hit = fileHit("relics-guide", "relics");
		if (hit >= 0) return hit;
		hit = fileHit("boon-checklist", "booncheck");
		if (hit >= 0) return hit;
		hit = fileHit("cc-defiance", "ccdefiance");
		if (hit >= 0) return hit;
		hit = fileHit("raid-wings", "raidwings");
		if (hit >= 0) return hit;
		hit = fileHit("home-garden", "homegarden");
		if (hit >= 0) return hit;
		hit = fileHit("strike-missions", "strikes");
		if (hit >= 0) return hit;
		hit = fileHit("fractal-cm-list", "fractalcm");
		if (hit >= 0) return hit;
		hit = fileHit("squad-template", "squadtmpl");
		if (hit >= 0) return hit;
		hit = fileHit("stability-cleanse", "stabcleanse");
		if (hit >= 0) return hit;
		hit = fileHit("material-conversions", "matconv");
		if (hit >= 0) return hit;
		hit = fileHit("legendary-paths", "legpaths");
		if (hit >= 0) return hit;
		hit = fileHit("mount-unlock", "mounts");
		if (hit >= 0) return hit;
		hit = fileHit("daily-weekly", "dailyweekly");
		if (hit >= 0) return hit;
		hit = fileHit("live-news", "live_news");
		if (hit >= 0) return hit;
		hit = fileHit("live-fashion", "live_fashion");
		if (hit >= 0) return hit;
		hit = fileHit("live-tp", "live_tp");
		if (hit >= 0) return hit;
		/* live-progress.html → Account → Progress (no Browse site id). */
		hit = fileHit("currency-sinks", "currencysinks");
		if (hit >= 0) return hit;
		hit = fileHit("ascended-start", "ascendedstart");
		if (hit >= 0) return hit;
		hit = fileHit("portals-pulls", "portalspulls");
		if (hit >= 0) return hit;
		hit = fileHit("homestead-extras", "homestead");
		if (hit >= 0) return hit;
		hit = fileHit("wvw-consumables", "wvwcons");
		if (hit >= 0) return hit;
		return -1;
	}

	EnsureUrlKeys();
	if (!gUrlKeysReady)
		return -1;

	const std::string live = UrlHostPath(url, false);
	const std::string liveHost = UrlHostPath(url, true);
	if (live.empty() || liveHost.empty())
		return -1;

	const auto hostIt = gUrlKeysByHost.find(liveHost);
	if (hostIt == gUrlKeysByHost.end())
		return -1;

	/* Candidates are longest-path-first — first path hit is the best match. */
	int hostFallback = -1;
	for (int i : hostIt->second)
	{
		const SiteUrlKey& key = gUrlKeys[i];
		if (!key.http || key.path.empty())
			continue;
		if (live == key.path || live.rfind(key.pathSlash, 0) == 0 || live.rfind(key.path, 0) == 0)
			return i;
		if (hostFallback < 0)
			hostFallback = i;
	}

	return hostFallback;
}

bool Sites::IsFavorite(const char* id)
{
	if (!id || !id[0])
		return false;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (std::strcmp(gFavoriteIds[i], id) == 0)
			return true;
	}
	return false;
}

bool Sites::ToggleFavorite(const char* id)
{
	if (!id || !id[0] || IndexOfId(id) < 0)
		return false;

	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (std::strcmp(gFavoriteIds[i], id) == 0)
		{
			for (int j = i; j < gFavoriteCount - 1; ++j)
				std::snprintf(gFavoriteIds[j], sizeof(gFavoriteIds[j]), "%s", gFavoriteIds[j + 1]);
			gFavoriteIds[gFavoriteCount - 1][0] = 0;
			--gFavoriteCount;
			++gFavoriteGeneration;
			Settings::SetDirty();
			return false;
		}
	}

	if (gFavoriteCount >= kMaxFavorites)
		return false;
	std::snprintf(gFavoriteIds[gFavoriteCount], sizeof(gFavoriteIds[gFavoriteCount]), "%s", id);
	++gFavoriteCount;
	++gFavoriteGeneration;
	Settings::SetDirty();
	return true;
}

int Sites::FavoriteCount()
{
	return gFavoriteCount;
}

unsigned Sites::FavoritesGeneration()
{
	return gFavoriteGeneration;
}

int Sites::FavoriteSiteIndex(int favSlot)
{
	if (favSlot < 0 || favSlot >= gFavoriteCount)
		return -1;
	return IndexOfId(gFavoriteIds[favSlot]);
}

void Sites::ParseFavorites(const char* csv)
{
	gFavoriteCount = 0;
	++gFavoriteGeneration;
	if (!csv || !csv[0])
		return;

	const char* p = csv;
	while (*p && gFavoriteCount < kMaxFavorites)
	{
		while (*p == ' ' || *p == ',')
			++p;
		if (!*p)
			break;
		const char* start = p;
		while (*p && *p != ',')
			++p;
		size_t len = static_cast<size_t>(p - start);
		while (len > 0 && start[len - 1] == ' ')
			--len;
		if (len == 0 || len >= sizeof(gFavoriteIds[0]))
			continue;
		std::memcpy(gFavoriteIds[gFavoriteCount], start, len);
		gFavoriteIds[gFavoriteCount][len] = 0;
		++gFavoriteCount;
	}
	PruneFavorites();
}

void Sites::SerializeFavorites(char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return;
	out[0] = 0;
	size_t used = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		const char* id = gFavoriteIds[i];
		if (!id || !id[0])
			continue;
		const size_t idLen = std::strlen(id);
		const size_t need = idLen + (used ? 1u : 0u);
		if (used + need + 1 >= outLen)
			break;
		if (used)
			out[used++] = ',';
		std::memcpy(out + used, id, idLen);
		used += idLen;
		out[used] = 0;
	}
}

void Sites::PruneFavorites()
{
	int w = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (IndexOfId(gFavoriteIds[i]) < 0)
			continue;
		/* Drop duplicates while pruning. */
		bool dup = false;
		for (int j = 0; j < w; ++j)
		{
			if (std::strcmp(gFavoriteIds[j], gFavoriteIds[i]) == 0)
			{
				dup = true;
				break;
			}
		}
		if (dup)
			continue;
		if (w != i)
			std::snprintf(gFavoriteIds[w], sizeof(gFavoriteIds[w]), "%s", gFavoriteIds[i]);
		++w;
	}
	for (int i = w; i < gFavoriteCount; ++i)
		gFavoriteIds[i][0] = 0;
	if (w != gFavoriteCount)
		++gFavoriteGeneration;
	gFavoriteCount = w;
}

bool Sites::MoveFavorite(int fromSlot, int toSlot)
{
	if (fromSlot < 0 || toSlot < 0 || fromSlot >= gFavoriteCount || toSlot >= gFavoriteCount)
		return false;
	if (fromSlot == toSlot)
		return false;

	char tmp[64];
	std::snprintf(tmp, sizeof(tmp), "%s", gFavoriteIds[fromSlot]);
	if (fromSlot < toSlot)
	{
		for (int i = fromSlot; i < toSlot; ++i)
			std::snprintf(gFavoriteIds[i], sizeof(gFavoriteIds[i]), "%s", gFavoriteIds[i + 1]);
	}
	else
	{
		for (int i = fromSlot; i > toSlot; --i)
			std::snprintf(gFavoriteIds[i], sizeof(gFavoriteIds[i]), "%s", gFavoriteIds[i - 1]);
	}
	std::snprintf(gFavoriteIds[toSlot], sizeof(gFavoriteIds[toSlot]), "%s", tmp);
	++gFavoriteGeneration;
	Settings::SetDirty();
	return true;
}

namespace
{
	void OnCatalogReloaded()
	{
		gActive = 0;
		ResetCategoryCache();
		ResetUrlKeys();
	}
}

void Sites::Init()
{
	SitesDetail::LoadCatalog();
	OnCatalogReloaded();
}

void Sites::Shutdown()
{
	SitesDetail::ClearCatalog();
	OnCatalogReloaded();
}
