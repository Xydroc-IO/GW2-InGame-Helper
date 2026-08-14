# Pathing — packs, markers, compass, world GPS

**Revision:** 2.2.4.7 · **Audience:** contributors and advanced players  
**Companions:** [`../pathing/README.md`](../pathing/README.md), [`COMPLIANCE.md`](COMPLIANCE.md), [`COMPLETION.md`](COMPLETION.md), [`WHITEPAPER.md`](WHITEPAPER.md) §17.2, [`ARCHITECTURE.md`](ARCHITECTURE.md)

---

## 1. Purpose

Pathing loads TacO / Blish / Taimi **`.taco`** packs, draws compass overlays, supports marker behaviors, route helpers (Find nearest waypoints), and **in-world GPS** ribbons. It does **not** automate movement or teleport.

---

## 2. Curated packs

On first Pathing load / **Update curated**, the addon downloads into `addons/<name>/pathing/`:

| Pack | Source |
|------|--------|
| `tw_ALL_IN_ONE.taco` | Tekkit's Workshop CDN (used with permission) |
| `LadyElyssa.taco` | LadyElyssaTacoTrails GitHub Releases |
| `LadyElyssaAP.taco` | LadyElyssaAchievementGuides GitHub Releases |
| `Hero.Blish.Pack.taco` | Heros-Marker-Pack GitHub Releases |

Stamp files (`*.taco.ver`) avoid redundant downloads. **User `.taco` files are never deleted.** Skip duplicate Tekkit AIO aliases automatically.

Runtime folder: `<GW2>/addons/GW2-InGame-Helper/pathing/`  
Repo notes: [`pathing/README.md`](../pathing/README.md)

---

## 3. Features and categories

### Lady Features (current map)

Under **Lady Elyssa - extras** (independent of Tekkit Map Completion presets):

**Map Completion (independent — can combine):**

| Toggle | Shows |
|--------|--------|
| **Barefoot** | Barefoot trails + markers + Barefoot Shortcut (`bfs`) trails/markers |
| **WP Only** | WP edition trails + markers + shortcuts (`…map.<zone>.wp*`) |
| **With Mounts** | Mount map-completion trails + mount-guide markers/shortcuts |

Enabling any Map Completion / Hearts toggle turns on **`legs.map` / `leag.map`** only (merged — does **not** wipe Tekkit/Hero or enable bounty/fishing/…). Preferred edition when enabling: **Barefoot**. Empty / first-run `PathingEnabled` leaves all categories **off** (no Lady auto-enable on load).

**Other extras (independent):**

| Toggle | Shows |
|--------|--------|
| **Hearts** | Heart trails (`heartpath`) on this map |
| **Hero Point Train** | `legs.hp.*` train trails + icons — same tree as Categories → **Hero Points** |

**Other Lady trees** (Bounty, Fishing, Map Traversal, Ranger Pets, Rift Hunting, Map Enhancements, …) are **not** gated by Features exclusivity — they follow **Categories** whenever `legs` / `leag` (or those subtrees) are enabled.

### Map Completion presets (Tekkit)

Foot / Griffon / Skyscale (mutually exclusive Tekkit route edition) + hearts/POIs/vistas. Skyscale editions exist only for some expansions (see pathing README). Enabling a preset strips only MC paths (and a broad `tw_guides` root, restoring non-MC siblings like Fishing / Map Enhancements when that root was on). Separate from Lady Map Completion under Extras.

### Bulk toggles

All Tekkit / All Lady / All Hero / All off.

Settings persist as `PathingEnabled` (still loads legacy `TekkitEnabled` / `ShowTekkitTrails`).

---

## 4. Marker behaviors

Implemented: behaviors 0–7 and 101, AutoTrigger, hide=/show=, tips, info, copy clipboard.

**Lua (opt-in, default off):** Pathing → Features → Enable Lua scripts. Blish-shaped subset:

