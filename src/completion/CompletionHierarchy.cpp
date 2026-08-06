#include "CompletionShared.h"

#include <cstdio>
#include <cstring>

namespace CompletionDetail
{
	namespace
	{
		struct HierRow
		{
			uint32_t    mapId;
			const char* release;
			const char* region;
			const char* name; /* optional fill if blank */
		};

		/* Open-world / hub Public maps from official /v2/maps (type=Public, continent=Tyria).
		   Strikes, raids, festival, and "(Public)" instance clones omitted.
		   Local table only - no game memory. Unknown ids -> Uncatalogued. */
		const HierRow kHier[] = {
			/* Core Tyria */
			{ 218, "Core Tyria", "Ascalon", "Black Citadel" },
			{ 20, "Core Tyria", "Ascalon", "Blazeridge Steppes" },
			{ 32, "Core Tyria", "Ascalon", "Diessa Plateau" },
			{ 21, "Core Tyria", "Ascalon", "Fields of Ruin" },
			{ 22, "Core Tyria", "Ascalon", "Fireheart Rise" },
			{ 25, "Core Tyria", "Ascalon", "Iron Marches" },
			{ 19, "Core Tyria", "Ascalon", "Plains of Ashford" },
			{ 73, "Core Tyria", "Kryta", "Bloodtide Coast" },
			{ 335, "Core Tyria", "Kryta", "Claw Island" },
			{ 18, "Core Tyria", "Kryta", "Divinity's Reach" },
			{ 24, "Core Tyria", "Kryta", "Gendarran Fields" },
			{ 17, "Core Tyria", "Kryta", "Harathi Hinterlands" },
			{ 23, "Core Tyria", "Kryta", "Kessex Hills" },
			{ 50, "Core Tyria", "Kryta", "Lion's Arch" },
			{ 15, "Core Tyria", "Kryta", "Queensdale" },
			{ 873, "Core Tyria", "Kryta", "Southsun Cove" },
			{ 54, "Core Tyria", "Maguuma Jungle", "Brisban Wildlands" },
			{ 34, "Core Tyria", "Maguuma Jungle", "Caledon Forest" },
			{ 35, "Core Tyria", "Maguuma Jungle", "Metrica Province" },
			{ 39, "Core Tyria", "Maguuma Jungle", "Mount Maelstrom" },
			{ 139, "Core Tyria", "Maguuma Jungle", "Rata Sum" },
			{ 53, "Core Tyria", "Maguuma Jungle", "Sparkfly Fen" },
			{ 91, "Core Tyria", "Maguuma Jungle", "The Grove" },
			{ 62, "Core Tyria", "Ruins of Orr", "Cursed Shore" },
			{ 65, "Core Tyria", "Ruins of Orr", "Malchor's Leap" },
			{ 51, "Core Tyria", "Ruins of Orr", "Straits of Devastation" },
			{ 26, "Core Tyria", "Shiverpeak Mountains", "Dredgehaunt Cliffs" },
			{ 30, "Core Tyria", "Shiverpeak Mountains", "Frostgorge Sound" },
			{ 326, "Core Tyria", "Shiverpeak Mountains", "Hoelbrak" },
			{ 27, "Core Tyria", "Shiverpeak Mountains", "Lornar's Pass" },
			{ 31, "Core Tyria", "Shiverpeak Mountains", "Snowden Drifts" },
			{ 29, "Core Tyria", "Shiverpeak Mountains", "Timberline Falls" },
			{ 28, "Core Tyria", "Shiverpeak Mountains", "Wayfarer Foothills" },
			/* End of Dragons */
			{ 1428, "End of Dragons", "Cantha", "Arborstone" },
			{ 1422, "End of Dragons", "Cantha", "Dragon's End" },
			{ 1490, "End of Dragons", "Cantha", "Gyala Delve" },
			{ 1438, "End of Dragons", "Cantha", "New Kaineng City" },
			{ 1442, "End of Dragons", "Cantha", "Seitung Province" },
			{ 1452, "End of Dragons", "Cantha", "The Echovald Wilds" },
			/* Heart of Thorns */
			{ 1043, "Heart of Thorns", "Heart of Maguuma", "Auric Basin" },
			{ 1041, "Heart of Thorns", "Heart of Maguuma", "Dragon's Stand" },
			{ 1045, "Heart of Thorns", "Heart of Maguuma", "Tangled Depths" },
			{ 1042, "Heart of Thorns", "Heart of Maguuma", "Verdant Brink" },
			/* Icebrood Saga */
			{ 1330, "Icebrood Saga", "Ascalon", "Grothmar Valley" },
			{ 1343, "Icebrood Saga", "Shiverpeak Mountains", "Bjora Marches" },
			{ 1371, "Icebrood Saga", "Shiverpeak Mountains", "Drizzlewood Coast" },
			{ 1370, "Icebrood Saga", "Shiverpeak Mountains", "Eye of the North" },
			/* Janthir Wilds */
			{ 1574, "Janthir Wilds", "Janthir", "Bava Nisos" },
			{ 1554, "Janthir Wilds", "Janthir", "Janthir Syntri" },
			{ 1550, "Janthir Wilds", "Janthir", "Lowland Shore" },
			{ 1575, "Janthir Wilds", "Janthir", "Mistburned Barrens" },
			/* Living World */
			{ 1263, "Living World", "Crystal Desert", "Domain of Istan" },
			{ 1288, "Living World", "Crystal Desert", "Domain of Kourna" },
			{ 1317, "Living World", "Crystal Desert", "Dragonfall" },
			{ 1301, "Living World", "Crystal Desert", "Jahai Bluffs" },
			{ 1271, "Living World", "Crystal Desert", "Sandswept Isles" },
			{ 1165, "Living World", "Heart of Maguuma", "Bloodstone Fen" },
			{ 1175, "Living World", "Heart of Maguuma", "Ember Bay" },
			{ 1185, "Living World", "Kryta", "Lake Doric" },
			{ 988, "Living World", "Maguuma Wastes", "Dry Top" },
			{ 1015, "Living World", "Maguuma Wastes", "The Silverwastes" },
			{ 1195, "Living World", "Ring of Fire", "Draconis Mons" },
			{ 1203, "Living World", "Ruins of Orr", "Siren's Landing" },
			{ 1178, "Living World", "Shiverpeak Mountains", "Bitterfrost Frontier" },
			{ 1310, "Living World", "Shiverpeak Mountains", "Thunderhead Peaks" },
			/* Path of Fire */
			{ 1210, "Path of Fire", "Crystal Desert", "Crystal Oasis" },
			{ 1211, "Path of Fire", "Crystal Desert", "Desert Highlands" },
			{ 1248, "Path of Fire", "Crystal Desert", "Domain of Vabbi" },
			{ 1228, "Path of Fire", "Crystal Desert", "Elon Riverlands" },
			{ 1226, "Path of Fire", "Crystal Desert", "The Desolation" },
			{ 1215, "Path of Fire", "Crystal Desert", "Windswept Haven" },
			/* Secrets of the Obscure */
			{ 1517, "Secrets of the Obscure", "Horn of Maguuma", "Amnytas" },
			{ 1526, "Secrets of the Obscure", "Horn of Maguuma", "Inner Nayos" },
			{ 1510, "Secrets of the Obscure", "Horn of Maguuma", "Skywatch Archipelago" },
			{ 1509, "Secrets of the Obscure", "Horn of Maguuma", "The Wizard's Tower" },
			/* Visions of Eternity */
			{ 1622, "Visions of Eternity", "Castora", "Eternity's Garden" },
			{ 1595, "Visions of Eternity", "Castora", "Shipwreck Strand" },
			{ 1593, "Visions of Eternity", "Castora", "Starlit Weald" },
		};

	}

	const char* DefaultRelease() { return "Uncatalogued"; }
	const char* DefaultRegion() { return "Unknown"; }

	void ApplyHierarchy(MapInfo& m)
	{
		for (const HierRow& r : kHier)
		{
			if (r.mapId != m.id)
				continue;
			if (!m.release[0])
				std::snprintf(m.release, sizeof(m.release), "%s", r.release);
			if (!m.region[0])
				std::snprintf(m.region, sizeof(m.region), "%s", r.region);
			if (!m.name[0] && r.name && r.name[0])
				std::snprintf(m.name, sizeof(m.name), "%s", r.name);
			return;
		}
		if (!m.release[0])
			std::snprintf(m.release, sizeof(m.release), "%s", DefaultRelease());
		if (!m.region[0])
			std::snprintf(m.region, sizeof(m.region), "%s", DefaultRegion());
	}

	void VisitHierarchy(HierVisitFn fn, void* ctx)
	{
		if (!fn) return;
		for (const HierRow& r : kHier)
			fn(r.mapId, r.release, r.region, r.name ? r.name : "", ctx);
	}
}
