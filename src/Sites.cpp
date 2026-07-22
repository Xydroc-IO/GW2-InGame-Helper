#include "Sites.h"

#include "Settings.h"
#include "WikiBrowser.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
	/*
	 * Built-in sites. To add another:
	 *  1. Append a SiteDef in the right category block (keep categories contiguous).
	 *  2. Rebuild — no other wiring needed for basic browse/home.
	 *  3. Optional: set searchUrlPrefix/Suffix for toolbar search.
	 *
	 * Intentionally omitted (outdated / superseded / low quality): Hardstuck, Discretize, Lucky Noobs.
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

		/* —— Search —— */
		{
			"google",
			"Search",
			"Google",
			"Google",
			"https://www.google.com/",
			"https://www.google.com/search?q=",
			nullptr,
		},
		{
			"duckduckgo",
			"Search",
			"DuckDuckGo",
			"DuckDuckGo",
			"https://duckduckgo.com/",
			"https://duckduckgo.com/?q=",
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
		{
			"gw2forums",
			"Official",
			"Forums",
			"Guild Wars 2 Forums",
			"https://en-forum.guildwars2.com/",
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
		{
			"wiki_updates",
			"Wiki",
			"Game Updates",
			"Guild Wars 2 Wiki — Game Updates",
			"https://wiki.guildwars2.com/wiki/Game_updates",
			nullptr,
			nullptr,
		},
		{
			"wiki_legendaries",
			"Wiki",
			"Legendaries",
			"Guild Wars 2 Wiki — Legendary Equipment",
			"https://wiki.guildwars2.com/wiki/Legendary_equipment",
			nullptr,
			nullptr,
		},
		{
			"wiki_mounts",
			"Wiki",
			"Mounts",
			"Guild Wars 2 Wiki — Mounts",
			"https://wiki.guildwars2.com/wiki/Mount",
			nullptr,
			nullptr,
		},
		{
			"wiki_vault_easy",
			"Wiki",
			"Vault Easy Objectives",
			"Guild Wars 2 Wiki — Wizard's Vault Easy Objectives",
			"https://wiki.guildwars2.com/wiki/Wizard's_Vault/Easy_objectives",
			nullptr,
			nullptr,
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
		{
			"metabattle_pvp",
			"Builds",
			"MetaBattle PvP",
			"MetaBattle — PvP Builds",
			"https://metabattle.com/wiki/PvP_Builds",
			nullptr,
			nullptr,
		},
		{
			"metabattle_wvw",
			"Builds",
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
			"gw2eff_legendaries",
			"Tools",
			"Legendary Tracker",
			"gw2efficiency — Legendaries",
			"https://gw2efficiency.com/account/legendaries",
			nullptr,
			nullptr,
		},
		{
			"blishhud",
			"Tools",
			"Blish HUD",
			"Blish HUD",
			"https://blishhud.com/",
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
		{
			"killproof",
			"Tools",
			"KillProof",
			"KillProof.me",
			"https://killproof.me/",
			nullptr,
			nullptr,
		},
		{
			"wingman",
			"Tools",
			"Wingman",
			"GW2 Wingman — Log Stats",
			"https://gw2wingman.nevermindcreations.de/",
			nullptr,
			nullptr,
		},
		{
			"gw2bltc",
			"Tools",
			"GW2BLTC",
			"GW2BLTC — Trading Post",
			"https://www.gw2bltc.com/",
			nullptr,
			nullptr,
		},
		{
			"gw2treasures",
			"Tools",
			"GW2 Treasures",
			"GW2 Treasures",
			"https://gw2treasures.com/",
			nullptr,
			nullptr,
		},

		/* —— Cheat Sheets (built-in offline pages) —— */
		{
			"raidfood",
			"Cheat Sheets",
			"Raid Food",
			"Raid Food — Universal Seasoning Guide",
			"about:raid-food",
			nullptr,
			nullptr,
		},
		{
			"ubersaio",
			"Cheat Sheets",
			"Uber's All-In-One",
			"Uber's All-In-One — Waypoints",
			"about:ubers-aio",
			nullptr,
			nullptr,
		},
		{
			"raidutils",
			"Cheat Sheets",
			"Raid Utilities",
			"Raid Utilities — Oils, Stones & Crystals",
			"about:raid-utilities",
			nullptr,
			nullptr,
		},
		{
			"fractalcons",
			"Cheat Sheets",
			"Fractal Consumables",
			"Fractal Consumables — Potions & Agony",
			"about:fractal-consumables",
			nullptr,
			nullptr,
		},
		{
			"sigilsrunes",
			"Cheat Sheets",
			"Sigils & Runes",
			"Sigils & Runes — Common Role Picks",
			"about:sigils-runes",
			nullptr,
			nullptr,
		},
		{
			"relics",
			"Cheat Sheets",
			"Relics",
			"Relics — Picks by Role",
			"about:relics",
			nullptr,
			nullptr,
		},
		{
			"booncheck",
			"Cheat Sheets",
			"Boon Checklist",
			"Boon Checklist — Squad Coverage",
			"about:boon-checklist",
			nullptr,
			nullptr,
		},
		{
			"ccdefiance",
			"Cheat Sheets",
			"CC / Defiance",
			"CC / Defiance — Breakbar by Profession",
			"about:cc-defiance",
			nullptr,
			nullptr,
		},
		{
			"raidwings",
			"Cheat Sheets",
			"Raid Wings",
			"Raid Wings Overview",
			"about:raid-wings",
			nullptr,
			nullptr,
		},
		{
			"homegarden",
			"Cheat Sheets",
			"Home Garden",
			"Home Garden — Cultivated Herbs",
			"about:home-garden",
			nullptr,
			nullptr,
		},
		{
			"strikes",
			"Cheat Sheets",
			"Strike Missions",
			"Strike Missions Overview",
			"about:strike-missions",
			nullptr,
			nullptr,
		},
		{
			"fractalcm",
			"Cheat Sheets",
			"Fractal CM / T4",
			"Fractal CM / T4 List",
			"about:fractal-cm",
			nullptr,
			nullptr,
		},
		{
			"squadtmpl",
			"Cheat Sheets",
			"Squad Template",
			"Squad Template — 10-Man Roles",
			"about:squad-template",
			nullptr,
			nullptr,
		},
		{
			"stabcleanse",
			"Cheat Sheets",
			"Stability / Cleanse",
			"Stability / Cleanse — Squad Utility",
			"about:stability-cleanse",
			nullptr,
			nullptr,
		},
		{
			"matconv",
			"Cheat Sheets",
			"Material Conversions",
			"Material Conversions",
			"about:material-conversions",
			nullptr,
			nullptr,
		},
		{
			"legpaths",
			"Cheat Sheets",
			"Legendary Paths",
			"Legendary Short Paths",
			"about:legendary-paths",
			nullptr,
			nullptr,
		},
		{
			"mounts",
			"Cheat Sheets",
			"Mount Unlock",
			"Mount Unlock Checklist",
			"about:mount-unlock",
			nullptr,
			nullptr,
		},
		{
			"dailyweekly",
			"Cheat Sheets",
			"Daily / Weekly",
			"Daily / Weekly Checklist",
			"about:daily-weekly",
			nullptr,
			nullptr,
		},
		{
			"currencysinks",
			"Cheat Sheets",
			"Currency Sinks",
			"Currency Sinks",
			"about:currency-sinks",
			nullptr,
			nullptr,
		},
		{
			"ascendedstart",
			"Cheat Sheets",
			"Ascended Start",
			"Ascended Start — Gearing Path",
			"about:ascended-start",
			nullptr,
			nullptr,
		},
		{
			"portalspulls",
			"Cheat Sheets",
			"Portals / Pulls",
			"Portals / Pulls / Utility",
			"about:portals-pulls",
			nullptr,
			nullptr,
		},
		{
			"homestead",
			"Cheat Sheets",
			"Homestead",
			"Homestead Extras",
			"about:homestead",
			nullptr,
			nullptr,
		},
		{
			"wvwcons",
			"Cheat Sheets",
			"WvW Consumables",
			"WvW Consumables — Siege & Utility",
			"about:wvw-consumables",
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
		{
			"guildjen_lw",
			"Guides",
			"Living World",
			"Guildjen — Living World Guides",
			"https://guildjen.com/gw2-living-world-guides/",
			nullptr,
			nullptr,
		},
		{
			"mb_leveling",
			"Guides",
			"Leveling",
			"MetaBattle — Leveling Up",
			"https://metabattle.com/wiki/Guide:Leveling_Up",
			nullptr,
			nullptr,
		},
		{
			"mb_gold",
			"Guides",
			"Earn Gold",
			"MetaBattle — Ways to Earn Gold",
			"https://metabattle.com/wiki/Guide:Ways_to_Earn_Gold",
			nullptr,
			nullptr,
		},
		{
			"mb_griffon",
			"Guides",
			"Griffon Unlock",
			"MetaBattle — Unlock the Griffon",
			"https://metabattle.com/wiki/Guide:How_to_Unlock_the_Griffon_Mount",
			nullptr,
			nullptr,
		},
		{
			"mb_skyscale",
			"Guides",
			"Skyscale Unlock",
			"MetaBattle — Unlock the Skyscale",
			"https://metabattle.com/wiki/Guide:How_to_Unlock_the_Skyscale_Mount",
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
		{
			"gw2tldr_raids",
			"Guides",
			"TLDR Raids",
			"GW2 TLDR — Raid Encounters",
			"https://gw2tldr.com/raids",
			nullptr,
			nullptr,
		},
		{
			"gw2tldr_fractals",
			"Guides",
			"TLDR Fractals",
			"GW2 TLDR — Fractals",
			"https://gw2tldr.com/fractals",
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
		{
			"discord_overflow",
			"Discord",
			"Overflow Trading",
			"Overflow Trading Company",
			"https://discord.com/invite/gw2overflow",
			nullptr,
			nullptr,
		},
		{
			"discord_central",
			"Discord",
			"GW2 Central Hub",
			"GW2 Central Hub — Server Directory",
			"https://discord.com/invite/fnUha3QwmQ",
			nullptr,
			nullptr,
		},
	};

	constexpr int kSiteCount = static_cast<int>(sizeof(gSites) / sizeof(gSites[0]));
	int gActive = 0;

	constexpr int kMaxCategories = 32;
	const char* gCategories[kMaxCategories] = {};
	int gCategoryCount = -1;

	constexpr int kMaxFavorites = 48;
	char gFavoriteIds[kMaxFavorites][64] = {};
	int gFavoriteCount = 0;

	void EnsureCategories()
	{
		if (gCategoryCount >= 0)
			return;
		gCategoryCount = 0;
		const char* last = nullptr;
		for (int i = 0; i < kSiteCount && gCategoryCount < kMaxCategories; ++i)
		{
			const char* cat = gSites[i].category ? gSites[i].category : "";
			if (!last || std::strcmp(last, cat) != 0)
			{
				gCategories[gCategoryCount++] = cat;
				last = cat;
			}
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

const char* const* Sites::Categories(size_t* outCount)
{
	EnsureCategories();
	if (outCount)
		*outCount = static_cast<size_t>(gCategoryCount > 0 ? gCategoryCount : 0);
	return gCategories;
}

int Sites::CountInCategory(const char* category)
{
	if (!category)
		category = "";
	int n = 0;
	for (int i = 0; i < kSiteCount; ++i)
	{
		const char* cat = gSites[i].category ? gSites[i].category : "";
		if (std::strcmp(cat, category) == 0)
			++n;
	}
	return n;
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
	{
		if (query.empty())
			return "about:helper-home";
		return std::string("https://www.google.com/search?q=") + WikiBrowser::UrlEncode(query);
	}
	if (query.empty())
		return site.homeUrl ? site.homeUrl : "";
	if (!site.searchUrlPrefix)
		return std::string("https://www.google.com/search?q=") + WikiBrowser::UrlEncode(query);

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
	if (!id || !id[0])
		return -1;
	for (int i = 0; i < kSiteCount; ++i)
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
}

int Sites::BestMatchForUrl(const std::string& url)
{
	if (url.empty())
		return -1;

	if (url.rfind("about:", 0) == 0 || url.rfind("file:", 0) == 0)
	{
		for (int i = 0; i < kSiteCount; ++i)
		{
			const char* home = gSites[i].homeUrl;
			if (!home || !home[0])
				continue;
			if (std::strcmp(home, url.c_str()) == 0)
				return i;
		}
		/* file:///…/helper-home.html etc. */
		auto fileHit = [&](const char* needle, const char* id) -> int {
			if (url.find(needle) != std::string::npos)
				return IndexOfId(id);
			return -1;
		};
		int hit = fileHit("helper-home", "home");
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

	const std::string live = UrlHostPath(url, false);
	const std::string liveHost = UrlHostPath(url, true);
	if (live.empty())
		return -1;

	int best = -1;
	size_t bestLen = 0;
	int hostBest = -1;
	size_t hostBestLen = 0;

	for (int i = 0; i < kSiteCount; ++i)
	{
		const char* home = gSites[i].homeUrl;
		if (!home || !home[0])
			continue;
		if (std::strncmp(home, "http", 4) != 0)
			continue;

		const std::string homePath = UrlHostPath(home, false);
		const std::string homeHost = UrlHostPath(home, true);
		if (homePath.empty())
			continue;

		if (live == homePath || live.rfind(homePath + "/", 0) == 0 || live.rfind(homePath, 0) == 0)
		{
			if (homePath.size() > bestLen)
			{
				bestLen = homePath.size();
				best = i;
			}
		}
		else if (!homeHost.empty() && liveHost == homeHost)
		{
			if (homeHost.size() > hostBestLen)
			{
				hostBestLen = homeHost.size();
				hostBest = i;
			}
		}
	}

	return best >= 0 ? best : hostBest;
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
			Settings::SetDirty();
			return false;
		}
	}

	if (gFavoriteCount >= kMaxFavorites)
		return false;
	std::snprintf(gFavoriteIds[gFavoriteCount], sizeof(gFavoriteIds[gFavoriteCount]), "%s", id);
	++gFavoriteCount;
	Settings::SetDirty();
	return true;
}

int Sites::FavoriteCount()
{
	return gFavoriteCount;
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
	Settings::SetDirty();
	return true;
}
