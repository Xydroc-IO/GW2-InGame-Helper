# Pathing packs

Put TacO / BlishHUD **`.taco`** files in this folder. The addon loads all of them.

**Full contributor reference:** [`docs/PATHING.md`](../docs/PATHING.md) (Features, GPS module map, compliance).
Engineering systems map: [`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) · design rationale: [`docs/WHITEPAPER.md`](../docs/WHITEPAPER.md) §17.2.

## Curated packs (auto-updated)

On first Pathing load (and when you click **Update curated**), the addon downloads
three shipping packs into this folder (plus any `.taco` files you add yourself):

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

**Not supported by default:** Lua is **opt-in** (Pathing → Features → Enable Lua scripts,
default off). When enabled, a Blish-shaped subset runs (`script-*` attrs, Marker/Trail,
Menu, CDN textures, Pack:CreateMarker). Full Blish libdef hosts (Storage, Instance,
GameTime, achievement auto-hide, etc.) still need
[Blish HUD Pathing](https://blishhud.com/modules/?module=bh.community.pathing).

Interact default: **Ctrl+Shift+F** (`KB_HELPER_MARKER_INTERACT`) — rebind in Nexus.
States persist in `config/marker_behaviors.txt` (Pathing → **Reset marker states** to clear).

Do **not** keep a second copy of Tekkit under another name (e.g. both
`tw_ALL_IN_ONE.taco` and `Tekkit's All-In-One.taco`). That loads every route
twice and stacks in-world GPS. The addon skips known duplicates automatically.

## Trail sections

TacO / Blish / Taimi `.trl` files can contain **section breaks** (a `(0,0,0)`
point). Those end one polyline and start another (e.g. after a portal). The
addon honors breaks on the compass and in-world GPS — stitching them used to
draw straight lines across the whole map.

## Route → Find nearest waypoints

Pathing **Route** finds public API waypoints near an anchor and lists chat codes
(copy only — no auto-teleport).

- **No categories enabled:** anchors on your Mumble continent position; orange
  guide points at the closest waypoint (search trails may load in the background).
- **Categories enabled:** prefers the trail start when one is on the map; otherwise
  falls back to your position.
- Does **not** wait for pack indexing to finish before listing waypoints.

## Custom packs

1. Copy any `.taco` pack into this folder (same format as TacO / BlishHUD / Taimi).
2. In-game, open **Pathing** and click **Reload packs** (or **Update curated** to refresh downloads too).
3. Enable the categories / Features edition you want (first run may auto-enable Lady).

### Trail Tools (author packs)

Side-rail **Trail Tools** creates packs without TacO/TrlTool. Trails and Markers are
**separate windows** so you can keep both open while placing POIs along a route:

1. **Trail Tools** — Live coords + Pack (Looks, import, Build `.taco`).
2. **Trails** — Insert Map / Vector, section breaks, save `.trl`.
3. **Markers** — Drop POIs, nudge XYZ, type/behavior (open beside Trails).

Workspace: `pathing/authoring/<PackName>/` under the addon data folder. Drop PNG icons into
`Data/<PackName>/Markers/` yourself. Textured draft preview shows on compass + world GPS while
any of those pads is open. Details: [`docs/PATHING.md`](../docs/PATHING.md) §8b.

### Quick presets

| Control | What it does |
|---------|----------------|
| **Map Completion – Foot / Griffon / Skyscale** | Hearts / POIs / vistas + one Tekkit route edition (mutually exclusive) |
| **Lady – Barefoot / With Mounts / WP Only** | One Lady map-completion edition at a time (Features); enabling one turns Lady categories on when needed |
| **All Tekkit** | Enables the whole Tekkit tree (`tw_guides`) — merges with Lady/Hero |
| **All Lady** | Enables Lady Elyssa Guides + Achievements (`legs` + `leag`) — merges with Tekkit/Hero |
| **All Hero** | Enables Hero's Marker Pack (`HMP` + `hmpSim`) — merges |
| **All off** | Clears every enabled category (Tekkit, Lady, Hero, MC presets) |

Default Features edition is **Barefoot**. First run with an empty category list
auto-enables Lady so Core routes show without hunting Categories.

### In-world GPS

Pathing **GPS width** (default **1.0×**) multiplies pack `trailScale` so **1.0**
matches Blish / TacO at authored scale (no per-edition width bias). Compass
thickness uses the same pack scale × slider.

**Renderer:** Nexus SwapChain **D3D11** world-space ribbons (Blish-style upright
strips + pack chevron textures, UV flow, soft player clear). Markers stay on
ImGui. No Present / `d3d11.dll` hooks — device comes from `AddonAPI::SwapChain`
only (see [`docs/COMPLIANCE.md`](../docs/COMPLIANCE.md)). If D3D init fails
(missing `d3dcompiler_*.dll` under some Wine setups), world trails do not fall
back to ImGui billboards.

**Sampling:** Nearby snippets grow by **along-path** meters from the nearest
vertex; sticky cache + hysteresis reduce blink at range edges. TacO `.trl`
section breaks are honored (no map-wide stitches).

**Player clear** (default **1**) fades the ribbon near you; **0** shows the full
path. Range / width / Player clear sit under Overview → In-world GPS.

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
