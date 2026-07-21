#include "Sites.h"

#include "WikiBrowser.h"

#include <cstring>

namespace
{
	/*
	 * Built-in sites. To add another:
	 *  1. Append a SiteDef in the right category block (keep categories contiguous).
	 *  2. Rebuild — no other wiring needed for basic browse/home.
	 *  3. Optional: set searchUrlPrefix/Suffix for toolbar search.
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
		},
		{
			"raidcore",
			"Official",
			"Raidcore",
			"Raidcore — Guild Wars 2",
			"https://raidcore.gg/gw2",
			nullptr,
			nullptr,
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
		},
		{
			"metabattle",
			"Builds",
			"MetaBattle",
			"MetaBattle Wiki",
			"https://metabattle.com/wiki/MetaBattle_Wiki",
			nullptr,
			nullptr,
		},
		{
			"aw2help",
			"Builds",
			"Accessibility Wars",
			"Accessibility Wars",
			"https://aw2.help/",
			nullptr,
			nullptr,
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
		},
		{
			"gw2timer",
			"Tools",
			"GW2Timer Map",
			"GW2Timer.com — Map",
			"https://gw2timer.com/?page=Map",
			nullptr,
			nullptr,
		},
		{
			"gw2crafts",
			"Tools",
			"GW2 Crafts",
			"GW2 Crafts — Crafting Guides",
			"https://gw2crafts.net/",
			nullptr,
			nullptr,
		},
		{
			"gw2mb",
			"Tools",
			"Music Box",
			"GW2 Music Box",
			"https://gw2mb.com/",
			nullptr,
			nullptr,
		},

		/* —— Guides —— */
		{
			"guildjen",
			"Guides",
			"Guildjen",
			"Guildjen",
			"https://guildjen.com/",
			nullptr,
			nullptr,
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
		},

		/* —— Discord —— */
		{
			"discord_fractal",
			"Discord",
			"Fractal Training",
			"Mistlocked [FotM] — Fractal Training",
			"https://discord.com/invite/zxeVeSqpuS",
			nullptr,
			nullptr,
		},
		{
			"discord_raidcore",
			"Discord",
			"Raidcore",
			"Raidcore Discord",
			"https://discord.com/invite/raidcore",
			nullptr,
			nullptr,
		},
		{
			"discord_raidacademy",
			"Discord",
			"Raid Academy",
			"GW2 Raid Academy (NA)",
			"https://discord.com/invite/gw2ra",
			nullptr,
			nullptr,
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
