#include "Sites.h"
#include "SitesInternal.h"

#include "WikiBrowser.h"

#include <cstdio>
#include <cstring>
#include <string>

using SitesDetail::gSites;
using SitesDetail::gSiteCount;
using SitesRuntimeDetail::ContainsIgnoreCase;
using SitesRuntimeDetail::EnsureCategories;
using SitesRuntimeDetail::gActive;
using SitesRuntimeDetail::gCategories;
using SitesRuntimeDetail::gCategoryCount;
using SitesRuntimeDetail::gCategoryCounts;
using SitesRuntimeDetail::OnCatalogReloaded;

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
	return "about:browse-hub";
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