| Surface | Supported |
|---------|-----------|
| Attrs | `script-once`, `script-trigger`, `script-filter`, `script-tick`, `script-focus` |
| Marker | fields Position/Alpha/Tint/…; `SetPos*`, `Remove`, `SetTexture(path\|id)`, `Interact`, `GetBehavior` |
| Trail | fields Alpha/Tint/TrailScale/AnimSpeed; `Remove`, `SetTexture(path\|id)` |
| World | `Print`, `MarkerByGuid`, `TrailByGuid`, `GetClosestMarker(s)`, `GetClosestTrail(s)`, `CategoryByType` |
| Pack | `CreateMarker`, `Require`, `SetCategoryEnabled` |
| Menu | `Add(name, onClick, canCheck?, checked?, tooltip?)`, `Remove` — drawn under Pathing → Features |
| Mumble | `PlayerPosition`, `PlayerCharacter`, `PlayerCamera`, `CurrentMap`, `Info` |
| Event | `OnTick(fn)` |
| Other | `Vector3`, `Color.FromArgb`, `User:SetClipboard` |

CDN textures use `assets.gw2dat.com/{id}.png` (Nexus FromURL, or WinHttp → pack icon upload).

Interact: **Ctrl+Shift+F** (Settings → Keybinds → Marker). State file: `marker_behaviors.txt`.

---

## 5. Trail sections

`.trl` `(0,0,0)` points are **section breaks** — end one polyline, start another. Honored on compass and world GPS (no map-wide stitches through portals).

---

## 6. Route → Find nearest waypoints

Uses public waypoint index + chat codes (copy only — no auto-teleport).

- **No categories:** anchor on Mumble continent position; orange guide to closest WP.
- **Categories on:** prefer trail start when available.
- Does not block on pack indexing before listing.

---

## 7. Direction compass vs world GPS

| | Compass pad | World GPS |
|--|-------------|-----------|
| UI | Side-rail **Compass** | Pathing Overview → In-world GPS |
| Draw | ImGui / Nexus fonts (N/E/S/W) | D3D11 upright ribbons |
| Purpose | Cardinal orientation | Follow trail geometry |

Independent systems; both may use Mumble pose read-only.

**Related overlays (not Pathing pad UI):** Completion / Farming routes set the orange **search guide**. `src/overlay/GpsArrow` draws a floating arrow toward that guide; `ZoneBanner` shows a short zone-entry toast on map change. See [`COMPLETION.md`](COMPLETION.md).

---

## 7b. Map assist (opt-in)

Pathing Overview → **Map assist (pan)** (default **off**). Optional **Click waypoint after pan** (default **off**).

| | Behavior |
|--|----------|
| Data | MumbleLink `mapCenter` / `mapScale` + continent target from Pathing Guides |
| Input | OS cursor `SendInput` **only while the fullscreen world map is open** |
| UI | Pathing → Route → **Pan map** / **Travel** when assist is enabled |
| Teleport | Opens GW2’s native prompt only; **never** auto-confirms |

Implementation: `src/pathing/mapassist/MapAssist*` + `src/app/GameLive*` (UITick freshness). Policy: [`COMPLIANCE.md`](COMPLIANCE.md) § Allowed exception — world-map assist.

---

## 8. World GPS (D3D)

### Compliance invariants

- Device from Nexus **`SwapChain` only**.
- **No** Present / `d3d11.dll` hooks.
- **No** game depth buffer / camera matrix reads from process memory.
- Pose from MumbleLink / Nexus DataLink **read-only**.

### Visual model

- Blish-style upright strips + pack chevron textures.
- Base half-width \(20''\) (`WorldGpsMath::kBlishHalfM`) × soft-clamped pack `trailScale` × user **GPS width** (default **1.0×** = authored scale; Lady width-bias removed).
- Fixed UV tile period; animspeed flow **forward** along route (heart trails use the same scroll + pack yellow tint; **1.5×** ribbon width).
- Soft **Player clear** (default 1; **0** = full path).
- Soft **Marker clear** (default 1; **0** = keep icons at feet) — ~2–5.5 m hole for
  world markers; Mount / Barefoot shortcuts use a smaller bubble.
