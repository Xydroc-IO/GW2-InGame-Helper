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
	 *  2. Map Browse section headers in UI.cpp when the category uses sub-sections.
	 *  3. Rebuild — no other wiring needed for basic browse/home.
	 *  4. Optional: set searchUrlPrefix/Suffix for toolbar search.
	 *  5. Run: python3 tools/validate_sites.py  (unique ids + UI id references).
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
		{
			"gemini",
			"Search",
			"Gemini",
			"Google Gemini",
			"https://gemini.google.com/app",
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
			"Easy Objectives",
			"Guild Wars 2 Wiki — Wizard's Vault Easy Objectives",
			"https://wiki.guildwars2.com/wiki/Wizard%27s_Vault/Easy_objectives",
			nullptr,
			nullptr,
		},

		/* —— Builds —— */
		{
			"snowcrows",
			"Builds",
			"SC Raid Builds",
			"Snowcrows — Raid Builds (All)",
			"https://snowcrows.com/builds/raids",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_ele",
			"Builds",
			"SC Raid Elementalist",
			"Snowcrows — Elementalist Raid Builds",
			"https://snowcrows.com/builds/raids/elementalist",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_mes",
			"Builds",
			"SC Raid Mesmer",
			"Snowcrows — Mesmer Raid Builds",
			"https://snowcrows.com/builds/raids/mesmer",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_nec",
			"Builds",
			"SC Raid Necromancer",
			"Snowcrows — Necromancer Raid Builds",
			"https://snowcrows.com/builds/raids/necromancer",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_eng",
			"Builds",
			"SC Raid Engineer",
			"Snowcrows — Engineer Raid Builds",
			"https://snowcrows.com/builds/raids/engineer",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_ran",
			"Builds",
			"SC Raid Ranger",
			"Snowcrows — Ranger Raid Builds",
			"https://snowcrows.com/builds/raids/ranger",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_thf",
			"Builds",
			"SC Raid Thief",
			"Snowcrows — Thief Raid Builds",
			"https://snowcrows.com/builds/raids/thief",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_gua",
			"Builds",
			"SC Raid Guardian",
			"Snowcrows — Guardian Raid Builds",
			"https://snowcrows.com/builds/raids/guardian",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_rev",
			"Builds",
			"SC Raid Revenant",
			"Snowcrows — Revenant Raid Builds",
			"https://snowcrows.com/builds/raids/revenant",
			nullptr,
			nullptr,
		},
		{
			"sc_raid_war",
			"Builds",
			"SC Raid Warrior",
			"Snowcrows — Warrior Raid Builds",
			"https://snowcrows.com/builds/raids/warrior",
			nullptr,
			nullptr,
		},
		{
			"sc_accessibuilds",
			"Builds",
			"SC AccessiBuilds",
			"Snowcrows — AccessiBuilds",
			"https://snowcrows.com/builds/accessibuilds",
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
			"sc_open_world",
			"Builds",
			"SC Open World",
			"Snowcrows — Open World Builds",
			"https://snowcrows.com/builds/open-world",
			nullptr,
			nullptr,
		},
		{
			"sc_pvp",
			"Builds",
			"SC PvP",
			"Snowcrows — PvP Builds",
			"https://snowcrows.com/builds/pvp",
			nullptr,
			nullptr,
		},
		{
			"sc_wvw",
			"Builds",
			"SC WvW",
			"Snowcrows — WvW Builds",
			"https://snowcrows.com/builds/wvw",
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
			"hs_arcdps",
			"Tools",
			"ArcDPS Guide",
			"Hardstuck — ArcDPS Setup Guide",
			"https://hardstuck.gg/gw2/guides/other/arcdps/",
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
			"sc_guides",
			"Guides",
			"Snowcrows Guides",
			"Snowcrows — Guides",
			"https://snowcrows.com/guides",
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
			"mb_pve_guides",
			"Guides",
			"PvE Guides Hub",
			"MetaBattle — PvE Guides",
			"https://metabattle.com/wiki/PvE_Guides",
			nullptr,
			nullptr,
		},
		{
			"mb_pvp_guides",
			"Guides",
			"PvP Guides Hub",
			"MetaBattle — PvP Guides",
			"https://metabattle.com/wiki/PvP_Guides",
			nullptr,
			nullptr,
		},
		{
			"mb_wvw_guides",
			"Guides",
			"WvW Guides Hub",
			"MetaBattle — WvW Guides",
			"https://metabattle.com/wiki/WvW_Guides",
			nullptr,
			nullptr,
		},
		/* Fractals (MetaBattle + community) */
		{
			"mb_intro_fractals",
			"Guides",
			"Intro to Fractals",
			"MetaBattle — Introduction to Fractals",
			"https://metabattle.com/wiki/Guide:Introduction_to_Fractals",
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
		/* Raid Wings (Guildjen) — https://guildjen.com/gw2-raid-guides/ */
		{
			"gj_raid_guides",
			"Guides",
			"Guildjen Raid Guides",
			"Guildjen — GW2 Raid Guides Hub",
			"https://guildjen.com/gw2-raid-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_intro_raiding",
			"Guides",
			"Intro to Raiding",
			"Guildjen — Beginner’s Guide to Guild Wars 2 Raids",
			"https://guildjen.com/beginners-guide-to-guild-wars-2-raids/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw1",
			"Guides",
			"Wing 1 Spirit Vale",
			"Guildjen — Spirit Vale Wing 1 Raid Guide",
			"https://guildjen.com/spirit-vale-wing-1-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw2",
			"Guides",
			"Wing 2 Salvation Pass",
			"Guildjen — Salvation Pass Wing 2 Raid Guide",
			"https://guildjen.com/salvation-pass-wing-2-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw3",
			"Guides",
			"Wing 3 Stronghold",
			"Guildjen — Stronghold of the Faithful Wing 3 Raid Guide",
			"https://guildjen.com/stronghold-of-the-faithful-wing-3-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw4",
			"Guides",
			"Wing 4 Bastion",
			"Guildjen — Bastion of the Penitent Wing 4 Raid Guide",
			"https://guildjen.com/bastion-of-the-penitent-wing-4-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw5",
			"Guides",
			"Wing 5 Hall of Chains",
			"Guildjen — Hall of Chains Wing 5 Raid Guide",
			"https://guildjen.com/hall-of-chains-wing-5-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw6",
			"Guides",
			"Wing 6 Mythwright",
			"Guildjen — Mythwright Gambit Wing 6 Raid Guide",
			"https://guildjen.com/mythwright-gambit-wing-6-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw7",
			"Guides",
			"Wing 7 Ahdashim",
			"Guildjen — The Key of Ahdashim Wing 7 Raid Guide",
			"https://guildjen.com/the-key-of-ahdashim-wing-7-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rw8",
			"Guides",
			"Wing 8 Mount Balrior",
			"Guildjen — Mount Balrior Wing 8 Raid Guide",
			"https://guildjen.com/mount-balrior-wing-8-guide/",
			nullptr,
			nullptr,
		},
		/* Raid Boss (Snow Crows per-encounter guides) — https://snowcrows.com/guides/raids */
		{
			"sc_raids_hub",
			"Guides",
			"SC Raids Hub",
			"Snow Crows — Raid Guides",
			"https://snowcrows.com/guides/raids",
			nullptr,
			nullptr,
		},
		{
			"sc_intro_squads",
			"Guides",
			"Intro to Squads",
			"Snow Crows — Introduction to Squads",
			"https://snowcrows.com/guides/getting-started/introduction-to-squads",
			nullptr,
			nullptr,
		},
		{
			"sc_squad_roles",
			"Guides",
			"Squad Roles",
			"Snow Crows — Understanding Squad Roles",
			"https://snowcrows.com/guides/getting-started/understanding-squad-roles",
			nullptr,
			nullptr,
		},
		{
			"sc_joining_squads",
			"Guides",
			"Joining Squads",
			"Snow Crows — Joining Squads",
			"https://snowcrows.com/guides/getting-started/joining-squads",
			nullptr,
			nullptr,
		},
		{
			"sc_w1_vale_guardian",
			"Guides",
			"Vale Guardian",
			"Snow Crows — Vale Guardian (W1)",
			"https://snowcrows.com/guides/raids/spirit-vale/vale-guardian",
			nullptr,
			nullptr,
		},
		{
			"sc_w1_spirit_woods",
			"Guides",
			"Spirit Woods",
			"Snow Crows — Spirit Woods (W1)",
			"https://snowcrows.com/guides/raids/spirit-vale/spirit-woods",
			nullptr,
			nullptr,
		},
		{
			"sc_w1_gorseval",
			"Guides",
			"Gorseval",
			"Snow Crows — Gorseval (W1)",
			"https://snowcrows.com/guides/raids/spirit-vale/gorseval",
			nullptr,
			nullptr,
		},
		{
			"sc_w1_sabetha",
			"Guides",
			"Sabetha",
			"Snow Crows — Sabetha (W1)",
			"https://snowcrows.com/guides/raids/spirit-vale/sabetha",
			nullptr,
			nullptr,
		},
		{
			"sc_w2_slothasor",
			"Guides",
			"Slothasor",
			"Snow Crows — Slothasor (W2)",
			"https://snowcrows.com/guides/raids/salvation-pass/slothasor",
			nullptr,
			nullptr,
		},
		{
			"sc_w2_bandit_trio",
			"Guides",
			"Bandit Trio",
			"Snow Crows — Bandit Trio (W2)",
			"https://snowcrows.com/guides/raids/salvation-pass/bandit-trio",
			nullptr,
			nullptr,
		},
		{
			"sc_w2_matthias",
			"Guides",
			"Matthias Gabrel",
			"Snow Crows — Matthias Gabrel (W2)",
			"https://snowcrows.com/guides/raids/salvation-pass/matthias-gabrel",
			nullptr,
			nullptr,
		},
		{
			"sc_w3_escort",
			"Guides",
			"Escort",
			"Snow Crows — Siege the Stronghold / Escort (W3)",
			"https://snowcrows.com/guides/raids/stronghold-faithful/siege-the-stronghold",
			nullptr,
			nullptr,
		},
		{
			"sc_w3_keep_construct",
			"Guides",
			"Keep Construct",
			"Snow Crows — Keep Construct (W3)",
			"https://snowcrows.com/guides/raids/stronghold-faithful/keep-construct",
			nullptr,
			nullptr,
		},
		{
			"sc_w3_twisted_castle",
			"Guides",
			"Twisted Castle",
			"Snow Crows — Twisted Castle (W3)",
			"https://snowcrows.com/guides/raids/stronghold-faithful/twisted-castle",
			nullptr,
			nullptr,
		},
		{
			"sc_w3_xera",
			"Guides",
			"Xera",
			"Snow Crows — Xera (W3)",
			"https://snowcrows.com/guides/raids/stronghold-faithful/xera",
			nullptr,
			nullptr,
		},
		{
			"sc_w4_cairn",
			"Guides",
			"Cairn",
			"Snow Crows — Cairn the Indomitable (W4)",
			"https://snowcrows.com/guides/raids/bastion-penitent/cairn-the-indomitable",
			nullptr,
			nullptr,
		},
		{
			"sc_w4_mursaat_overseer",
			"Guides",
			"Mursaat Overseer",
			"Snow Crows — Mursaat Overseer (W4)",
			"https://snowcrows.com/guides/raids/bastion-penitent/mursaat-overseer",
			nullptr,
			nullptr,
		},
		{
			"sc_w4_samarog",
			"Guides",
			"Samarog",
			"Snow Crows — Samarog (W4)",
			"https://snowcrows.com/guides/raids/bastion-penitent/samarog",
			nullptr,
			nullptr,
		},
		{
			"sc_w4_deimos",
			"Guides",
			"Deimos",
			"Snow Crows — Deimos (W4)",
			"https://snowcrows.com/guides/raids/bastion-penitent/deimos",
			nullptr,
			nullptr,
		},
		{
			"sc_w5_soulless_horror",
			"Guides",
			"Soulless Horror",
			"Snow Crows — Soulless Horror (W5)",
			"https://snowcrows.com/guides/raids/hall-chains/soulless-horror",
			nullptr,
			nullptr,
		},
		{
			"sc_w5_river_of_souls",
			"Guides",
			"River of Souls",
			"Snow Crows — River of Souls (W5)",
			"https://snowcrows.com/guides/raids/hall-chains/river-of-souls",
			nullptr,
			nullptr,
		},
		{
			"sc_w5_statues_of_grenth",
			"Guides",
			"Statues of Grenth",
			"Snow Crows — Statues of Grenth (W5)",
			"https://snowcrows.com/guides/raids/hall-chains/statues-of-grenth",
			nullptr,
			nullptr,
		},
		{
			"sc_w5_dhuum",
			"Guides",
			"Dhuum",
			"Snow Crows — Dhuum (W5)",
			"https://snowcrows.com/guides/raids/hall-chains/dhuum",
			nullptr,
			nullptr,
		},
		{
			"sc_w6_conjured_amalgamate",
			"Guides",
			"Conjured Amalgamate",
			"Snow Crows — Conjured Amalgamate (W6)",
			"https://snowcrows.com/guides/raids/mythwright-gambit/conjured-amalgamate",
			nullptr,
			nullptr,
		},
		{
			"sc_w6_twin_largos",
			"Guides",
			"Twin Largos",
			"Snow Crows — Twin Largos (W6)",
			"https://snowcrows.com/guides/raids/mythwright-gambit/twin-largos",
			nullptr,
			nullptr,
		},
		{
			"sc_w6_qadim",
			"Guides",
			"Qadim",
			"Snow Crows — Qadim (W6)",
			"https://snowcrows.com/guides/raids/mythwright-gambit/qadim",
			nullptr,
			nullptr,
		},
		{
			"sc_w6_sorting_appraisal",
			"Guides",
			"Sorting & Appraisal",
			"Snow Crows — Sorting and Appraisal (W6)",
			"https://snowcrows.com/guides/raids/mythwright-gambit/sorting-appraisal",
			nullptr,
			nullptr,
		},
		{
			"sc_w7_gate",
			"Guides",
			"Gates of Ahdashim",
			"Snow Crows — Gates of Ahdashim (W7)",
			"https://snowcrows.com/guides/raids/key-ahdashim/gates-of-ahdashim",
			nullptr,
			nullptr,
		},
		{
			"sc_w7_adina",
			"Guides",
			"Cardinal Adina",
			"Snow Crows — Cardinal Adina (W7)",
			"https://snowcrows.com/guides/raids/key-ahdashim/cardinal-adina",
			nullptr,
			nullptr,
		},
		{
			"sc_w7_sabir",
			"Guides",
			"Cardinal Sabir",
			"Snow Crows — Cardinal Sabir (W7)",
			"https://snowcrows.com/guides/raids/key-ahdashim/cardinal-sabir",
			nullptr,
			nullptr,
		},
		{
			"sc_w7_qadim_peerless",
			"Guides",
			"Qadim the Peerless",
			"Snow Crows — Qadim the Peerless (W7)",
			"https://snowcrows.com/guides/raids/key-ahdashim/qadim-the-peerless",
			nullptr,
			nullptr,
		},
		/* Strikes / Raid Encounters (MetaBattle) */
		{
			"mb_mai_trin",
			"Guides",
			"Aetherblade Hideout",
			"MetaBattle — Aetherblade Hideout (Mai Trin)",
			"https://metabattle.com/wiki/Guide:Aetherblade_Hideout_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_boneskinner",
			"Guides",
			"Boneskinner",
			"MetaBattle — Boneskinner",
			"https://metabattle.com/wiki/Guide:Boneskinner_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_cold_war",
			"Guides",
			"Cold War",
			"MetaBattle — Cold War",
			"https://metabattle.com/wiki/Guide:Cold_War_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_cosmic_obs",
			"Guides",
			"Cosmic Observatory",
			"MetaBattle — Cosmic Observatory (Dagda)",
			"https://metabattle.com/wiki/Guide:Cosmic_Observatory_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_forging_steel",
			"Guides",
			"Forging Steel",
			"MetaBattle — Forging Steel",
			"https://metabattle.com/wiki/Guide:Forging_Steel_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_fraenir",
			"Guides",
			"Fraenir of Jormag",
			"MetaBattle — Fraenir of Jormag",
			"https://metabattle.com/wiki/Guide:Fraenir_of_Jormag_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_icebrood",
			"Guides",
			"Icebrood Construct",
			"MetaBattle — Icebrood Construct (Shiverpeaks Pass)",
			"https://metabattle.com/wiki/Guide:Icebrood_Construct_(Shiverpeaks_Pass)_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_kaineng",
			"Guides",
			"Kaineng Overlook",
			"MetaBattle — Kaineng Overlook",
			"https://metabattle.com/wiki/Guide:Kaineng_Overlook_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_lions_court",
			"Guides",
			"Old Lion's Court",
			"MetaBattle — Old Lion's Court",
			"https://metabattle.com/wiki/Guide:Old_Lion%27s_Court_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_cerus",
			"Guides",
			"Temple of Febe",
			"MetaBattle — Temple of Febe (Cerus)",
			"https://metabattle.com/wiki/Guide:Temple_of_Febe_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_voice_claw",
			"Guides",
			"Voice and Claw",
			"MetaBattle — The Voice and the Claw",
			"https://metabattle.com/wiki/Guide:The_Voice_and_the_Claw_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_whisper",
			"Guides",
			"Whisper of Jormag",
			"MetaBattle — Whisper of Jormag",
			"https://metabattle.com/wiki/Guide:Whisper_of_Jormag_Raid_Encounter_Strategy_Guide",
			nullptr,
			nullptr,
		},
		{
			"mb_ankka",
			"Guides",
			"Xunlai Jade Junkyard",
			"MetaBattle — Xunlai Jade Junkyard (Ankka)",
			"https://metabattle.com/wiki/Guide:Xunlai_Jade_Junkyard_Raid_Encounter_Strategy_Guide",
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
		{
			"gw2tldr_dungeons",
			"Guides",
			"TLDR Dungeons",
			"GW2 TLDR — Dungeons",
			"https://gw2tldr.com/dungeons",
			nullptr,
			nullptr,
		},

		/* Guildjen Progress / Mounts / Fractals / Strikes / Rifts / PvP / WvW / W8 */
/* AUTO: Guildjen guide expansion — see tools/_gen note */
/* NEW_SITES_START */
		{
			"gj_new_player",
			"Guides",
			"New Player Roadmap",
			"Guildjen — New Players Guide / Roadmap to Endgame",
			"https://guildjen.com/new-players-guide-roadmap-to-endgame/",
			nullptr,
			nullptr,
		},
		{
			"gj_gold",
			"Guides",
			"Make Gold",
			"Guildjen — How to Make Gold in Guild Wars 2",
			"https://guildjen.com/how-to-make-gold-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_gem_store",
			"Guides",
			"Gem Store Essentials",
			"Guildjen — Best Items to Buy from the Gem Store",
			"https://guildjen.com/the-best-items-to-buy-from-the-gem-store/",
			nullptr,
			nullptr,
		},
		{
			"gj_wizards_vault",
			"Guides",
			"Wizard's Vault Essentials",
			"Guildjen — Best Items from the Wizard's Vault",
			"https://guildjen.com/the-best-items-to-get-from-the-wizards-vault/",
			nullptr,
			nullptr,
		},
		{
			"gj_roller_beetle",
			"Guides",
			"Roller Beetle Unlock",
			"Guildjen — Roller Beetle Mount Achievement Guide",
			"https://guildjen.com/roller-beetle-mount-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_siege_turtle",
			"Guides",
			"Siege Turtle Unlock",
			"Guildjen — Siege Turtle Mount Unlock Guide",
			"https://guildjen.com/siege-turtle-mount-unlock-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_fractals_hub",
			"Guides",
			"Guildjen Fractals",
			"Guildjen — Fractal Guides Hub",
			"https://guildjen.com/gw2-fractal-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_fractals_beginner",
			"Guides",
			"Fractals Beginner",
			"Guildjen — Fractals of the Mists Beginner's Guide",
			"https://guildjen.com/fractals-of-the-mists-beginners-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_aquatic",
			"Guides",
			"Aquatic Ruins",
			"Guildjen — Aquatic Ruins Fractal Guide",
			"https://guildjen.com/aquatic-ruins-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_lonely",
			"Guides",
			"Lonely Tower",
			"Guildjen — Lonely Tower Fractal Guide",
			"https://guildjen.com/the-lonely-tower-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_silent",
			"Guides",
			"Silent Surf",
			"Guildjen — Silent Surf Fractal Guide",
			"https://guildjen.com/silent-surf-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_kinfall",
			"Guides",
			"Kinfall",
			"Guildjen — Kinfall Fractal Guide",
			"https://guildjen.com/kinfall-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_sunqua",
			"Guides",
			"Sunqua Peak",
			"Guildjen — Sunqua Peak Fractal Guide",
			"https://guildjen.com/sunqua-peak-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_shattered",
			"Guides",
			"Shattered Observatory",
			"Guildjen — Shattered Observatory Fractal Guide",
			"https://guildjen.com/shattered-observatory-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_nightmare",
			"Guides",
			"Nightmare",
			"Guildjen — Nightmare Fractal Guide",
			"https://guildjen.com/nightmare-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_underground",
			"Guides",
			"Underground Facility",
			"Guildjen — Underground Facility Fractal Guide",
			"https://guildjen.com/underground-facility-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_sirens",
			"Guides",
			"Siren's Reef",
			"Guildjen — Siren's Reef Fractal Guide",
			"https://guildjen.com/sirens-reef-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_swampland",
			"Guides",
			"Swampland",
			"Guildjen — Swampland Fractal Guide",
			"https://guildjen.com/swampland-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_aetherblade",
			"Guides",
			"Aetherblade",
			"Guildjen — Aetherblade Fractal Guide",
			"https://guildjen.com/aetherblade-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_deepstone",
			"Guides",
			"Deepstone",
			"Guildjen — Deepstone Fractal Guide",
			"https://guildjen.com/deepstone-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_mai_trin",
			"Guides",
			"Captain Mai Trin Boss",
			"Guildjen — Captain Mai Trin Boss Fractal Guide",
			"https://guildjen.com/captain-mai-trin-boss-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_chaos",
			"Guides",
			"Chaos",
			"Guildjen — Chaos Fractal Guide",
			"https://guildjen.com/chaos-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_cliffside",
			"Guides",
			"Cliffside",
			"Guildjen — Cliffside Fractal Guide",
			"https://guildjen.com/cliffside-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_molten_boss",
			"Guides",
			"Molten Boss",
			"Guildjen — Molten Boss Fractal Guide",
			"https://guildjen.com/molten-boss-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_molten_furnace",
			"Guides",
			"Molten Furnace",
			"Guildjen — Molten Furnace Fractal Guide",
			"https://guildjen.com/molten-furnace-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_thaumanova",
			"Guides",
			"Thaumanova Reactor",
			"Guildjen — Thaumanova Reactor Fractal Guide",
			"https://guildjen.com/thaumanova-reactor-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_urban",
			"Guides",
			"Urban Battleground",
			"Guildjen — Urban Battleground Fractal Guide",
			"https://guildjen.com/urban-battleground-fractal/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_volcanic",
			"Guides",
			"Volcanic",
			"Guildjen — Volcanic Fractal Guide",
			"https://guildjen.com/volcanic-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_uncategorized",
			"Guides",
			"Uncategorized",
			"Guildjen — Uncategorized Fractal Guide",
			"https://guildjen.com/uncategorized-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_twilight",
			"Guides",
			"Twilight Oasis",
			"Guildjen — Twilight Oasis Fractal Guide",
			"https://guildjen.com/twilight-oasis-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_snowblind",
			"Guides",
			"Snowblind",
			"Guildjen — Snowblind Fractal Guide",
			"https://guildjen.com/snowblind-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_frac_solid",
			"Guides",
			"Solid Ocean",
			"Guildjen — Solid Ocean Fractal Guide",
			"https://guildjen.com/solid-ocean-fractal-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_harvest_temple",
			"Guides",
			"Harvest Temple",
			"Guildjen — Harvest Temple Strike / Raid Guide",
			"https://guildjen.com/harvest-temple-raid-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_rifts",
			"Guides",
			"Rifts & Convergences",
			"Guildjen — Rift Hunting and Convergences Guide",
			"https://guildjen.com/rift-hunting-and-convergences-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_pvp_hub",
			"Guides",
			"Guildjen PvP Guides",
			"Guildjen — PvP Guides Hub",
			"https://guildjen.com/gw2-pvp-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_pvp_beginner",
			"Guides",
			"PvP Beginner",
			"Guildjen — Beginner's Guide to GW2 PvP",
			"https://guildjen.com/beginners-guide-to-gw2-pvp/",
			nullptr,
			nullptr,
		},
		{
			"gj_wvw_beginner",
			"Guides",
			"WvW Beginner",
			"Guildjen — Beginner's Guide to World vs World",
			"https://guildjen.com/beginners-guide-to-guild-wars-2-world-vs-world/",
			nullptr,
			nullptr,
		},
		{
			"gj_w8_decima",
			"Guides",
			"Decima",
			"Guildjen — Decima (Mount Balrior W8)",
			"https://guildjen.com/mount-balrior-wing-8-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_w8_greer",
			"Guides",
			"Greer",
			"Guildjen — Greer (Mount Balrior W8)",
			"https://guildjen.com/mount-balrior-wing-8-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_w8_ura",
			"Guides",
			"Ura",
			"Guildjen — Ura (Mount Balrior W8)",
			"https://guildjen.com/mount-balrior-wing-8-guide/",
			nullptr,
			nullptr,
		},
		{
			"mb_w8_balrior",
			"Guides",
			"Mount Balrior (MB)",
			"MetaBattle — Raid Wing 8 Mount Balrior",
			"https://metabattle.com/wiki/Guide:Raid_Wing_8_-_Mount_Balrior",
			nullptr,
			nullptr,
		},
/* ACHIEVEMENTS_START */
		{
			"gj_ach_hub",
			"Guides",
			"Achievements Hub",
			"Guildjen — Achievement Guides",
			"https://guildjen.com/gw2-achievements/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_hub",
			"Guides",
			"Living World Hub",
			"Guildjen — Living World Guides",
			"https://guildjen.com/gw2-living-world-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_flame",
			"Guides",
			"Flame and Frost",
			"Guildjen — Flame and Frost",
			"https://guildjen.com/flame-and-frost-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_sky",
			"Guides",
			"Sky Pirates",
			"Guildjen — Sky Pirates",
			"https://guildjen.com/sky-pirates-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_clock",
			"Guides",
			"Clockwork Chaos",
			"Guildjen — Clockwork Chaos",
			"https://guildjen.com/clockwork-chaos-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_tower",
			"Guides",
			"Tower of Nightmares",
			"Guildjen — Tower of Nightmares",
			"https://guildjen.com/the-tower-of-nightmares-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_battle",
			"Guides",
			"Battle for Lion's Arch",
			"Guildjen — Battle for Lion's Arch",
			"https://guildjen.com/the-battle-for-lions-arch-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_marionette",
			"Guides",
			"Twisted Marionette",
			"Guildjen — Twisted Marionette",
			"https://guildjen.com/the-twisted-marionette-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_memory",
			"Guides",
			"Lion's Memory",
			"Guildjen — Lion's Memory",
			"https://guildjen.com/lions-memory-old-lions-arch-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_gates",
			"Guides",
			"Gates of Maguuma",
			"Guildjen — Gates of Maguuma",
			"https://guildjen.com/gates-of-maguuma-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_entangle",
			"Guides",
			"Entanglement",
			"Guildjen — Entanglement",
			"https://guildjen.com/entanglement-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_reach1",
			"Guides",
			"Dragon's Reach Part 1",
			"Guildjen — Dragon's Reach Part 1",
			"https://guildjen.com/the-dragons-reach-part-1-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_reach2",
			"Guides",
			"Dragon's Reach Part 2",
			"Guildjen — Dragon's Reach Part 2",
			"https://guildjen.com/the-dragons-reach-part-2-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_echoes",
			"Guides",
			"Echoes of the Past",
			"Guildjen — Echoes of the Past",
			"https://guildjen.com/echoes-of-the-past-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_tangled",
			"Guides",
			"Tangled Paths",
			"Guildjen — Tangled Paths",
			"https://guildjen.com/tangled-paths-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_seeds",
			"Guides",
			"Seeds of Truth",
			"Guildjen — Seeds of Truth",
			"https://guildjen.com/seeds-of-truth-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_point",
			"Guides",
			"Point of No Return",
			"Guildjen — Point of No Return",
			"https://guildjen.com/point-of-no-return-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_drytop",
			"Guides",
			"Dry Top",
			"Guildjen — Dry Top",
			"https://guildjen.com/dry-top-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_silver",
			"Guides",
			"Silverwastes",
			"Guildjen — Silverwastes",
			"https://guildjen.com/the-silverwastes-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_lumi",
			"Guides",
			"Luminescent Armor",
			"Guildjen — Luminescent Armor",
			"https://guildjen.com/luminescent-armor-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_mawdrey",
			"Guides",
			"Mawdrey",
			"Guildjen — Mawdrey",
			"https://guildjen.com/mawdrey-complete-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_shadows",
			"Guides",
			"Out of the Shadows",
			"Guildjen — Out of the Shadows",
			"https://guildjen.com/out-of-the-shadows-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_rising",
			"Guides",
			"Rising Flames",
			"Guildjen — Rising Flames",
			"https://guildjen.com/rising-flames-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_crack",
			"Guides",
			"Crack in the Ice",
			"Guildjen — Crack in the Ice",
			"https://guildjen.com/a-crack-in-the-ice-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_snake",
			"Guides",
			"Head of the Snake",
			"Guildjen — Head of the Snake",
			"https://guildjen.com/the-head-of-the-snake-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_flash",
			"Guides",
			"Flashpoint",
			"Guildjen — Flashpoint",
			"https://guildjen.com/flashpoint-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_path",
			"Guides",
			"One Path Ends",
			"Guildjen — One Path Ends",
			"https://guildjen.com/one-path-ends-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_daybreak",
			"Guides",
			"Daybreak",
			"Guildjen — Daybreak",
			"https://guildjen.com/daybreak-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_bug",
			"Guides",
			"Bug in the System",
			"Guildjen — Bug in the System",
			"https://guildjen.com/a-bug-in-the-system-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_lich",
			"Guides",
			"Long Live the Lich",
			"Guildjen — Long Live the Lich",
			"https://guildjen.com/long-live-the-lich-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_star",
			"Guides",
			"A Star to Guide Us",
			"Guildjen — A Star to Guide Us",
			"https://guildjen.com/a-star-to-guide-us-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_all",
			"Guides",
			"All or Nothing",
			"Guildjen — All or Nothing",
			"https://guildjen.com/all-or-nothing-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_war",
			"Guides",
			"War Eternal",
			"Guildjen — War Eternal",
			"https://guildjen.com/war-eternal-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_elegy",
			"Guides",
			"Elegy & Requiem Armor",
			"Guildjen — Elegy & Requiem Armor",
			"https://guildjen.com/elegy-and-requiem-armor-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_bound",
			"Guides",
			"Bound by Blood",
			"Guildjen — Bound by Blood",
			"https://guildjen.com/prologue-bound-by-blood-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_whisper",
			"Guides",
			"Whisper in the Dark",
			"Guildjen — Whisper in the Dark",
			"https://guildjen.com/whisper-in-the-dark-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_shadow_ice",
			"Guides",
			"Shadow in the Ice",
			"Guildjen — Shadow in the Ice",
			"https://guildjen.com/shadow-in-the-ice-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_visions",
			"Guides",
			"Visions of the Past",
			"Guildjen — Visions of the Past",
			"https://guildjen.com/visions-of-the-past-steel-and-fire-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_quarter",
			"Guides",
			"No Quarter",
			"Guildjen — No Quarter",
			"https://guildjen.com/no-quarter-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_jormag",
			"Guides",
			"Jormag Rising",
			"Guildjen — Jormag Rising",
			"https://guildjen.com/jormag-rising-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_champs",
			"Guides",
			"Champions",
			"Guildjen — Champions",
			"https://guildjen.com/champions-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_lw_drm",
			"Guides",
			"Dragon Response Missions",
			"Guildjen — Dragon Response Missions",
			"https://guildjen.com/drm-guides-dragon-response-missions/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_hot_hub",
			"Guides",
			"HoT Hub",
			"Guildjen — Heart of Thorns Guides",
			"https://guildjen.com/gw2-heart-of-thorns-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_hot_act1",
			"Guides",
			"HoT Act 1",
			"Guildjen — Heart of Thorns Act 1 Story Achievements",
			"https://guildjen.com/heart-of-thorns-act-1-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_hot_act2",
			"Guides",
			"HoT Act 2",
			"Guildjen — Heart of Thorns Act 2 Story Achievements",
			"https://guildjen.com/heart-of-thorns-act-2-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_hot_act3",
			"Guides",
			"HoT Act 3",
			"Guildjen — Heart of Thorns Act 3 Story Achievements",
			"https://guildjen.com/heart-of-thorns-act-3-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_hot_act4",
			"Guides",
			"HoT Act 4",
			"Guildjen — Heart of Thorns Act 4 Story Achievements",
			"https://guildjen.com/heart-of-thorns-act-4-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_hot_verdant",
			"Guides",
			"Verdant Brink",
			"Guildjen — Verdant Brink Achievements",
			"https://guildjen.com/verdant-brink-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_hot_auric",
			"Guides",
			"Auric Basin",
			"Guildjen — Auric Basin Achievements",
			"https://guildjen.com/auric-basin-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_hub",
			"Guides",
			"PoF Hub",
			"Guildjen — Path of Fire Guides",
			"https://guildjen.com/gw2-path-of-fire-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_act1",
			"Guides",
			"PoF Act 1",
			"Guildjen — Path of Fire Act 1 Story Achievements",
			"https://guildjen.com/path-of-fire-act-1-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_act2",
			"Guides",
			"PoF Act 2",
			"Guildjen — Path of Fire Act 2 Story Achievements",
			"https://guildjen.com/path-of-fire-act-2-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_act3",
			"Guides",
			"PoF Act 3",
			"Guildjen — Path of Fire Act 3 Story Achievements",
			"https://guildjen.com/path-of-fire-act-3-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_crystal",
			"Guides",
			"Crystal Oasis",
			"Guildjen — Crystal Oasis",
			"https://guildjen.com/crystal-oasis-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_highlands",
			"Guides",
			"Desert Highlands",
			"Guildjen — Desert Highlands",
			"https://guildjen.com/desert-highlands-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_elon",
			"Guides",
			"Elon Riverlands",
			"Guildjen — Elon Riverlands",
			"https://guildjen.com/elon-riverlands-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_desolation",
			"Guides",
			"The Desolation",
			"Guildjen — The Desolation",
			"https://guildjen.com/the-desolation-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_pof_vabbi",
			"Guides",
			"Domain of Vabbi",
			"Guildjen — Domain of Vabbi",
			"https://guildjen.com/domain-of-vabbi-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_hub",
			"Guides",
			"EoD Hub",
			"Guildjen — End of Dragons Guides",
			"https://guildjen.com/gw2-end-of-dragons-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_act1",
			"Guides",
			"EoD Act 1",
			"Guildjen — End of Dragons Act 1 Story Achievements",
			"https://guildjen.com/end-of-dragons-act-1-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_act2",
			"Guides",
			"EoD Act 2",
			"Guildjen — End of Dragons Act 2 Story Achievements",
			"https://guildjen.com/end-of-dragons-act-2-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_act3",
			"Guides",
			"EoD Act 3",
			"Guildjen — End of Dragons Act 3 Story Achievements",
			"https://guildjen.com/end-of-dragons-act-3-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_act4",
			"Guides",
			"EoD Act 4",
			"Guildjen — End of Dragons Act 4 Story Achievements",
			"https://guildjen.com/end-of-dragons-act-4-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_act5",
			"Guides",
			"EoD Act 5",
			"Guildjen — End of Dragons Act 5 Story Achievements",
			"https://guildjen.com/end-of-dragons-act-5-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_haze",
			"Guides",
			"Delve into the Haze",
			"Guildjen — Delve into the Haze",
			"https://guildjen.com/delve-into-the-haze-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_within",
			"Guides",
			"What Lies Within",
			"Guildjen — What Lies Within",
			"https://guildjen.com/what-lies-within-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_seitung",
			"Guides",
			"Seitung Province",
			"Guildjen — Seitung Province",
			"https://guildjen.com/seitung-province-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_kaineng",
			"Guides",
			"New Kaineng City",
			"Guildjen — New Kaineng City",
			"https://guildjen.com/new-kaineng-city-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_echovald",
			"Guides",
			"Echovald Wilds",
			"Guildjen — Echovald Wilds",
			"https://guildjen.com/the-echovald-wilds-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_dragons_end",
			"Guides",
			"Dragon's End",
			"Guildjen — Dragon's End",
			"https://guildjen.com/dragons-end-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_arborstone",
			"Guides",
			"Arborstone",
			"Guildjen — Arborstone",
			"https://guildjen.com/arborstone-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_spiritual",
			"Guides",
			"Spiritual Childcare",
			"Guildjen — Spiritual Childcare",
			"https://guildjen.com/spiritual-childcare-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_foxes",
			"Guides",
			"Little Foxes",
			"Guildjen — Little Foxes",
			"https://guildjen.com/little-foxes-in-the-big-city-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_shrine",
			"Guides",
			"Shrine Guardians",
			"Guildjen — Shrine Guardians",
			"https://guildjen.com/what-did-the-shrine-guardians-say-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_dead",
			"Guides",
			"Dead Play",
			"Guildjen — Dead Play",
			"https://guildjen.com/dead-play-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_eod_infants",
			"Guides",
			"Playing with Infants",
			"Guildjen — Playing with Infants",
			"https://guildjen.com/playing-with-the-infants-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_hub",
			"Guides",
			"SotO Hub",
			"Guildjen — Secrets of the Obscure Guides",
			"https://guildjen.com/gw2-secrets-of-the-obscure-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_story",
			"Guides",
			"SotO Story",
			"Guildjen — SotO Story",
			"https://guildjen.com/secrets-of-the-obscure-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_veil",
			"Guides",
			"Through the Veil",
			"Guildjen — Through the Veil",
			"https://guildjen.com/secrets-of-the-obscure-through-the-veil-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_dreams",
			"Guides",
			"Realm of Dreams",
			"Guildjen — Realm of Dreams",
			"https://guildjen.com/secrets-of-the-obscure-the-realm-of-dreams-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_midnight",
			"Guides",
			"Midnight King",
			"Guildjen — Midnight King",
			"https://guildjen.com/secrets-of-the-obscure-the-midnight-king-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_skywatch",
			"Guides",
			"Skywatch Archipelago",
			"Guildjen — Skywatch Archipelago",
			"https://guildjen.com/skywatch-archipelago-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_amnytas",
			"Guides",
			"Amnytas",
			"Guildjen — Amnytas",
			"https://guildjen.com/amnytas-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_tower",
			"Guides",
			"Wizard's Tower",
			"Guildjen — Wizard's Tower",
			"https://guildjen.com/the-wizards-tower-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_heitor",
			"Guides",
			"Heitor's Territory",
			"Guildjen — Heitor's Territory",
			"https://guildjen.com/inner-nayos-heitors-territory-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_nyedra",
			"Guides",
			"Nyedra Surrounds",
			"Guildjen — Nyedra Surrounds",
			"https://guildjen.com/inner-nayos-nyedra-surrounds-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_zakiros",
			"Guides",
			"Citadel of Zakiros",
			"Guildjen — Citadel of Zakiros",
			"https://guildjen.com/inner-nayos-citadel-of-zakiros-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_unlock_tower",
			"Guides",
			"Unlock Wizard's Tower Meta",
			"Guildjen — Unlock Wizard's Tower Meta",
			"https://guildjen.com/unlocking-the-wizards-tower-meta-event-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_shrine",
			"Guides",
			"Rise and Shrine",
			"Guildjen — Rise and Shrine",
			"https://guildjen.com/rise-and-shrine-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_foxes",
			"Guides",
			"Fantastic Flying Foxes",
			"Guildjen — Fantastic Flying Foxes",
			"https://guildjen.com/fantastic-flying-foxes-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_defense",
			"Guides",
			"Defense of Amnytas",
			"Guildjen — Defense of Amnytas",
			"https://guildjen.com/the-defense-of-amnytas-meta-event-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_soto_lantern",
			"Guides",
			"Waylit Lantern",
			"Guildjen — Waylit Lantern",
			"https://guildjen.com/the-waylit-lantern-achievement-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_hub",
			"Guides",
			"JW Hub",
			"Guildjen — Janthir Wilds Guides",
			"https://guildjen.com/gw2-janthir-wilds-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_story",
			"Guides",
			"JW Story",
			"Guildjen — JW Story",
			"https://guildjen.com/janthir-wilds-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_godspawn",
			"Guides",
			"Godspawn",
			"Guildjen — Godspawn",
			"https://guildjen.com/janthir-wilds-godspawn-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_repentance",
			"Guides",
			"Repentance",
			"Guildjen — Repentance",
			"https://guildjen.com/janthir-wilds-repentance-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_absolution",
			"Guides",
			"Absolution",
			"Guildjen — Absolution",
			"https://guildjen.com/janthir-wilds-absolution-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_lowland",
			"Guides",
			"Lowland Shore",
			"Guildjen — Lowland Shore",
			"https://guildjen.com/lowland-shore-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_syntri",
			"Guides",
			"Janthir Syntri",
			"Guildjen — Janthir Syntri",
			"https://guildjen.com/janthir-syntri-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_mistburned",
			"Guides",
			"Mistburned Barrens",
			"Guildjen — Mistburned Barrens",
			"https://guildjen.com/mistburned-barrens-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_bava",
			"Guides",
			"Bava Nisos",
			"Guildjen — Bava Nisos",
			"https://guildjen.com/bava-nisos-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_falling",
			"Guides",
			"Falling Star Quest",
			"Guildjen — Falling Star Quest",
			"https://guildjen.com/the-falling-star-quest-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_jw_wisp",
			"Guides",
			"Wayward Wisp Quest",
			"Guildjen — Wayward Wisp Quest",
			"https://guildjen.com/wayward-wisp-quest-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_hub",
			"Guides",
			"VoE Hub",
			"Guildjen — Visions of Eternity Guides",
			"https://guildjen.com/visions-of-eternity-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_story",
			"Guides",
			"VoE Story",
			"Guildjen — VoE Story",
			"https://guildjen.com/visions-of-eternity-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_only_way",
			"Guides",
			"The Only Way",
			"Guildjen — The Only Way",
			"https://guildjen.com/the-only-way-story-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_shipwreck",
			"Guides",
			"Shipwreck Strand",
			"Guildjen — Shipwreck Strand",
			"https://guildjen.com/shipwreck-strand-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_starlit",
			"Guides",
			"Starlit Weald",
			"Guildjen — Starlit Weald",
			"https://guildjen.com/starlit-weald-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_garden",
			"Guides",
			"Eternity's Garden",
			"Guildjen — Eternity's Garden",
			"https://guildjen.com/eternitys-garden-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_backpiece",
			"Guides",
			"Emergent Metamorphosis",
			"Guildjen — Emergent Metamorphosis",
			"https://guildjen.com/emergent-metamorphosis-backpiece-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_voe_culture",
			"Guides",
			"Castoran Culture",
			"Guildjen — Castoran Culture",
			"https://guildjen.com/castoran-culture-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_fest_hub",
			"Guides",
			"Festivals Hub",
			"Guildjen — Festival Guides",
			"https://guildjen.com/gw2-festival-guides/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_fest_bash",
			"Guides",
			"Dragon Bash",
			"Guildjen — Dragon Bash Festival",
			"https://guildjen.com/dragon-bash-festival-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_fest_sab",
			"Guides",
			"Super Adventure Box",
			"Guildjen — Super Adventure Box Festival",
			"https://guildjen.com/super-adventure-box-festival-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_fest_lunar",
			"Guides",
			"Lunar New Year",
			"Guildjen — Lunar New Year Festival",
			"https://guildjen.com/lunar-new-year-festival-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_fest_winter",
			"Guides",
			"Wintersday",
			"Guildjen — Wintersday Festival",
			"https://guildjen.com/wintersday-festival-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_fest_halloween",
			"Guides",
			"Halloween",
			"Guildjen — Halloween Festival",
			"https://guildjen.com/halloween-festival-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_fest_four",
			"Guides",
			"Four Winds",
			"Guildjen — Four Winds Festival",
			"https://guildjen.com/festival-of-the-four-winds-achievements-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_side_hub",
			"Guides",
			"Side Stories Hub",
			"Guildjen — Side Stories",
			"https://guildjen.com/category/gw2/gw2-achievements/gw2-side-stories/",
			nullptr,
			nullptr,
		},
		{
			"gj_ach_side_portal_tome",
			"Guides",
			"Wizard's Portal Tome",
			"Guildjen — Wizard's Portal Tome (Work Hard for Now)",
			"https://guildjen.com/wizards-portal-tome-guide-work-hard-for-now/",
			nullptr,
			nullptr,
		},
/* NEW_SITES_END */

		/* Jumping Puzzles (Guildjen) */
		{
			"gj_jp_hub",
			"Guides",
			"Jumping Puzzles Hub",
			"Guildjen — Jumping Puzzle Guides",
			"https://guildjen.com/category/gw2/gw2-guides/gw2-jps/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_the_long_way_around",
			"Guides",
			"The Long Way Around",
			"Guildjen — The Long Way Around Jumping Puzzle Guide",
			"https://guildjen.com/the-long-way-around-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_trolls_revenge",
			"Guides",
			"Troll’s Revenge",
			"Guildjen — Troll’s Revenge Jumping Puzzle Guide",
			"https://guildjen.com/trolls-revenge-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_skipping_stones",
			"Guides",
			"Skipping Stones",
			"Guildjen — Skipping Stones Jumping Puzzle Guide",
			"https://guildjen.com/skipping-stones-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_urmaugs_secret",
			"Guides",
			"Urmaug’s Secret",
			"Guildjen — Urmaug’s Secret Jumping Puzzle Guide",
			"https://guildjen.com/urmaugs-secret-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_swashbucklers_cove",
			"Guides",
			"Swashbuckler’s Cove",
			"Guildjen — Swashbuckler’s Cove Jumping Puzzle Guide",
			"https://guildjen.com/swashbucklers-cove-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_skip_up_the_volcano",
			"Guides",
			"Skip Up The Volcano",
			"Guildjen — Skip Up The Volcano Jumping Puzzle Guide",
			"https://guildjen.com/skip-up-the-volcano-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_searing_ascent",
			"Guides",
			"Searing Ascent",
			"Guildjen — Searing Ascent Jumping Puzzle Guide",
			"https://guildjen.com/searing-ascent-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_abaddons_ascent",
			"Guides",
			"Abaddon’s Ascent",
			"Guildjen — Abaddon’s Ascent Jumping Puzzle Guide",
			"https://guildjen.com/abaddons-ascent-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_hidden_garden",
			"Guides",
			"Hidden Garden",
			"Guildjen — Hidden Garden Jumping Puzzle Guide",
			"https://guildjen.com/hidden-garden-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_weyandts_revenge",
			"Guides",
			"Weyandt’s Revenge",
			"Guildjen — Weyandt’s Revenge Jumping Puzzle Guide",
			"https://guildjen.com/weyandts-revenge-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_traversing_the_titan",
			"Guides",
			"Traversing the Titan",
			"Guildjen — Traversing the Titan Jumping Puzzle Guide",
			"https://guildjen.com/traversing-the-titan-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_demongrub_pits",
			"Guides",
			"Demongrub Pits",
			"Guildjen — Demongrub Pits Jumping Puzzle Guide",
			"https://guildjen.com/demongrub-pits-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_viziers_tower",
			"Guides",
			"Vizier’s Tower",
			"Guildjen — Vizier’s Tower Jumping Puzzle Guide",
			"https://guildjen.com/viziers-tower-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_the_collapsed_observatory",
			"Guides",
			"The Collapsed Observatory",
			"Guildjen — The Collapsed Observatory Jumping Puzzle Guide",
			"https://guildjen.com/the-collapsed-observatory-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_spelunkers_delve",
			"Guides",
			"Spelunker’s Delve",
			"Guildjen — Spelunker’s Delve Jumping Puzzle Guide",
			"https://guildjen.com/spelunkers-delve-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_dark_reverie",
			"Guides",
			"Dark Reverie",
			"Guildjen — Dark Reverie Jumping Puzzle Guide",
			"https://guildjen.com/dark-reverie-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_pig_iron_quarry",
			"Guides",
			"Pig Iron Quarry",
			"Guildjen — Pig Iron Quarry Jumping Puzzle Guide",
			"https://guildjen.com/pig-iron-quarry-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_crazes_folly",
			"Guides",
			"Craze’s Folly",
			"Guildjen — Craze’s Folly Jumping Puzzle Guide",
			"https://guildjen.com/crazes-folly-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_chaos_crystal_cavern",
			"Guides",
			"Chaos Crystal Cavern",
			"Guildjen — Chaos Crystal Cavern Jumping Puzzle Guide",
			"https://guildjen.com/chaos-crystal-cavern-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_hexfoundry_unhinged",
			"Guides",
			"Hexfoundry Unhinged",
			"Guildjen — Hexfoundry Unhinged Jumping Puzzle Guide",
			"https://guildjen.com/hexfoundry-unhinged-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_conundrum_cubed",
			"Guides",
			"Conundrum Cubed",
			"Guildjen — Conundrum Cubed Jumping Puzzle Guide",
			"https://guildjen.com/conundrum-cubed-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_buried_archives",
			"Guides",
			"Buried Archives",
			"Guildjen — Buried Archives Jumping Puzzle Guide",
			"https://guildjen.com/buried-archives-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_spekkss_laboratory",
			"Guides",
			"Spekks’s Laboratory",
			"Guildjen — Spekks’s Laboratory Jumping Puzzle Guide",
			"https://guildjen.com/spekkss-laboratory-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_behem_gauntlet",
			"Guides",
			"Behem Gauntlet",
			"Guildjen — Behem Gauntlet Jumping Puzzle Guide",
			"https://guildjen.com/behem-gauntlet-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_only_zuhl",
			"Guides",
			"Only Zuhl",
			"Guildjen — Only Zuhl Jumping Puzzle Guide",
			"https://guildjen.com/only-zuhl-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_not_so_secret",
			"Guides",
			"Not So Secret",
			"Guildjen — Not So Secret Jumping Puzzle Guide",
			"https://guildjen.com/not-so-secret-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_branded_mine",
			"Guides",
			"Branded Mine",
			"Guildjen — Branded Mine Jumping Puzzle Guide",
			"https://guildjen.com/branded-mine-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_coddlers_cove",
			"Guides",
			"Coddler’s Cove",
			"Guildjen — Coddler’s Cove Jumping Puzzle Guide",
			"https://guildjen.com/coddlers-cove-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_morgans_leap",
			"Guides",
			"Morgan’s Leap",
			"Guildjen — Morgan’s Leap Jumping Puzzle Guide",
			"https://guildjen.com/morgans-leap-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_loreclaw_expanse",
			"Guides",
			"Loreclaw Expanse",
			"Guildjen — Loreclaw Expanse Jumping Puzzle Guide",
			"https://guildjen.com/loreclaw-expanse-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_king_jaliss_refuge",
			"Guides",
			"King Jalis’s Refuge",
			"Guildjen — King Jalis’s Refuge Jumping Puzzle Guide",
			"https://guildjen.com/king-jaliss-refuge-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_antre_of_adjournment",
			"Guides",
			"Antre of Adjournment",
			"Guildjen — Antre of Adjournment Jumping Puzzle Guide",
			"https://guildjen.com/antre-of-adjournment-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_wall_breach_blitz",
			"Guides",
			"Wall Breach Blitz",
			"Guildjen — Wall Breach Blitz Jumping Puzzle Guide",
			"https://guildjen.com/wall-breach-blitz-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_shattered_ice_ruins",
			"Guides",
			"Shattered Ice Ruins",
			"Guildjen — Shattered Ice Ruins Jumping Puzzle Guide",
			"https://guildjen.com/shattered-ice-ruins-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_griffonrook_run",
			"Guides",
			"Griffonrook Run",
			"Guildjen — Griffonrook Run Jumping Puzzle Guide",
			"https://guildjen.com/griffonrook-run-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_shamans_rookery",
			"Guides",
			"Shaman’s Rookery",
			"Guildjen — Shaman’s Rookery Jumping Puzzle Guide",
			"https://guildjen.com/shamans-rookery-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_under_new_management",
			"Guides",
			"Under New Management",
			"Guildjen — Under New Management Jumping Puzzle Guide",
			"https://guildjen.com/under-new-management-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_scavengers_chasm",
			"Guides",
			"Scavenger’s Chasm",
			"Guildjen — Scavenger’s Chasm Jumping Puzzle Guide",
			"https://guildjen.com/scavengers-chasm-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_grendich_gamble",
			"Guides",
			"Grendich Gamble",
			"Guildjen — Grendich Gamble Jumping Puzzle Guide",
			"https://guildjen.com/grendich-gamble-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_professor_portmatts_lab",
			"Guides",
			"Professor Portmatt’s Lab",
			"Guildjen — Professor Portmatt’s Lab Jumping Puzzle Guide",
			"https://guildjen.com/professor-portmatts-lab-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_goemms_lab",
			"Guides",
			"Goemm’s Lab",
			"Guildjen — Goemm’s Lab Jumping Puzzle Guide",
			"https://guildjen.com/goemms-lab-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_crimson_plateau",
			"Guides",
			"Crimson Plateau",
			"Guildjen — Crimson Plateau Jumping Puzzle Guide",
			"https://guildjen.com/crimson-plateau-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_fawcetts_bounty",
			"Guides",
			"Fawcett’s Bounty",
			"Guildjen — Fawcett’s Bounty Jumping Puzzle Guide",
			"https://guildjen.com/fawcetts-bounty-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},
		{
			"gj_jp_tribulation_caverns",
			"Guides",
			"Tribulation Caverns",
			"Guildjen — Tribulation Caverns Jumping Puzzle Guide",
			"https://guildjen.com/tribulation-caverns-jumping-puzzle-guide/",
			nullptr,
			nullptr,
		},

		/* Cosmetic Infusions (Guildjen how-to + wiki list) */
		{
			"wiki_cosmetic_infusions",
			"Guides",
			"Wiki Cosmetic Infusions",
			"GW2 Wiki — Cosmetic Infusion",
			"https://wiki.guildwars2.com/wiki/Cosmetic_infusion",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_hub",
			"Guides",
			"Cosmetic Infusions",
			"Guildjen — How to Get Cosmetic Infusions",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_vault_arcane_flow",
			"Guides",
			"Arcane Flow Infusion",
			"Guildjen — Arcane Flow Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_vault_forest_wisp",
			"Guides",
			"Forest Wisp Infusion",
			"Guildjen — Forest Wisp Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_forge_mystic",
			"Guides",
			"Mystic Infusion",
			"Guildjen — Mystic Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_queen_bee",
			"Guides",
			"Queen Bee Infusion",
			"Guildjen — Queen Bee Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_aurillium",
			"Guides",
			"Liquid Aurillium Infusion",
			"Guildjen — Liquid Aurillium Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_chak",
			"Guides",
			"Chak Infusion",
			"Guildjen — Chak Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_confetti",
			"Guides",
			"Festive Confetti Infusion",
			"Guildjen — Festive Confetti Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_crystal",
			"Guides",
			"Crystal Infusion",
			"Guildjen — Crystal Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_khan_ur",
			"Guides",
			"Heart of the Khan-Ur",
			"Guildjen — Heart of the Khan-Ur",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_frost_legion",
			"Guides",
			"Frost Legion Infusion",
			"Guildjen — Frost Legion Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_jormag_left",
			"Guides",
			"Jormag Left Eye Infusion",
			"Guildjen — Jormag Left Eye Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_jormag_right",
			"Guides",
			"Jormag Right Eye Infusion",
			"Guildjen — Jormag Right Eye Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_dragonvoid",
			"Guides",
			"Echo of the Dragonvoid",
			"Guildjen — Echo of the Dragonvoid",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_chromatic",
			"Guides",
			"Chromatic Bubbles",
			"Guildjen — Chromatic Bubbles",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_seer",
			"Guides",
			"Seer Transcendence",
			"Guildjen — Seer Transcendence",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_ow_ethereal",
			"Guides",
			"Ethereal Sea-Life Infusion",
			"Guildjen — Ethereal Sea-Life Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_celestial_blue",
			"Guides",
			"Celestial Infusion (Blue)",
			"Guildjen — Celestial Infusion (Blue)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_celestial_red",
			"Guides",
			"Celestial Infusion (Red)",
			"Guildjen — Celestial Infusion (Red)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_abyssal",
			"Guides",
			"Abyssal Infusion",
			"Guildjen — Abyssal Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_mote",
			"Guides",
			"Mote of Darkness",
			"Guildjen — Mote of Darkness",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_ghostly",
			"Guides",
			"Ghostly Infusion",
			"Guildjen — Ghostly Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_peerless",
			"Guides",
			"Peerless Infusion",
			"Guildjen — Peerless Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_everbloom",
			"Guides",
			"Imperial Everbloom",
			"Guildjen — Imperial Everbloom",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_clockwork",
			"Guides",
			"Clockwork Infusion",
			"Guildjen — Clockwork Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_jotun",
			"Guides",
			"Jotun Infusion",
			"Guildjen — Jotun Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_possession",
			"Guides",
			"Possession Infusion",
			"Guildjen — Possession Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_bloodstone",
			"Guides",
			"Bloodstone Infusion",
			"Guildjen — Bloodstone Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_demonic",
			"Guides",
			"Demonic Infusion",
			"Guildjen — Demonic Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_inst_deldrimor",
			"Guides",
			"Deldrimor Stoneskin Infusion",
			"Guildjen — Deldrimor Stoneskin Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_ember",
			"Guides",
			"Ember Infusion",
			"Guildjen — Ember Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_phospho",
			"Guides",
			"Phospholuminescent Infusion",
			"Guildjen — Phospholuminescent Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_reverb_gray",
			"Guides",
			"Polysaturating Reverb (Gray)",
			"Guildjen — Polysaturating Reverberating Infusion (Gray)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_reverb_purple",
			"Guides",
			"Polysaturating Reverb (Purple)",
			"Guildjen — Polysaturating Reverberating Infusion (Purple)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_reverb_red",
			"Guides",
			"Polysaturating Reverb (Red)",
			"Guildjen — Polysaturating Reverberating Infusion (Red)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_poly_black",
			"Guides",
			"Polyluminescent Undulating (Black)",
			"Guildjen — Polyluminescent Undulating Infusion (Black)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_poly_green",
			"Guides",
			"Polyluminescent Undulating (Green)",
			"Guildjen — Polyluminescent Undulating Infusion (Green)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_poly_orange",
			"Guides",
			"Polyluminescent Undulating (Orange)",
			"Guildjen — Polyluminescent Undulating Infusion (Orange)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_poly_teal",
			"Guides",
			"Polyluminescent Undulating (Teal)",
			"Guildjen — Polyluminescent Undulating Infusion (Teal)",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_moto_red",
			"Guides",
			"Moto's Unstable Bauble (Red)",
			"Guildjen — Moto's Unstable Bauble Infusion: Red",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_moto_blue",
			"Guides",
			"Moto's Unstable Bauble (Blue)",
			"Guildjen — Moto's Unstable Bauble Infusion: Blue",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_winters_heart",
			"Guides",
			"Winter's Heart Infusion",
			"Guildjen — Winter's Heart Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_snow_diamond",
			"Guides",
			"Snow Diamond Infusion",
			"Guildjen — Snow Diamond Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_toy_shell",
			"Guides",
			"Toy-Shell Infusion",
			"Guildjen — Toy-Shell Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_fest_silent_symphony",
			"Guides",
			"Silent Symphony",
			"Guildjen — Silent Symphony",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_wvw_mistwalker",
			"Guides",
			"Mistwalker Infusion",
			"Guildjen — Mistwalker Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
			nullptr,
			nullptr,
		},
		{
			"gj_infusion_wvw_heat_core",
			"Guides",
			"Heat Core Infusion",
			"Guildjen — Heat Core Infusion",
			"https://guildjen.com/how-to-get-cosmetic-infusions-in-guild-wars-2/",
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
