#!/usr/bin/env python3
"""One-shot: stamp browsePath + browseSections onto data/sites.json (schema v2).

Encodes the hierarchy rules formerly hardcoded in src/UI_Browse.cpp.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITES_JSON = ROOT / "data" / "sites.json"

BROWSE_SECTIONS: dict[str, list[str]] = {
	"Help": ["Getting Started", "ArenaNet", "Nexus", "Other"],
	"Search": ["Web Search", "AI"],
	"Live": ["News", "Fashion", "Other"],
	"Cheat Sheets": [
		"Prep", "Gear", "Squad", "Fractals", "Encounters", "Account", "WvW", "Other"
	],
	"Tools": [
		"Account", "Overlay", "Timers", "Economy", "Logs / KP", "GW2.app", "Misc", "Other"
	],
	"Guides": [
		"Living World", "Progress", "Mounts", "Fractals", "Raids",
		"Strikes", "Rifts", "PvP", "WvW", "Achievements",
		"Jumping Puzzles", "Crafting", "TLDR", "Farming", "Other",
	],
	"Discord": [
		"Community", "Builds / Sites", "Training", "PvP", "WvW",
		"Farming / Trade", "Addons", "Other",
	],
	"Builds": [
		"Raids", "AccessiBuilds", "Open World / General", "PvP", "WvW", "Editor", "Other"
	],
	"Wiki": [
		"Main", "News", "Special Events", "Collections", "Legendary Armory",
		"Cosmetic Infusions", "Lifestyle", "Crafting", "Food", "Ascended Food",
		"Utility", "Minis", "Upgrades", "Other",
	],
}


def starts(id: str, prefix: str) -> bool:
	return id.startswith(prefix)


def eq(id: str, *vals: str) -> bool:
	return id in vals


def browse_section(category: str, sid: str) -> str | None:
	if category == "Help":
		if eq(sid, "home", "dak393_new_player", "dpsloghelp", "apikeyhelp"):
			return "Getting Started"
		if eq(sid, "gw2official", "gw2news", "gw2forums"):
			return "ArenaNet"
		if eq(sid, "raidcore"):
			return "Nexus"
		return "Other"
	if category == "Search":
		return "AI" if eq(sid, "gemini") else "Web Search"
	if category == "Live":
		if eq(sid, "live_news"):
			return "News"
		if eq(sid, "live_fashion"):
			return "Fashion"
		return "Other"
	if category == "Cheat Sheets":
		if eq(sid, "raidfood", "raidutils", "homegarden", "ascendedstart"):
			return "Prep"
		if eq(sid, "sigilsrunes", "relics"):
			return "Gear"
		if eq(sid, "booncheck", "squadtmpl", "stabcleanse", "ccdefiance", "portalspulls"):
			return "Squad"
		if eq(sid, "fractalcons", "fractalcm"):
			return "Fractals"
		if eq(sid, "raidwings", "strikes"):
			return "Encounters"
		if eq(
			sid,
			"ubersaio",
			"dailyweekly",
			"currencysinks",
			"matconv",
			"legpaths",
			"mounts",
			"homestead",
		):
			return "Account"
		if eq(sid, "wvwcons"):
			return "WvW"
		return "Other"
	if category == "Tools":
		if starts(sid, "gw2app"):
			return "GW2.app"
		if eq(sid, "gw2efficiency", "gw2eff_legendaries"):
			return "Account"
		if eq(sid, "blishhud"):
			return "Overlay"
		if eq(sid, "gw2timer_events", "gw2timer", "gw2tldr_metas"):
			return "Timers"
		if eq(sid, "gw2crafts", "gw2bltc", "gw2treasures"):
			return "Economy"
		if eq(sid, "killproof", "wingman", "hs_arcdps"):
			return "Logs / KP"
		if eq(sid, "gw2mb", "peuresearch"):
			return "Misc"
		return "Other"
	if category == "Guides":
		if eq(sid, "guildjen", "guildjen_lw"):
			return "Living World"
		if eq(
			sid,
			"mb_leveling",
			"mb_gold",
			"gj_new_player",
			"gj_gold",
			"gj_gem_store",
			"gj_wizards_vault",
		):
			return "Progress"
		if eq(sid, "mb_griffon", "mb_skyscale", "gj_roller_beetle", "gj_siege_turtle"):
			return "Mounts"
		if (
			eq(sid, "mb_intro_fractals", "mukluk_fractals", "gj_fractals_hub", "gj_fractals_beginner")
			or starts(sid, "gj_frac_")
		):
			return "Fractals"
		if (
			eq(
				sid,
				"gj_raid_guides",
				"gj_intro_raiding",
				"gj_rw1",
				"gj_rw2",
				"gj_rw3",
				"gj_rw4",
				"gj_rw5",
				"gj_rw6",
				"gj_rw7",
				"gj_rw8",
				"mb_raids_hub",
				"mb_intro_raiding",
				"mb_w8_balrior",
				"sc_raids_hub",
				"sc_intro_squads",
				"sc_squad_roles",
				"sc_joining_squads",
			)
			or starts(sid, "mb_rb_w")
			or starts(sid, "gj_w8_")
			or starts(sid, "sc_w1_")
			or starts(sid, "sc_w2_")
			or starts(sid, "sc_w3_")
			or starts(sid, "sc_w4_")
			or starts(sid, "sc_w5_")
			or starts(sid, "sc_w6_")
			or starts(sid, "sc_w7_")
		):
			return "Raids"
		if eq(
			sid,
			"mb_mai_trin",
			"mb_boneskinner",
			"mb_cold_war",
			"mb_cosmic_obs",
			"mb_forging_steel",
			"mb_fraenir",
			"mb_icebrood",
			"mb_kaineng",
			"mb_lions_court",
			"mb_cerus",
			"mb_voice_claw",
			"mb_whisper",
			"mb_ankka",
			"gj_harvest_temple",
		):
			return "Strikes"
		if eq(sid, "gj_rifts"):
			return "Rifts"
		if eq(sid, "mb_pvp_guides", "gj_pvp_hub", "gj_pvp_beginner"):
			return "PvP"
		if eq(sid, "mb_wvw_guides", "gj_wvw_beginner"):
			return "WvW"
		if starts(sid, "gj_ach_"):
			return "Achievements"
		if eq(sid, "gj_jp_hub") or starts(sid, "gj_jp_"):
			return "Jumping Puzzles"
		if eq(sid, "crafts_hub") or starts(sid, "crafts_"):
			return "Crafting"
		if eq(sid, "gw2tldr", "gw2tldr_raids", "gw2tldr_fractals", "gw2tldr_dungeons"):
			return "TLDR"
		if eq(sid, "fastfarming"):
			return "Farming"
		return "Other"
	if category == "Discord":
		if eq(sid, "discord_official", "discord_community", "discord_central"):
			return "Community"
		if eq(
			sid,
			"discord_snowcrows",
			"discord_metabattle",
			"discord_guildjen",
			"discord_mukluk",
			"discord_aw2",
			"discord_skein",
		):
			return "Builds / Sites"
		if eq(
			sid,
			"discord_fractal",
			"discord_raidacademy",
			"discord_uni",
			"discord_crossroads",
			"discord_rti",
		):
			return "Training"
		if eq(sid, "discord_pvp"):
			return "PvP"
		if eq(sid, "discord_wvw_na", "discord_wvw_eu"):
			return "WvW"
		if eq(sid, "discord_fastfarming", "discord_overflow"):
			return "Farming / Trade"
		if eq(sid, "discord_raidcore"):
			return "Addons"
		return "Other"
	if category == "Builds":
		if eq(
			sid,
			"snowcrows",
			"sc_raid_ele",
			"sc_raid_mes",
			"sc_raid_nec",
			"sc_raid_eng",
			"sc_raid_ran",
			"sc_raid_thf",
			"sc_raid_gua",
			"sc_raid_rev",
			"sc_raid_war",
			"mb_raid_builds",
			"mb_raid_ele",
			"mb_raid_mes",
			"mb_raid_nec",
			"mb_raid_eng",
			"mb_raid_ran",
			"mb_raid_thf",
			"mb_raid_gua",
			"mb_raid_rev",
			"mb_raid_war",
		):
			return "Raids"
		if eq(sid, "sc_accessibuilds", "aw2help"):
			return "AccessiBuilds"
		if eq(sid, "metabattle", "metabattle_ow", "sc_open_world"):
			return "Open World / General"
		if eq(sid, "metabattle_pvp", "sc_pvp"):
			return "PvP"
		if eq(sid, "metabattle_wvw", "sc_wvw"):
			return "WvW"
		if eq(sid, "gw2skills"):
			return "Editor"
		return "Other"
	if category == "Wiki":
		if eq(sid, "wiki"):
			return "Main"
		if eq(sid, "wiki_updates"):
			return "News"
		if eq(sid, "wiki_legendaries", "wiki_mounts"):
			return "Collections"
		if (
			eq(sid, "wiki_larmory_hub", "wiki_larmor_hub", "wiki_leg_hub")
			or starts(sid, "wiki_larmor_")
			or starts(sid, "wiki_laccessory_")
			or starts(sid, "wiki_lamulet_")
			or starts(sid, "wiki_lring_")
			or starts(sid, "wiki_lback_")
			or starts(sid, "wiki_lrune_")
			or starts(sid, "wiki_lsigil_")
			or starts(sid, "wiki_lrelic_")
			or starts(sid, "wiki_leg_")
		):
			return "Legendary Armory"
		if eq(sid, "wiki_cosmetic_infusions", "gj_infusion_hub") or starts(sid, "gj_infusion_"):
			return "Cosmetic Infusions"
		if starts(sid, "wiki_life_"):
			return "Lifestyle"
		if eq(sid, "wiki_craft_hub") or starts(sid, "wiki_craft_"):
			return "Crafting"
		if eq(sid, "wiki_food_hub") or starts(sid, "wiki_food_"):
			return "Food"
		if starts(sid, "wiki_util_"):
			return "Utility"
		if eq(sid, "wiki_mini_hub") or starts(sid, "wiki_mini_"):
			return "Minis"
		if (
			eq(sid, "wiki_rune_hub", "wiki_relic_hub", "wiki_sigil_hub")
			or starts(sid, "wiki_rune_")
			or starts(sid, "wiki_relic_")
			or starts(sid, "wiki_sigil_")
		):
			return "Upgrades"
		if eq(sid, "wiki_afood_hub", "wiki_afood_gourmet") or starts(sid, "wiki_afood_"):
			return "Ascended Food"
		if eq(sid, "wiki_special_events") or starts(sid, "wiki_rush_"):
			return "Special Events"
		return "Other"
	return None


def raid_boss_wing(sid: str) -> str | None:
	if starts(sid, "sc_w1_") or eq(sid, "mb_rb_w1"):
		return "W1 Spirit Vale"
	if starts(sid, "sc_w2_") or eq(sid, "mb_rb_w2"):
		return "W2 Salvation Pass"
	if starts(sid, "sc_w3_") or eq(sid, "mb_rb_w3"):
		return "W3 Stronghold"
	if starts(sid, "sc_w4_") or eq(sid, "mb_rb_w4"):
		return "W4 Bastion"
	if starts(sid, "sc_w5_") or eq(sid, "mb_rb_w5"):
		return "W5 Hall of Chains"
	if starts(sid, "sc_w6_") or eq(sid, "mb_rb_w6"):
		return "W6 Mythwright"
	if starts(sid, "sc_w7_") or eq(sid, "mb_rb_w7"):
		return "W7 Ahdashim"
	if starts(sid, "gj_w8_") or eq(sid, "mb_w8_balrior"):
		return "W8 Mount Balrior"
	return None


def raids_sub(sid: str) -> str | None:
	if eq(
		sid,
		"gj_raid_guides",
		"gj_intro_raiding",
		"gj_rw1",
		"gj_rw2",
		"gj_rw3",
		"gj_rw4",
		"gj_rw5",
		"gj_rw6",
		"gj_rw7",
		"gj_rw8",
	):
		return "Raid Wings"
	if (
		eq(
			sid,
			"sc_raids_hub",
			"sc_intro_squads",
			"sc_squad_roles",
			"sc_joining_squads",
			"mb_raids_hub",
			"mb_intro_raiding",
			"mb_w8_balrior",
		)
		or starts(sid, "sc_w1_")
		or starts(sid, "sc_w2_")
		or starts(sid, "sc_w3_")
		or starts(sid, "sc_w4_")
		or starts(sid, "sc_w5_")
		or starts(sid, "sc_w6_")
		or starts(sid, "sc_w7_")
		or starts(sid, "mb_rb_w")
		or starts(sid, "gj_w8_")
	):
		return "Raid Boss"
	return None


def achievements_sub(sid: str) -> str | None:
	if not starts(sid, "gj_ach_") or eq(sid, "gj_ach_hub"):
		return None
	if starts(sid, "gj_ach_lw_"):
		return "Living World"
	if starts(sid, "gj_ach_hot_"):
		return "Heart of Thorns"
	if starts(sid, "gj_ach_pof_"):
		return "Path of Fire"
	if starts(sid, "gj_ach_eod_"):
		return "End of Dragons"
	if starts(sid, "gj_ach_soto_"):
		return "Secrets of the Obscure"
	if starts(sid, "gj_ach_jw_"):
		return "Janthir Wilds"
	if starts(sid, "gj_ach_voe_"):
		return "Visions of Eternity"
	if starts(sid, "gj_ach_fest_"):
		return "Festivals"
	if starts(sid, "gj_ach_side_"):
		return "Side Stories"
	return None


def infusion_sub(sid: str) -> str | None:
	if eq(sid, "wiki_cosmetic_infusions", "gj_infusion_hub"):
		return None
	if starts(sid, "gj_infusion_vault_"):
		return "Wizard's Vault"
	if starts(sid, "gj_infusion_forge_"):
		return "Mystic Forge"
	if starts(sid, "gj_infusion_ow_"):
		return "Open World"
	if starts(sid, "gj_infusion_inst_"):
		return "Instanced"
	if starts(sid, "gj_infusion_fest_"):
		return "Festival"
	if starts(sid, "gj_infusion_wvw_"):
		return "WvW"
	return None


def legendary_armory_sub(sid: str) -> str | None:
	if eq(sid, "wiki_larmory_hub"):
		return None
	if eq(sid, "wiki_larmor_hub") or starts(sid, "wiki_larmor_"):
		return "Legendary Armor"
	if eq(sid, "wiki_leg_hub") or starts(sid, "wiki_leg_"):
		return "Legendary Weapons"
	if starts(sid, "wiki_laccessory_"):
		return "Legendary Accessory"
	if starts(sid, "wiki_lamulet_"):
		return "Legendary Amulet"
	if starts(sid, "wiki_lring_"):
		return "Legendary Rings"
	if starts(sid, "wiki_lback_"):
		return "Legendary Back Items"
	if starts(sid, "wiki_lrune_") or starts(sid, "wiki_lsigil_") or starts(sid, "wiki_lrelic_"):
		return "Legendary Upgrade Components"
	return None


def legendary_weapon_sub(sid: str) -> str | None:
	if eq(sid, "wiki_leg_hub"):
		return None
	if starts(sid, "wiki_leg_g3v_"):
		return "Generation 3 Variants"
	if starts(sid, "wiki_leg_g1_"):
		return "Generation 1"
	if starts(sid, "wiki_leg_g2_"):
		return "Generation 2"
	if starts(sid, "wiki_leg_g3_"):
		return "Generation 3"
	if starts(sid, "wiki_leg_g4_"):
		return "Generation 4"
	return None


def gen3_variant_dragon(sid: str) -> str | None:
	if not starts(sid, "wiki_leg_g3v_"):
		return None
	if starts(sid, "wiki_leg_g3v_hub_") or starts(sid, "wiki_leg_g3v_facet_"):
		return None
	if starts(sid, "wiki_leg_g3v_zhaitan_"):
		return "Zhaitan"
	if starts(sid, "wiki_leg_g3v_mordremoth_"):
		return "Mordremoth"
	if starts(sid, "wiki_leg_g3v_kralkatorrik_"):
		return "Kralkatorrik"
	if starts(sid, "wiki_leg_g3v_jormag_"):
		return "Jormag"
	if starts(sid, "wiki_leg_g3v_primordus_"):
		return "Primordus"
	if starts(sid, "wiki_leg_g3v_soo_won_"):
		return "Soo-Won"
	return None


def upgrades_sub(sid: str) -> str | None:
	if eq(sid, "wiki_rune_hub") or starts(sid, "wiki_rune_"):
		return "Superior Runes"
	if eq(sid, "wiki_relic_hub") or starts(sid, "wiki_relic_"):
		return "Relics"
	if eq(sid, "wiki_sigil_hub") or starts(sid, "wiki_sigil_"):
		return "Superior Sigils"
	return None


def wiki_crafting_sub(sid: str) -> str | None:
	if eq(sid, "wiki_craft_hub"):
		return None
	if starts(sid, "wiki_craft_disc_"):
		return "Disciplines"
	if starts(sid, "wiki_craft_rel_"):
		return "Related"
	return None


def food_attr_sub(sid: str) -> str | None:
	if eq(
		sid,
		"wiki_food_hub",
		"wiki_afood_hub",
		"wiki_afood_gourmet",
		"wiki_util_hub",
		"wiki_util_list",
		"wiki_util_enhancement",
		"wiki_util_slayer",
		"wiki_util_lw_exp",
		"wiki_util_festival",
	):
		return None
	p = None
	if starts(sid, "wiki_food_"):
		p = sid[10:]
	elif starts(sid, "wiki_afood_"):
		p = sid[11:]
	elif starts(sid, "wiki_util_"):
		p = sid[10:]
	else:
		return None
	pairs = [
		("power_", "Power"),
		("precision_", "Precision"),
		("toughness_", "Toughness"),
		("vitality_", "Vitality"),
		("concentration_", "Concentration"),
		("condition_damage_", "Condition Damage"),
		("expertise_", "Expertise"),
		("ferocity_", "Ferocity"),
		("healing_power_", "Healing Power"),
		("all_attributes_", "All Attributes"),
		("other_", "Other"),
	]
	for pref, name in pairs:
		if p.startswith(pref):
			return name
	return None


def minis_sub(sid: str) -> str | None:
	if eq(sid, "wiki_mini_hub") or not starts(sid, "wiki_mini_"):
		return None
	p = sid[10:]
	pairs = [
		("sets_", "Sets 1 to 3"),
		("core_", "Core"),
		("pvp_", "PvP"),
		("wvw_", "WvW"),
		("hot_", "Heart of Thorns"),
		("raids_", "Raids"),
		("lws3_", "Living World Season 3"),
		("pof_", "Path of Fire"),
		("lws4_", "Living World Season 4"),
		("ibs_", "The Icebrood Saga"),
		("eod_", "End of Dragons"),
		("soto_", "Secrets of the Obscure"),
		("jw_", "Janthir Wilds"),
		("voe_", "Visions of Eternity"),
		("fest_lny_", "Festival Minis - Lunar New Year"),
		("fest_sab_", "Festival Minis - Super Adventure Box"),
		("fest_db_", "Festival Minis - Dragon Bash"),
		("fest_ffw_", "Festival Minis - Festival of the Four Winds"),
		("fest_hw_", "Festival Minis - Halloween"),
		("fest_ws_", "Festival Minis - Wintersday"),
		("gem_", "Gem Store/Black Lion"),
		("promo_", "Promotional Minis"),
		("unavailable_", "Unavailable"),
	]
	for pref, name in pairs:
		if p.startswith(pref):
			return name
	return None


def guides_crafting_sub(sid: str) -> str | None:
	if eq(sid, "crafts_hub"):
		return None
	if starts(sid, "crafts_n_"):
		return "Normal Guides"
	if starts(sid, "crafts_f_"):
		return "Fast Guides"
	if starts(sid, "crafts_400_"):
		return "400-500"
	if starts(sid, "crafts_s_"):
		return "Special"
	return None


def browse_path(category: str, sid: str) -> list[str]:
	sec = browse_section(category, sid)
	if not sec:
		return []
	path = [sec]

	# Guides → Raids nesting (Builds → Raids stays flat — no deeper path).
	if category == "Guides" and sec == "Raids":
		sub = raids_sub(sid)
		if sub:
			path.append(sub)
			if sub == "Raid Boss":
				wing = raid_boss_wing(sid)
				if wing:
					path.append(wing)
		return path

	if category == "Guides" and sec == "Achievements":
		sub = achievements_sub(sid)
		if sub:
			path.append(sub)
		return path

	if category == "Guides" and sec == "Crafting":
		sub = guides_crafting_sub(sid)
		if sub:
			path.append(sub)
		return path

	if category == "Wiki" and sec == "Cosmetic Infusions":
		sub = infusion_sub(sid)
		if sub:
			path.append(sub)
		return path

	if category == "Wiki" and sec == "Legendary Armory":
		sub = legendary_armory_sub(sid)
		if sub:
			path.append(sub)
			if sub == "Legendary Weapons":
				gen = legendary_weapon_sub(sid)
				if gen:
					path.append(gen)
					if gen == "Generation 3 Variants":
						dragon = gen3_variant_dragon(sid)
						if dragon:
							path.append(dragon)
		return path

	if category == "Wiki" and sec == "Upgrades":
		sub = upgrades_sub(sid)
		if sub:
			path.append(sub)
		return path

	if category == "Wiki" and sec == "Crafting":
		sub = wiki_crafting_sub(sid)
		if sub:
			path.append(sub)
		return path

	if category == "Wiki" and sec in ("Food", "Ascended Food", "Utility"):
		sub = food_attr_sub(sid)
		if sub:
			path.append(sub)
		return path

	if category == "Wiki" and sec == "Minis":
		sub = minis_sub(sid)
		if sub:
			path.append(sub)
		return path

	return path


def main() -> int:
	path = Path(sys.argv[1]) if len(sys.argv) > 1 else SITES_JSON
	payload = json.loads(path.read_text(encoding="utf-8"))
	sites = payload.get("sites") or []
	if not isinstance(sites, list) or not sites:
		print("error: empty sites", file=sys.stderr)
		return 1

	for s in sites:
		if not isinstance(s, dict):
			continue
		# Drop prior enrichment if re-run
		s.pop("browsePath", None)
		cat = s.get("category") or ""
		sid = s.get("id") or ""
		bp = browse_path(cat, sid)
		if bp:
			s["browsePath"] = bp

	payload["version"] = 2
	payload["browseSections"] = BROWSE_SECTIONS
	payload["count"] = len(sites)
	payload["source"] = "data/sites.json"

	text = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
	path.write_text(text, encoding="utf-8")
	with_path = sum(1 for s in sites if isinstance(s, dict) and s.get("browsePath"))
	print(f"ok: enriched {len(sites)} sites ({with_path} with browsePath) → {path}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
