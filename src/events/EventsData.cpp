#include "EventsData.h"
#include <cstring>

/* Public UTC spawn windows used by EventsPad. Values are schedule facts
   (wiki / in-game rotations), not copied source from third-party addons. */

namespace
{
	constexpr int Sec(int minutes) { return minutes * 60; }

	/* Fixed UTC-day start lists (seconds after 00:00 UTC). */
	constexpr int s_karkaUtc[] = {
		Sec(120), Sec(360), Sec(630), Sec(900), Sec(1080), Sec(1380)
	};
	constexpr int s_tequatlUtc[] = {
		0, Sec(180), Sec(420), Sec(690), Sec(960), Sec(1140)
	};
	constexpr int s_tripleUtc[] = {
		Sec(60), Sec(240), Sec(480), Sec(750), Sec(1020), Sec(1200)
	};
	/* Starts inside each 2h meta cycle (seconds after cycle epoch). */
	constexpr int s_jadeMawInCycle[] = {
		Sec(5), Sec(45)
	};

	constexpr EventsData::Entry s_entries[] = {
		{ "conv_nayos", "Instanced", "", "Outer Nayos", "[&BB8OAAA=]", "", "", EventsData::Sched::Repeat, true, 1200, 10800, 5400, 1, nullptr, 0 },
		{ "conv_balrior", "Instanced", "", "Mount Balrior", "[&BK4OAAA=]", "", "", EventsData::Sched::Repeat, true, 1200, 10800, 0, 1, nullptr, 0 },
		{ "admiral_taidha_covington", "Core bosses", "", "Admiral Taidha Covington", "[&BKgBAAA=]", "admiral_taidha_covington", "", EventsData::Sched::Repeat, true, 900, 10800, 0, 1, nullptr, 0 },
		{ "claw_of_jormag", "Core bosses", "", "Claw of Jormag", "[&BHoCAAA=]", "claw_of_jormag", "", EventsData::Sched::Repeat, true, 900, 10800, 9000, 1, nullptr, 0 },
		{ "fire_elemental", "Core bosses", "", "Fire Elemental", "[&BEcAAAA=]", "fire_elemental", "", EventsData::Sched::Repeat, true, 900, 7200, 2700, 1, nullptr, 0 },
		{ "inquest_golem_mark_ii", "Core bosses", "", "Golem Mark II", "[&BNQCAAA=]", "inquest_golem_mark_ii", "", EventsData::Sched::Repeat, true, 900, 10800, 7200, 1, nullptr, 0 },
		{ "great_jungle_wurm", "Core bosses", "", "Great Jungle Wurm", "[&BEEFAAA=]", "great_jungle_wurm", "", EventsData::Sched::Repeat, true, 900, 7200, 4500, 1, nullptr, 0 },
		{ "karka_queen", "Core bosses", "", "Karka Queen", "[&BNUGAAA=]", "karka_queen", "", EventsData::Sched::DayList, true, 900, 0, 0, 1, s_karkaUtc, 6 },
		{ "megadestroyer", "Core bosses", "", "Megadestroyer", "[&BM0CAAA=]", "megadestroyer", "", EventsData::Sched::Repeat, true, 900, 10800, 1800, 1, nullptr, 0 },
		{ "modniir_ulgoth", "Core bosses", "", "Modniir Ulgoth", "[&BLAAAAA=]", "modniir_ulgoth", "", EventsData::Sched::Repeat, true, 900, 10800, 5400, 1, nullptr, 0 },
		{ "shadow_behemoth", "Core bosses", "", "Shadow Behemoth", "[&BPcAAAA=]", "shadow_behemoth", "", EventsData::Sched::Repeat, true, 900, 7200, 6300, 1, nullptr, 0 },
		{ "svanir_shaman_chief", "Core bosses", "", "Svanir Shaman Chief", "[&BMIDAAA=]", "svanir_shaman_chief", "", EventsData::Sched::Repeat, true, 900, 7200, 900, 1, nullptr, 0 },
		{ "tequatl_the_sunless", "Core bosses", "", "Tequatl the Sunless", "[&BNABAAA=]", "tequatl_the_sunless", "", EventsData::Sched::DayList, true, 900, 0, 0, 1, s_tequatlUtc, 6 },
		{ "the_shatterer", "Core bosses", "", "The Shatterer", "[&BE4DAAA=]", "the_shatterer", "", EventsData::Sched::Repeat, true, 900, 10800, 3600, 1, nullptr, 0 },
		{ "triple_trouble_wurm", "Core bosses", "", "Triple Trouble", "[&BKoBAAA=]", "triple_trouble_wurm", "", EventsData::Sched::DayList, true, 900, 0, 0, 1, s_tripleUtc, 6 },
		{ "lla_timberline", "LLA", "", "Ley Line Anomaly (Timberline)", "[&BEwCAAA=]", "", "", EventsData::Sched::Repeat, true, 1200, 21600, 1200, 1, nullptr, 0 },
		{ "lla_iron", "LLA", "", "Ley Line Anomaly (Iron Marches)", "[&BOcBAAA=]", "", "", EventsData::Sched::Repeat, true, 1200, 21600, 8400, 1, nullptr, 0 },
		{ "lla_gendarran", "LLA", "", "Ley Line Anomaly (Gendarran)", "[&BOQAAAA=]", "", "", EventsData::Sched::Repeat, true, 1200, 21600, 15600, 1, nullptr, 0 },
		{ "scarlet_invasion", "Invasions", "", "Scarlet's Portal Invasion", "[&BOQAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 7200, 3600, 1, nullptr, 0 },
		{ "awakened_caledon", "Invasions", "", "Awakened Invasion (Caledon)", "[&BD0BAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 25200, 1800, 1, nullptr, 0 },
		{ "awakened_queensdale", "Invasions", "", "Awakened Invasion (Queensdale)", "[&BPcAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 25200, 5400, 1, nullptr, 0 },
		{ "awakened_wayfarer", "Invasions", "", "Awakened Invasion (Wayfarer)", "[&BH0BAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 25200, 9000, 1, nullptr, 0 },
		{ "awakened_ashford", "Invasions", "", "Awakened Invasion (Ashford)", "[&BJkDAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 25200, 12600, 1, nullptr, 0 },
		{ "awakened_gendarran", "Invasions", "", "Awakened Invasion (Gendarran)", "[&BOQAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 25200, 16200, 1, nullptr, 0 },
		{ "awakened_southsun", "Invasions", "", "Awakened Invasion (Southsun)", "[&BNUGAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 25200, 19800, 1, nullptr, 0 },
		{ "awakened_metrica", "Invasions", "", "Awakened Invasion (Metrica)", "[&BEgAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 25200, 23400, 1, nullptr, 0 },
		{ "fractal_kessex", "Fractal Incursions", "", "Fractal Incursion (Kessex)", "[&BBIAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 14400, 0, 1, nullptr, 0 },
		{ "fractal_snowden", "Fractal Incursions", "", "Fractal Incursion (Snowden)", "[&BLQAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 14400, 3600, 1, nullptr, 0 },
		{ "fractal_brisban", "Fractal Incursions", "", "Fractal Incursion (Brisban)", "[&BHUAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 14400, 7200, 1, nullptr, 0 },
		{ "fractal_diessa", "Fractal Incursions", "", "Fractal Incursion (Diessa)", "[&BLQAAAA=]", "", "", EventsData::Sched::Repeat, false, 900, 14400, 10800, 1, nullptr, 0 },
		{ "mad_king", "Festivals", "", "Your Mad King Says...", "[&BBEEAAA=]", "", "", EventsData::Sched::Repeat, false, 600, 7200, 0, 1, nullptr, 0 },
		{ "eye_of_the_north_twisted_marionette", "Icebrood Saga", "Eye of the North", "Twisted Marionette", "[&BAkMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 7200, 0, 1, nullptr, 0 },
		{ "eye_of_the_north_battle_for_lions_arch", "Icebrood Saga", "Eye of the North", "Battle for Lions Arch", "[&BAkMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 1800, 1, nullptr, 0 },
		{ "eye_of_the_north_dragonstorm", "Icebrood Saga", "Eye of the North", "Dragonstorm", "[&BAkMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 7200, 3600, 1, nullptr, 0 },
		{ "eye_of_the_north_tower_of_nightmares", "Icebrood Saga", "Eye of the North", "Tower of Nightmares", "[&BAkMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 5400, 1, nullptr, 0 },
		{ "festival_of_the_four_winds_skiff_race", "Festivals", "Festival of the Four Winds", "Skiff Race", "[&BBwHAAA=]", "", "", EventsData::Sched::CycleSlot, false, 600, 7200, 0, 1, nullptr, 0 },
		{ "festival_of_the_four_winds_water_balloons", "Festivals", "Festival of the Four Winds", "Water Balloons", "[&BBwHAAA=]", "", "", EventsData::Sched::CycleSlot, false, 600, 7200, 900, 1, nullptr, 0 },
		{ "festival_of_the_four_winds_treasure_hunt", "Festivals", "Festival of the Four Winds", "Treasure Hunt", "[&BBwHAAA=]", "", "", EventsData::Sched::CycleSlot, false, 1800, 7200, 1800, 1, nullptr, 0 },
		{ "festival_of_the_four_winds_skimmer_race", "Festivals", "Festival of the Four Winds", "Skimmer Race", "[&BBwHAAA=]", "", "", EventsData::Sched::CycleSlot, false, 600, 7200, 4500, 1, nullptr, 0 },
		{ "festival_of_the_four_winds_fishing", "Festivals", "Festival of the Four Winds", "Fishing", "[&BBwHAAA=]", "", "", EventsData::Sched::CycleSlot, false, 600, 7200, 5400, 1, nullptr, 0 },
		{ "festival_of_the_four_winds_dolyak_race", "Festivals", "Festival of the Four Winds", "Dolyak Race", "[&BBwHAAA=]", "", "", EventsData::Sched::CycleSlot, false, 600, 7200, 6300, 1, nullptr, 0 },
		{ "dry_top_crash_site", "Living World", "Dry Top", "Crash Site", "[&BIAHAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2400, 3600, 0, 1, nullptr, 0 },
		{ "dry_top_sandstorm", "Living World", "Dry Top", "Sandstorm", "[&BIAHAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 3600, 2400, 1, nullptr, 0 },
		{ "verdant_brink_night_bosses", "Heart of Thorns", "Verdant Brink", "Night Bosses", "[&BAgIAAA=]", "", "verdant_brink_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1200, 7200, 600, 1, nullptr, 0 },
		{ "verdant_brink_securing_day", "Heart of Thorns", "Verdant Brink", "Securing Day", "[&BAgIAAA=]", "", "verdant_brink_heros_choice_chest", EventsData::Sched::CycleSlot, true, 4500, 7200, 1800, 1, nullptr, 0 },
		{ "verdant_brink_night_enemy", "Heart of Thorns", "Verdant Brink", "Night Enemy", "[&BAgIAAA=]", "", "verdant_brink_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1500, 7200, 5700, 1, nullptr, 0 },
		{ "auric_basin_challenges", "Heart of Thorns", "Auric Basin", "Challenges", "[&BGwIAAA=]", "", "auric_basin_heros_choice_chest", EventsData::Sched::CycleSlot, true, 900, 7200, 2700, 1, nullptr, 0 },
		{ "auric_basin_octovine", "Heart of Thorns", "Auric Basin", "Octovine", "[&BAIIAAA=]", "", "auric_basin_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1200, 7200, 3600, 1, nullptr, 0 },
		{ "auric_basin_pylons", "Heart of Thorns", "Auric Basin", "Pylons", "[&BN0HAAA=]", "", "auric_basin_heros_choice_chest", EventsData::Sched::CycleSlot, true, 4500, 7200, 5400, 1, nullptr, 0 },
		{ "tangled_depths_prep", "Heart of Thorns", "Tangled Depths", "Prep", "[&BPUHAAA=]", "", "tangled_depths_heros_choice_chest", EventsData::Sched::CycleSlot, true, 300, 7200, 1500, 1, nullptr, 0 },
		{ "tangled_depths_chak_gerent", "Heart of Thorns", "Tangled Depths", "Chak Gerent", "[&BPUHAAA=]", "", "tangled_depths_heros_choice_chest", EventsData::Sched::CycleSlot, true, 2400, 7200, 1800, 1, nullptr, 0 },
		{ "tangled_depths_outposts", "Heart of Thorns", "Tangled Depths", "Outposts", "[&BAwIAAA=]", "", "tangled_depths_heros_choice_chest", EventsData::Sched::CycleSlot, true, 4500, 7200, 4200, 1, nullptr, 0 },
		{ "dragon_s_stand_mordremoth_start", "Heart of Thorns", "Dragon's Stand", "Mordremoth Start", "[&BIgIAAA=]", "", "dragons_stand_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1800, 7200, 5400, 1, nullptr, 0 },
		{ "dragon_s_stand_mordremoth_progress", "Heart of Thorns", "Dragon's Stand", "Mordremoth Progress", "[&BIgIAAA=]", "", "dragons_stand_heros_choice_chest", EventsData::Sched::CycleSlot, true, 5400, 7200, 0, 1, nullptr, 0 },
		{ "lake_doric_noran_s_homestead", "Living World", "Lake Doric", "Noran's Homestead", "[&BK8JAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1800, 7200, 1800, 1, nullptr, 0 },
		{ "lake_doric_saidra_s_haven", "Living World", "Lake Doric", "Saidra's Haven", "[&BK0JAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2700, 7200, 3600, 1, nullptr, 0 },
		{ "lake_doric_new_loamhurst", "Living World", "Lake Doric", "New Loamhurst", "[&BLQJAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2700, 7200, 6300, 1, nullptr, 0 },
		{ "crystal_oasis_casino_rounds", "Path of Fire", "Crystal Oasis", "Casino Rounds", "[&BLsKAAA=]", "", "crystal_oasis_heros_choice_chest", EventsData::Sched::CycleSlot, true, 900, 7200, 300, 1, nullptr, 0 },
		{ "crystal_oasis_choya_pinata", "Path of Fire", "Crystal Oasis", "Choya Pinata", "[&BLsKAAA=]", "", "crystal_oasis_heros_choice_chest", EventsData::Sched::CycleSlot, true, 600, 7200, 1200, 1, nullptr, 0 },
		{ "desert_highlands_buried_treasure", "Path of Fire", "Desert Highlands", "Buried Treasure", "[&BGsKAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 7200, 3600, 1, nullptr, 0 },
		{ "elon_riverlands_the_path_to_ascension", "Path of Fire", "Elon Riverlands", "The Path to Ascension", "[&BFMKAAA=]", "", "elon_riverlands_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1500, 7200, 5400, 1, nullptr, 0 },
		{ "elon_riverlands_doppelganger", "Path of Fire", "Elon Riverlands", "Doppelganger", "[&BFMKAAA=]", "", "elon_riverlands_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1200, 7200, 6900, 1, nullptr, 0 },
		{ "the_desolation_junundu_rising", "Path of Fire", "The Desolation", "Junundu Rising", "[&BMEKAAA=]", "", "the_desolation_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1200, 7200, 1800, 2, nullptr, 0 },
		{ "the_desolation_maws_of_torment", "Path of Fire", "The Desolation", "Maws of Torment", "[&BKMKAAA=]", "", "the_desolation_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1200, 7200, 3600, 1, nullptr, 0 },
		{ "domain_of_vabbi_forged_with_fire", "Path of Fire", "Domain of Vabbi", "Forged with Fire", "[&BO0KAAA=]", "", "domain_of_vabbi_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1800, 7200, 0, 2, nullptr, 0 },
		{ "domain_of_vabbi_serpents_ire", "Path of Fire", "Domain of Vabbi", "Serpents' Ire", "[&BHQKAAA=]", "", "domain_of_vabbi_heros_choice_chest", EventsData::Sched::CycleSlot, true, 1800, 7200, 1800, 1, nullptr, 0 },
		{ "domain_of_istan_palawadan", "Living World", "Domain of Istan", "Palawadan", "[&BAkLAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1800, 7200, 6300, 1, nullptr, 0 },
		{ "jahai_bluffs_escorts", "Living World", "Jahai Bluffs", "Escorts", "[&BIMLAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 3600, 1, nullptr, 0 },
		{ "jahai_bluffs_death_branded_shatterer", "Living World", "Jahai Bluffs", "Death-Branded Shatterer", "[&BJMLAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 4500, 1, nullptr, 0 },
		{ "thunderhead_peaks_the_oil_floes", "Living World", "Thunderhead Peaks", "The Oil Floes", "[&BKYLAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 2700, 1, nullptr, 0 },
		{ "thunderhead_peaks_thunderhead_keep", "Living World", "Thunderhead Peaks", "Thunderhead Keep", "[&BLsLAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 7200, 6300, 1, nullptr, 0 },
		{ "grothmar_valley_effigy", "Icebrood Saga", "Grothmar Valley", "Effigy", "[&BA4MAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 600, 1, nullptr, 0 },
		{ "grothmar_valley_doomlore_shrine", "Icebrood Saga", "Grothmar Valley", "Doomlore Shrine", "[&BA4MAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1320, 7200, 2280, 1, nullptr, 0 },
		{ "grothmar_valley_ooze_pits", "Icebrood Saga", "Grothmar Valley", "Ooze Pits", "[&BPgLAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 7200, 3900, 1, nullptr, 0 },
		{ "grothmar_valley_metal_concert", "Icebrood Saga", "Grothmar Valley", "Metal Concert", "[&BPgLAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 6000, 1, nullptr, 0 },
		{ "bjora_marches_storms_of_winter", "Icebrood Saga", "Bjora Marches", "Storms of Winter", "[&BCcMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 300, 7200, 0, 1, nullptr, 0 },
		{ "bjora_marches_icebrood_champions", "Icebrood Saga", "Bjora Marches", "Icebrood Champions", "[&BCcMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 300, 1, nullptr, 0 },
		{ "bjora_marches_drakkar", "Icebrood Saga", "Bjora Marches", "Drakkar", "[&BDkMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2100, 7200, 3900, 1, nullptr, 0 },
		{ "bjora_marches_defend_jora_s_keep", "Icebrood Saga", "Bjora Marches", "Defend Jora's Keep", "[&BCcMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 6300, 1, nullptr, 0 },
		{ "seitung_province_aetherblade_assault", "End of Dragons", "Seitung Province", "Aetherblade Assault", "[&BGUNAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1800, 7200, 5400, 1, nullptr, 0 },
		{ "new_kaineng_city_kaineng_blackout", "End of Dragons", "New Kaineng City", "Kaineng Blackout", "[&BBkNAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2400, 7200, 0, 1, nullptr, 0 },
		{ "the_echovald_wilds_gang_war", "End of Dragons", "The Echovald Wilds", "Gang War", "[&BMwMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2100, 7200, 1800, 1, nullptr, 0 },
		{ "the_echovald_wilds_kaineng_blackout", "End of Dragons", "The Echovald Wilds", "Kaineng Blackout", "[&BBkNAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 7200, 6000, 1, nullptr, 0 },
		{ "dragon_s_end_jade_maw", "End of Dragons", "Dragon's End", "Jade Maw", "[&BKIMAAA=]", "", "", EventsData::Sched::CycleList, true, 480, 7200, 0, 1, s_jadeMawInCycle, 2 },
		{ "dragon_s_end_battle_for_the_jade_sea", "End of Dragons", "Dragon's End", "Battle for the Jade Sea", "[&BKIMAAA=]", "", "", EventsData::Sched::CycleSlot, true, 3600, 7200, 3600, 1, nullptr, 0 },
		{ "skywatch_archipelago_unlocking_the_wizard_s_tower", "Secrets of the Obscure", "Skywatch Archipelago", "Unlocking the Wizard's Tower", "[&BL4NAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1500, 7200, 3600, 1, nullptr, 0 },
		{ "wizard_s_tower_target_practice", "Secrets of the Obscure", "Wizard's Tower", "Target Practice", "[&BB8OAAA=]", "", "", EventsData::Sched::CycleSlot, true, 3300, 7200, 3600, 1, nullptr, 0 },
		{ "wizard_s_tower_fly_by_night", "Secrets of the Obscure", "Wizard's Tower", "Fly by Night", "[&BB8OAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2400, 7200, 6000, 1, nullptr, 0 },
		{ "amnytas_defense_of_amnytas", "Secrets of the Obscure", "Amnytas", "Defense of Amnytas", "[&BDQOAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1500, 7200, 0, 1, nullptr, 0 },
		{ "janthir_syntri_of_mists_and_monsters", "Janthir Wilds", "Janthir Syntri", "Of Mists and Monsters", "[&BCoPAAA=]", "", "", EventsData::Sched::CycleSlot, true, 900, 7200, 1800, 1, nullptr, 0 },
		{ "bava_nisos_a_titanic_voyage", "Janthir Wilds", "Bava Nisos", "A Titanic Voyage", "[&BGEPAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1500, 7200, 4800, 1, nullptr, 0 },
		{ "shipwreck_strand_hammerhart_rumble", "Visions of Eternity", "Shipwreck Strand", "Hammerhart Rumble", "[&BJEPAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1200, 7200, 2400, 1, nullptr, 0 },
		{ "starlit_weald_secrets_of_the_weald", "Visions of Eternity", "Starlit Weald", "Secrets of the Weald", "[&BJ4PAAA=]", "", "", EventsData::Sched::CycleSlot, true, 2100, 7200, 6000, 1, nullptr, 0 },
		{ "eternity_s_garden_shackles_of_the_ancients", "Visions of Eternity", "Eternity's Garden", "Shackles of the Ancients", "[&BPwPAAA=]", "", "", EventsData::Sched::CycleSlot, true, 1500, 7200, 4200, 1, nullptr, 0 },
	};
	constexpr int s_entryCount = static_cast<int>(sizeof(s_entries) / sizeof(s_entries[0]));

	constexpr const char* s_sections[] = {
		"Instanced",
		"Core bosses",
		"LLA",
		"Invasions",
		"Fractal Incursions",
		"Festivals",
		"Icebrood Saga",
		"Living World",
		"Heart of Thorns",
		"Path of Fire",
		"End of Dragons",
		"Secrets of the Obscure",
		"Janthir Wilds",
		"Visions of Eternity",
	};
	constexpr int s_sectionCount = static_cast<int>(sizeof(s_sections) / sizeof(s_sections[0]));
}

const EventsData::Entry* EventsData::All(size_t* outCount)
{
	if (outCount) *outCount = static_cast<size_t>(s_entryCount);
	return s_entries;
}

const char* const* EventsData::Sections(size_t* outCount)
{
	if (outCount) *outCount = static_cast<size_t>(s_sectionCount);
	return s_sections;
}

int EventsData::IndexOfKey(const char* key)
{
	if (!key || !key[0]) return -1;
	for (int i = 0; i < s_entryCount; ++i)
		if (std::strcmp(s_entries[i].key, key) == 0) return i;
	return -1;
}

namespace
{
	struct MapIds
	{
		unsigned a = 0;
		unsigned b = 0; /* optional second id (e.g. Verdant Brink layers) */
	};

	bool IdIn(const MapIds& m, unsigned id)
	{
		return id != 0 && (id == m.a || (m.b != 0 && id == m.b));
	}

	/* Meta / festival mapLabel -> open-world map id(s). */
	MapIds IdsForLabel(const char* label)
	{
		if (!label || !label[0]) return {};
		struct Row { const char* label; MapIds ids; };
		static constexpr Row kRows[] = {
			{ "Dry Top", { 988 } },
			{ "Verdant Brink", { 1042, 1052 } },
			{ "Auric Basin", { 1043 } },
			{ "Tangled Depths", { 1045 } },
			{ "Dragon's Stand", { 1041 } },
			{ "Lake Doric", { 1185 } },
			{ "Crystal Oasis", { 1210 } },
			{ "Desert Highlands", { 1211 } },
			{ "Elon Riverlands", { 1228 } },
			{ "The Desolation", { 1226 } },
			{ "Domain of Vabbi", { 1248 } },
			{ "Domain of Istan", { 1263 } },
			{ "Jahai Bluffs", { 1301 } },
			{ "Thunderhead Peaks", { 1310 } },
			{ "Grothmar Valley", { 1330 } },
			{ "Bjora Marches", { 1343 } },
			{ "Eye of the North", { 1370, 1358 } },
			{ "Seitung Province", { 1442 } },
			{ "New Kaineng City", { 1438 } },
			{ "The Echovald Wilds", { 1452 } },
			{ "Dragon's End", { 1422 } },
			{ "Skywatch Archipelago", { 1510 } },
			{ "Wizard's Tower", { 1509 } },
			{ "Amnytas", { 1517 } },
			{ "Janthir Syntri", { 1554 } },
			{ "Bava Nisos", { 1574 } },
			{ "Shipwreck Strand", { 1595 } },
			{ "Starlit Weald", { 1593 } },
			{ "Eternity's Garden", { 1622 } },
			{ "Festival of the Four Winds", { 922 } }, /* Labyrinthine Cliffs */
		};
		for (const Row& r : kRows)
			if (std::strcmp(r.label, label) == 0)
				return r.ids;
		return {};
	}

	/* World bosses / LLA / invasions / fractals (empty mapLabel). */
	MapIds IdsForKey(const char* key)
	{
		if (!key || !key[0]) return {};
		struct Row { const char* key; MapIds ids; };
		static constexpr Row kRows[] = {
			{ "admiral_taidha_covington", { 73 } },
			{ "claw_of_jormag", { 30 } },
			{ "fire_elemental", { 35 } },
			{ "inquest_golem_mark_ii", { 39 } },
			{ "great_jungle_wurm", { 34 } },
			{ "karka_queen", { 873 } },
			{ "megadestroyer", { 39 } },
			{ "modniir_ulgoth", { 17 } },
			{ "shadow_behemoth", { 15 } },
			{ "svanir_shaman_chief", { 28 } },
			{ "tequatl_the_sunless", { 53 } },
			{ "the_shatterer", { 20 } },
			{ "triple_trouble_wurm", { 54 } },
			{ "lla_timberline", { 29 } },
			{ "lla_iron", { 25 } },
			{ "lla_gendarran", { 24 } },
			{ "scarlet_invasion", { 24 } },
			{ "awakened_caledon", { 34 } },
			{ "awakened_queensdale", { 15 } },
			{ "awakened_wayfarer", { 28 } },
			{ "awakened_ashford", { 19 } },
			{ "awakened_gendarran", { 24 } },
			{ "awakened_southsun", { 873 } },
			{ "awakened_metrica", { 35 } },
			{ "fractal_kessex", { 23 } },
			{ "fractal_snowden", { 31 } },
			{ "fractal_brisban", { 54 } },
			{ "fractal_diessa", { 32 } },
			{ "mad_king", { 50 } },
			{ "conv_nayos", { 1526 } },
			{ "conv_balrior", { 1564, 1572 } },
		};
		for (const Row& r : kRows)
			if (std::strcmp(r.key, key) == 0)
				return r.ids;
		return {};
	}

	struct NameRow { unsigned id; const char* name; };
	constexpr NameRow kNames[] = {
		{ 15, "Queensdale" }, { 17, "Harathi Hinterlands" }, { 19, "Plains of Ashford" },
		{ 20, "Blazeridge Steppes" }, { 23, "Kessex Hills" }, { 24, "Gendarran Fields" },
		{ 25, "Iron Marches" }, { 28, "Wayfarer Foothills" }, { 29, "Timberline Falls" },
		{ 30, "Frostgorge Sound" }, { 31, "Snowden Drifts" }, { 32, "Diessa Plateau" },
		{ 34, "Caledon Forest" }, { 35, "Metrica Province" }, { 39, "Mount Maelstrom" },
		{ 50, "Lion's Arch" }, { 53, "Sparkfly Fen" }, { 54, "Brisban Wildlands" },
		{ 73, "Bloodtide Coast" }, { 873, "Southsun Cove" }, { 922, "Labyrinthine Cliffs" },
		{ 988, "Dry Top" }, { 1041, "Dragon's Stand" }, { 1042, "Verdant Brink" },
		{ 1043, "Auric Basin" }, { 1045, "Tangled Depths" }, { 1052, "Verdant Brink" },
		{ 1185, "Lake Doric" }, { 1210, "Crystal Oasis" }, { 1211, "Desert Highlands" },
		{ 1226, "The Desolation" }, { 1228, "Elon Riverlands" }, { 1248, "Domain of Vabbi" },
		{ 1263, "Domain of Istan" }, { 1301, "Jahai Bluffs" }, { 1310, "Thunderhead Peaks" },
		{ 1330, "Grothmar Valley" }, { 1343, "Bjora Marches" }, { 1358, "Eye of the North" },
		{ 1370, "Eye of the North" }, { 1422, "Dragon's End" }, { 1438, "New Kaineng City" },
		{ 1442, "Seitung Province" }, { 1452, "The Echovald Wilds" }, { 1509, "Wizard's Tower" },
		{ 1510, "Skywatch Archipelago" }, { 1517, "Amnytas" }, { 1526, "Inner Nayos" },
		{ 1554, "Janthir Syntri" }, { 1564, "Mount Balrior" }, { 1572, "Mount Balrior" },
		{ 1574, "Bava Nisos" }, { 1593, "Starlit Weald" }, { 1595, "Shipwreck Strand" },
		{ 1622, "Eternity's Garden" },
	};
}

bool EventsData::EntryMatchesMap(const Entry& e, unsigned mapId)
{
	if (mapId == 0)
		return false;
	MapIds ids = IdsForLabel(e.mapLabel);
	if (ids.a == 0)
		ids = IdsForKey(e.key);
	return IdIn(ids, mapId);
}

const char* EventsData::MapDisplayName(unsigned mapId)
{
	if (mapId == 0)
		return nullptr;
	for (const NameRow& r : kNames)
		if (r.id == mapId)
			return r.name;
	return nullptr;
}