- **GPS range (m)** (default 120; Overview slider 40–200) is the real draw/activation
  radius — not floored to hundreds of meters.
- **Marker size:** Separate Overview sliders —
  **World markers** (`WorldMarkerScale`, default **2.0×**) for in-world GPS icons, and
  **Compass icons** (`CompassMarkerScale`, default **1.0×**) for the stock compass.
- Along-path sampling; sticky cache + hysteresis reduce blink / incomplete sparse routes (match by geometry; prefer full TacO sections for nearby hearts).
- Trail textures (including `Line - Heart`) are prioritized in the icon queue; hearts without a loaded texture are skipped (no solid-color fallback).
- **Search-guide pathfinding** (`PathingPathfind`): A* over pack-trail polylines + official
  waypoints when PreferTrail / Completing routes rebuild the orange guide; capped graph and
  rebuild hysteresis keep lock time bounded. Falls back to densified/smoothed direct.

### Module map

| File | Role |
|------|------|
| `WorldOverlay.cpp` | Thin orchestrator |
| `WorldGpsMath.*` | View-proj helpers, width, fade, UV constants |
| `WorldGpsD3dDevice.cpp` | Device/shaders from SwapChain; runtime `D3DCompile` |
| `WorldGpsD3dDraw.cpp` | Ribbon build/draw, textures, flow |
| `WorldGpsImgui.*` | Markers only (no trail billboards) |
| `PathingPathfind.*` | Trail-graph A* for search-guide rebuild |

If D3D init fails (missing `d3dcompiler` under some Wine setups), world trails **do not** fall back to ImGui billboards.

### HLSL note

Use `mul(M,v)` consistently. Wrong multiply order can “succeed” while drawing off-screen and skipping ImGui fallbacks — treat as severity-high GPS bug.

---

## 8b. Pack authoring (not in this addon)

Helper Pathing **plays** TacO/Blish packs. Authoring moved to the standalone
[GW2-TrailTools](https://github.com/Xydroc-IO/GW2-TrailTools) Nexus addon
(Editor, OverlayData, UberTool, ground snap). Install `GW2-TrailTools.dll`
beside the helper; do not load both as competing world gizmos if you only
need playback.

Built `.taco` files still drop into `addons/GW2-InGame-Helper/pathing/` (or
copy them there) and **Reload packs**.

Pathing still honors Blish `schedule` / `schedule-duration` (UTC cron) at draw
time. Lua: see §4; enable under Pathing Features.

---

## 9. Contributor edit guide

| Change | Start here |
|--------|------------|
| Pack download / stamps | `PathingPacks*` |
| `.trl` / zip parse | `PathingParse*` |
| Pack authoring | standalone [GW2-TrailTools](https://github.com/Xydroc-IO/GW2-TrailTools) |
| Categories / Features UI | `PathingTrailsUi` / presets TUs |
| GPS math / width | `WorldGpsMath` |
| GPS D3D | `WorldGpsD3d*` — compliance review if touching device acquisition |
| Map assist (opt-in) | `MapAssist*` + `GameLive*` |
| Markers | `MarkerBehaviors*` |
| Waypoint index | `WaypointsData*` |
| Completion GPS handoff | `CompletionRoute` + Pathing search guide ([`COMPLETION.md`](COMPLETION.md)) |
| Floating guide arrow / zone banner | `GpsArrow`, `ZoneBanner` (`src/overlay/`) |

Prefer ≤500 lines per `.cpp` (see contributor module notes in [`ARCHITECTURE.md`](ARCHITECTURE.md) / [`ONBOARDING.md`](ONBOARDING.md)).
