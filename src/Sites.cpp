#include "Sites.h"

#include "WikiBrowser.h"

#include <cstring>

namespace
{
	/*
	 * Built-in sites. To add another:
	 *  1. Append a SiteDef in the right category block (keep categories contiguous).
	 *  2. Rebuild — no other wiring needed for basic browse/home.
	 *  3. Optional: set searchUrlPrefix/Suffix for toolbar/item search.
	 *  4. Optional: itemLookup=true only for wiki-style Special:Search.
	 */
	SiteDef gSites[] = {
		/* —— Help (default landing) —— */
		{
			"home",
			"Help",
			"How to use",
			"How to use",
			"about:helper-home",
			nullptr,
			nullptr,
			false,
		},

		/* —— Official —— */
		{
			"gw2official",
			"Official",
			"Guild Wars 2",
			"Guild Wars 2",
			"https://www.guildwars2.com/",
			nullptr,
			nullptr,
			false,
		},

		/* —— Wiki —— */
		{
			"wiki",
			"Wiki",
			"GW2 Wiki",
			"Guild Wars 2 Wiki",
			"https://wiki.guildwars2.com/wiki/Main_Page",
			"https://wiki.guildwars2.com/wiki/Special:Search?search=",
			"&go=Go",
			true,
		},

		/* —— Builds —— */
		{
			"snowcrows",
			"Builds",
			"Snowcrows",
			"Snowcrows",
			"https://snowcrows.com/",
			nullptr,
			nullptr,
			false,
		},
		{
			"metabattle",
			"Builds",
			"MetaBattle",
			"MetaBattle Wiki",
			"https://metabattle.com/wiki/MetaBattle_Wiki",
			nullptr,
			nullptr,
			false,
		},

		/* —— Tools —— */
		{
			"gw2efficiency",
			"Tools",
			"gw2efficiency",
			"gw2efficiency",
			"https://gw2efficiency.com/",
			nullptr,
			nullptr,
			false,
		},

		/* —— Farming —— */
		{
			"fastfarming",
			"Farming",
			"Fast Farming",
			"Fast Farming Community",
			"https://fast.farming-community.eu/",
			nullptr,
			nullptr,
			false,
		},
	};

	constexpr int kSiteCount = static_cast<int>(sizeof(gSites) / sizeof(gSites[0]));
	int gActive = 0;
}

const SiteDef* Sites::All(size_t* outCount)
{
	if (outCount)
		*outCount = static_cast<size_t>(kSiteCount);
	return gSites;
}

const SiteDef& Sites::Active()
{
	if (gActive < 0 || gActive >= kSiteCount)
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

bool Sites::SetActiveIndex(int index)
{
	if (index < 0 || index >= kSiteCount || index == gActive)
		return false;
	gActive = index;
	return true;
}

bool Sites::SetActiveById(const char* id)
{
	if (!id || !id[0])
		return false;
	for (int i = 0; i < kSiteCount; ++i)
	{
		if (std::strcmp(gSites[i].id, id) == 0)
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
	if (IsHelpSite(site))
		return "about:helper-home";
	if (query.empty() || !site.searchUrlPrefix)
		return site.homeUrl ? site.homeUrl : "";

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
