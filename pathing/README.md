# Pathing packs

Put TacO / BlishHUD **`.taco`** files in this folder. The addon loads all of them.

## Curated packs (auto-updated)

On first Pathing load (and when you click **Update curated**), the addon downloads:

| Pack | Source |
|------|--------|
| `tw_ALL_IN_ONE.taco` | [Tekkit's Workshop](https://www.tekkitsworkshop.net/) CDN |
| `LadyElyssa.taco` | [LadyElyssaTacoTrails](https://github.com/LadyElyssa/LadyElyssaTacoTrails) GitHub Releases |
| `LadyElyssaAP.taco` | [LadyElyssaAchievementGuides](https://github.com/LadyElyssa/LadyElyssaAchievementGuides) GitHub Releases |
| `Hero.Blish.Pack.taco` | [Heros-Marker-Pack](https://github.com/QuitarHero/Heros-Marker-Pack) GitHub Releases (`Hero.Blish.Pack.zip`) |

Credits: **Tekkit's Workshop** (All-In-One — used with permission) · **Lady Elyssa**
([wiki](https://wiki.guildwars2.com/wiki/User:Lady_Elyssa)) · **QuitarHero**
([Hero's Marker Pack](https://github.com/QuitarHero/Heros-Marker-Pack)).

Stamp files (`*.taco.ver`) track the release / size so updates only re-download when needed.
**Your own `.taco` files are never deleted.**

### Blish Pathing behaviors

The addon implements TacO/Blish marker **behaviors** (0–7, 101), **AutoTrigger**,
**hide=/show=** category flips, **tips**, **info** popups, and **copy** clipboard —
enough for Hero's raid/strike tracking and most interactive markers.

**Not supported:** Lua `script-*` features (HTcm simulation scripts, combatFade,
script-trigger practices, etc.). Those still need [Blish HUD Pathing](https://blishhud.com/modules/?module=bh.community.pathing).

Interact default: **Ctrl+Shift+F** (`KB_HELPER_BETA_MARKER_INTERACT`) — rebind in Nexus.
States persist in `marker_behaviors.txt` (Pathing → **Reset marker states** to clear).

Do **not** keep a second copy of Tekkit under another name (e.g. both
`tw_ALL_IN_ONE.taco` and `Tekkit's All-In-One.taco`). That loads every route
twice and stacks in-world GPS. The addon skips known duplicates automatically.

## Trail sections

TacO / Blish / Taimi `.trl` files can contain **section breaks** (a `(0,0,0)`
point). Those end one polyline and start another (e.g. after a portal). The
addon honors breaks on the compass and in-world GPS — stitching them used to
draw straight lines across the whole map.

## Custom packs

1. Copy any `.taco` pack into this folder (same format as TacO / BlishHUD / Taimi).
2. In-game, open **Pathing** and click **Reload packs** (or **Update curated** to refresh downloads too).
3. Enable the categories you want (everything starts unchecked each launch).

### Quick presets

| Control | What it does |
|---------|----------------|
| **Map Completion – Foot / Griffon / Skyscale** | Hearts / POIs / vistas + one route edition (mutually exclusive) |
| **All Tekkit** | Enables the whole Tekkit tree (`tw_guides`) |
| **All Lady** | Enables Lady Elyssa Guides + Achievements (`legs` + `leag`) |
| **All Hero** | Enables Hero's Marker Pack (`HMP` + `hmpSim`) |
| **All off** | Clears every enabled category (Tekkit, Lady, Hero, MC presets) |

**Skyscale routes:** Tekkit only ships a Skyscale Edition for **Heart of Thorns**,
**Secrets of the Obscure**, and generic Routes in **Janthir Wilds**. Core Tyria,
PoF, EoD, Living World, and VoE have no Skyscale path set — use Foot or Griffon
there. The Skyscale preset still enables hearts / POIs / vistas in all MC regions.

Installed path:

```
<Guild Wars 2>/addons/GW2-InGame-Helper/pathing/
```

Taimi / Blish / TacO are **not** required. Fallback discovery still looks for
packs under Minimap Resizer / those installs if our folder is empty.
