/* Curated farm-run catalog — map IDs match GW2 continent maps. */
#include "FarmingShared.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace FarmingDetail
{
	namespace
	{
		bool gReady = false;

		void AddStep(Run& r, const char* text)
		{
			RunStep s{};
			std::snprintf(s.text, sizeof(s.text), "%s", text ? text : "");
			r.steps.push_back(s);
		}

		void AddRun(int id, int mapId, RunTag tag, const char* name, const char* blurb,
			const char* pathingHint, std::initializer_list<const char*> steps,
			const char* eventsKey = nullptr)
		{
			Run r{};
			r.id = id;
			r.mapId = mapId;
			r.tag = tag;
			r.custom = false;
			std::snprintf(r.name, sizeof(r.name), "%s", name ? name : "");
			std::snprintf(r.blurb, sizeof(r.blurb), "%s", blurb ? blurb : "");
			std::snprintf(r.pathingHint, sizeof(r.pathingHint), "%s",
				pathingHint ? pathingHint : "");
			if (eventsKey && eventsKey[0])
				std::snprintf(r.eventsKey, sizeof(r.eventsKey), "%s", eventsKey);
			for (const char* s : steps)
				AddStep(r, s);
			gRuns.push_back(r);
		}
	}

	const char* TagLabel(RunTag t)
	{
		switch (t)
		{
		case RunTag::Meta: return "Meta";
		case RunTag::Gather: return "Gather";
		case RunTag::Currency: return "Currency";
		case RunTag::Fishing: return "Fishing";
		case RunTag::Home: return "Home";
		case RunTag::Festival: return "Festival";
		case RunTag::Custom: return "Custom";
		default: return "All";
		}
	}

	void EnsureCatalog()
	{
		if (gReady) return;
		gReady = true;
		gRuns.clear();

		const char* kGather = "tw_guides.tw_gatheringnodes";
		const char* kFish = "tw_guides.tw_fishing";

		/* —— Maguuma Wastes / HoT —— */
		AddRun(1, 1015, RunTag::Meta, "Silverwastes farm loop",
			"Breaches, forts, chests — classic gold + bandit crests.",
			kGather,
			{"Enter The Silverwastes",
			 "Clear fort / breach events",
			 "Loot chests | gather nodes",
			 "Bank / salvage | repeat"});
		AddRun(2, 988, RunTag::Gather, "Dry Top crystals",
			"Crash-site / aspect events for geodes and crystals.",
			kGather,
			{"Enter Dry Top",
			 "Crash site / aspect events",
			 "Mine crystals | pack assist"});
		AddRun(5, 1042, RunTag::Meta, "Verdant Brink night",
			"Night bosses + airship chests when the canopy is dark.",
			kGather,
			{"Enter Verdant Brink",
			 "Wait for night cycle",
			 "Clear canopy / airship events",
			 "Loot chests | extract"},
			"verdant_brink_night_bosses");
		AddRun(6, 1043, RunTag::Meta, "Auric Basin meta",
			"Octovine / pylons — map meta + chests.",
			kGather,
			{"Enter Auric Basin",
			 "Hold / charge pylons",
			 "Join Octovine meta",
			 "Loot chests | salvage"},
			"auric_basin_octovine");
		AddRun(7, 1045, RunTag::Currency, "Tangled Depths chak eggs",
			"Chak Gerent lanes + egg chests — classic HoT farm.",
			kGather,
			{"Enter Tangled Depths",
			 "Join a Gerent lane",
			 "Clear egg chamber events",
			 "Loot | bank | repeat"},
			"tangled_depths_chak_gerent");
		AddRun(8, 1041, RunTag::Meta, "Dragon's Stand meta",
			"Triple worm + Mordremoth — weekly-scale map clear.",
			kGather,
			{"Enter Dragon's Stand",
			 "Advance lanes / smash Mordrem",
			 "Finish map meta",
			 "Loot | extract"},
			"dragon_s_stand_mordremoth_start");

		/* —— Living World S3 / Orr —— */
		AddRun(9, 1165, RunTag::Currency, "Bloodstone Fen shards",
			"Unbound magic + bloodstone dust from events / nodes.",
			kGather,
			{"Enter Bloodstone Fen",
			 "Clear crater / jade events",
			 "Gather nodes | loot",
			 "Bank unbound magic"});
		AddRun(10, 1175, RunTag::Gather, "Ember Bay nodes",
			"Thermal / volcanic gathering + events.",
			kGather,
			{"Enter Ember Bay",
			 "Clear thermal events",
			 "Gather ore / plants / wood",
			 "Bank | salvage"});
		AddRun(11, 1203, RunTag::Currency, "Siren's Landing orr",
			"Orrian pearls / map currency from temple events.",
			kGather,
			{"Enter Siren's Landing",
			 "Clear temple events",
			 "Gather | loot",
			 "Vendor / bank"});
		AddRun(12, 51, RunTag::Currency, "Straits — obsidian shards",
			"Temple / Orr farm hub for shards and rare mats.",
			kGather,
			{"Waypoint Straits of Devastation",
			 "Run temple / event chain",
			 "Gather nodes on route",
			 "Bank shards"});

		/* —— Path of Fire / Crystal Desert —— */
		AddRun(13, 1263, RunTag::Meta, "Istan corsair farm",
			"Corsair flotilla / bounty loops — gold and keys.",
			kGather,
			{"Enter Domain of Istan",
			 "Join corsair / bounty events",
			 "Loot chests | salvage",
			 "Bank | repeat"});
		AddRun(14, 1271, RunTag::Meta, "Sandswept Isles lab",
			"Lab / Inquest events + gather.",
			kGather,
			{"Enter Sandswept Isles",
			 "Clear lab / Inquest events",
			 "Gather nodes",
			 "Bank | salvage"});
		AddRun(15, 1317, RunTag::Meta, "Dragonfall meta",
			"Map meta + chests — strong PoF-era farm.",
			kGather,
			{"Enter Dragonfall",
			 "Hold islands / advance meta",
			 "Finish map event",
			 "Loot | bank"});
		AddRun(16, 1210, RunTag::Gather, "Crystal Oasis gather",
			"Desert nodes + bounty fillers.",
			kGather,
			{"Enter Crystal Oasis",
			 "Gather ore / plants / wood",
			 "Optional bounty",
			 "Bank"});

		/* —— Icebrood / Drizzlewood —— */
		AddRun(17, 1371, RunTag::Meta, "Drizzlewood coast",
			"Alliance / war progress events — dense loot.",
			kGather,
			{"Enter Drizzlewood Coast",
			 "Join base / war events",
			 "Loot chests | salvage",
			 "Bank | repeat"});
		AddRun(18, 1343, RunTag::Gather, "Bjora Marches nodes",
			"Icebrood gathering + Raven shrines.",
			kGather,
			{"Enter Bjora Marches",
			 "Gather nodes on route",
			 "Clear shrine / event fillers",
			 "Bank"});

		/* —— End of Dragons —— */
		AddRun(19, 1442, RunTag::Fishing, "Seitung Province fishing",
			"Canthan coastal / river holes — Pathing fishing markers.",
			kFish,
			{"Enter Seitung Province",
			 "Equip fishing gear + bait",
			 "Hit coastal / river holes",
			 "Log catches in Fishing tab"});
		AddRun(20, 1422, RunTag::Meta, "Dragon's End meta",
			"Soohyang / jade tech map meta.",
			kGather,
			{"Enter Dragon's End",
			 "Advance map meta stages",
			 "Loot chests",
			 "Bank | salvage"});
		AddRun(21, 1490, RunTag::Currency, "Gyala Delve",
			"Delve events + map currency.",
			kGather,
			{"Enter Gyala Delve",
			 "Clear delve event chain",
			 "Gather | loot",
			 "Bank"});
		AddRun(22, 1452, RunTag::Gather, "Echovald Wilds nodes",
			"Canthan forest gather loop.",
			kGather,
			{"Enter The Echovald Wilds",
			 "Gather wood / plants / ore",
			 "Optional event fillers",
			 "Bank"});

		/* —— SotO / Janthir —— */
		AddRun(23, 1510, RunTag::Meta, "Skywatch Archipelago",
			"Island events + SotO map farm.",
			kGather,
			{"Enter Skywatch Archipelago",
			 "Clear island events",
			 "Loot | gather",
			 "Bank"});
		AddRun(24, 1526, RunTag::Meta, "Inner Nayos",
			"Nayos events / map meta fillers.",
			kGather,
			{"Enter Inner Nayos",
			 "Join map events",
			 "Loot chests",
			 "Bank"});
		AddRun(25, 1554, RunTag::Gather, "Janthir Syntri nodes",
			"Janthir gathering + event fillers.",
			kGather,
			{"Enter Janthir Syntri",
			 "Gather nodes",
			 "Clear nearby events",
			 "Bank"});
		AddRun(26, 1575, RunTag::Meta, "Mistburned Barrens",
			"Janthir Wilds map events.",
			kGather,
			{"Enter Mistburned Barrens",
			 "Join map events",
			 "Loot | gather",
			 "Bank"});

		/* —— Home / fishing / festival —— */
		AddRun(3, 0, RunTag::Fishing, "Fishing — Kryta coasts",
			"Manual catch log; Pathing fishing holes when packs enabled.",
			kFish,
			{"Equip fishing gear",
			 "Visit coastal holes (Kryta)",
			 "Log catches in Fishing tab"});
		AddRun(27, 0, RunTag::Fishing, "Fishing — Maguuma / Ring of Fire",
			"Jungle and RoF fishing holes.",
			kFish,
			{"Equip fishing gear + regional bait",
			 "Visit Maguuma / RoF holes",
			 "Log catches in Fishing tab"});
		AddRun(28, 0, RunTag::Fishing, "Fishing — Crystal Desert",
			"Desert fishing holes + Pathing markers.",
			kFish,
			{"Equip fishing gear + desert bait",
			 "Visit desert holes",
			 "Log catches in Fishing tab"});
		AddRun(4, 0, RunTag::Home, "Home instance nodes",
			"Planted nodes + daily chests in your home.",
			kGather,
			{"Enter home instance",
			 "Gather planted nodes",
			 "Collect daily chests"});
		AddRun(29, 1370, RunTag::Home, "Eye of the North hub",
			"EoTN vendors / portals / daily fillers.",
			kGather,
			{"Enter Eye of the North",
			 "Check vendors / portals",
			 "Optional daily events",
			 "Bank"});
		AddRun(30, 0, RunTag::Home, "Homestead gather",
			"Homestead nodes and decorations farm.",
			kGather,
			{"Enter homestead",
			 "Gather homestead nodes",
			 "Collect decorations / chests"});
		AddRun(31, 922, RunTag::Festival, "Labyrinthine Cliffs",
			"Festival map — vendors + gather when open.",
			kGather,
			{"Enter Labyrinthine Cliffs (when open)",
			 "Vendor / event loop",
			 "Gather festival nodes",
			 "Bank"});
		AddRun(32, 0, RunTag::Festival, "Festival dailies (generic)",
			"Checklist for seasonal daily farms.",
			kGather,
			{"Open festival map / hub",
			 "Complete festival dailies",
			 "Spend tickets / vendor",
			 "Bank rewards"});
		AddRun(33, 39, RunTag::Gather, "Mount Maelstrom nodes",
			"Dense core Maguuma gathering.",
			kGather,
			{"Enter Mount Maelstrom",
			 "Gather ore / plants / wood",
			 "Optional event fillers",
			 "Bank"});
		AddRun(34, 62, RunTag::Currency, "Cursed Shore orr",
			"Temple events + Orr mats.",
			kGather,
			{"Enter Cursed Shore",
			 "Clear temple events",
			 "Gather on route",
			 "Bank"});
	}
}
