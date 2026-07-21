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
	 *
	 * Intentionally omitted (outdated / superseded): Hardstuck, Discretize.
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
			"gw2news",
			"Official",
			"GW2 News",
			"Guild Wars 2 — News",
			"https://www.guildwars2.com/en/news/",
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
			"metabattle_ow",
			"Builds",
			"MetaBattle OW",
			"MetaBattle — Open World",
			"https://metabattle.com/wiki/Open_World",
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
		{
			"gw2skills",
			"Builds",
			"Gw2Skills Editor",
			"Gw2Skills.Net — Build Editor",
			"https://en.gw2skills.net/editor/",
			nullptr,
			nullptr,
		},

		/* —— PvP —— */
		{
			"metabattle_pvp",
			"PvP",
			"MetaBattle PvP",
			"MetaBattle — PvP Builds",
			"https://metabattle.com/wiki/PvP_Builds",
			nullptr,
			nullptr,
		},

		/* —— WvW —— */
		{
			"metabattle_wvw",
			"WvW",
			"MetaBattle WvW",
			"MetaBattle — WvW",
			"https://metabattle.com/wiki/WvW",
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
			"gw2timer_events",
			"Tools",
			"GW2Timer Events",
			"GW2Timer.com — Events",
			"https://gw2timer.com/",
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
			"gw2tldr_metas",
			"Tools",
			"Meta Timers",
			"GW2 TLDR — Meta Events",
			"https://gw2tldr.com/metas",
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
			"http://gw2mb.com/",
			nullptr,
			nullptr,
		},
		{
			"peuresearch",
			"Tools",
			"Peu Research Center",
			"Peu Research Center",
			"https://peuresearchcenter.com/index.html",
			nullptr,
			nullptr,
		},

		/* —— Guides —— */
		{
			"raidfood",
			"Guides",
			"Raid Food",
			"Raid Food — Universal Seasoning Guide",
			"about:raid-food",
			nullptr,
			nullptr,
		},
		{
			"luckynoobs",
			"Guides",
			"Lucky Noobs",
			"Lucky Noobs — Raids & Speedclears",
			"https://lucky-noobs.com/",
			nullptr,
			nullptr,
		},
		{
			"guildjen",
			"Guides",
			"Guildjen",
			"Guildjen",
			"https://guildjen.com/",
			nullptr,
			nullptr,
		},
		{
			"mukluk_fractals",
			"Guides",
			"Mukluk Fractals",
			"Mukluk Labs — Fractal Guides",
			"https://mukluklabs.com/gw2-fractal-guides",
			nullptr,
			nullptr,
		},
		{
			"gw2tldr",
			"Guides",
			"GW2 TLDR",
			"GW2 TLDR — Quick Reference",
			"https://gw2tldr.com/",
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
			"discord_official",
			"Discord",
			"Official GW2",
			"Official Guild Wars Community",
			"https://discord.com/invite/guildwars",
			nullptr,
			nullptr,
		},
		{
			"discord_community",
			"Discord",
			"Community",
			"GW2 Community (Reddit)",
			"https://discord.com/invite/guildwars2",
			nullptr,
			nullptr,
		},
		{
			"discord_snowcrows",
			"Discord",
			"Snowcrows",
			"Snowcrows Discord",
			"https://discord.com/invite/qTs63YH",
			nullptr,
			nullptr,
		},
		{
			"discord_metabattle",
			"Discord",
			"MetaBattle",
			"MetaBattle Discord",
			"https://discord.com/invite/0jdZWe6xdIy46Yp5",
			nullptr,
			nullptr,
		},
		{
			"discord_guildjen",
			"Discord",
			"Guildjen",
			"Guildjen Discord",
			"https://discord.com/invite/8NZNRvC",
			nullptr,
			nullptr,
		},
		{
			"discord_mukluk",
			"Discord",
			"Mukluk Labs",
			"Mukluk Labs Discord",
			"https://discord.com/invite/h5qS6Pk",
			nullptr,
			nullptr,
		},
		{
			"discord_aw2",
			"Discord",
			"Accessibility Wars",
			"Accessibility Wars Discord",
			"https://discord.com/invite/bKt2CdS8k3",
			nullptr,
			nullptr,
		},
		{
			"discord_skein",
			"Discord",
			"Skein Gang",
			"Skein Gang Discord",
			"https://discord.com/invite/q9E2WDcxXW",
			nullptr,
			nullptr,
		},
		{
			"discord_luckynoobs",
			"Discord",
			"Lucky Noobs",
			"Lucky Noobs Discord",
			"https://discord.com/invite/d2RkJBstDd",
			nullptr,
			nullptr,
		},
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
			"discord_raidacademy",
			"Discord",
			"Raid Academy",
			"GW2 Raid Academy (NA)",
			"https://discord.com/invite/gw2ra",
			nullptr,
			nullptr,
		},
		{
			"discord_uni",
			"Discord",
			"GW2 University",
			"Guild Wars 2 University",
			"https://discord.com/invite/sSzRQhWEEZ",
			nullptr,
			nullptr,
		},
		{
			"discord_crossroads",
			"Discord",
			"Crossroads Inn",
			"The Crossroads Inn — Raid Training (EU)",
			"https://discord.com/invite/hdhDE3v",
			nullptr,
			nullptr,
		},
		{
			"discord_rti",
			"Discord",
			"Raid Training EU",
			"Raid Training Initiative (EU)",
			"https://discord.com/invite/rti",
			nullptr,
			nullptr,
		},
		{
			"discord_pvp",
			"Discord",
			"Welcome to PvP",
			"Welcome to PvP — Training",
			"https://discord.com/invite/fD6VxyW",
			nullptr,
			nullptr,
		},
		{
			"discord_wvw_na",
			"Discord",
			"WvW NA Alliance",
			"NA Alliance Server (WvW)",
			"https://discord.com/invite/26k9WRZsua",
			nullptr,
			nullptr,
		},
		{
			"discord_wvw_eu",
			"Discord",
			"WvW EU Alliance",
			"EU Alliance Server (WvW)",
			"https://discord.com/invite/QPHHe8GZrD",
			nullptr,
			nullptr,
		},
		{
			"discord_fastfarming",
			"Discord",
			"Fast Farming",
			"Fast Farming Community Discord",
			"https://discord.com/invite/PTCp2tC",
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
