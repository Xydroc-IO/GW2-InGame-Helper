# Pathing — packs, markers, compass, world GPS

**Revision:** 2.2.0.3 · **Audience:** contributors and advanced players  
**Companions:** [`../pathing/README.md`](../pathing/README.md), [`COMPLIANCE.md`](COMPLIANCE.md), [`WHITEPAPER.md`](WHITEPAPER.md) §17.2, [`ARCHITECTURE.md`](ARCHITECTURE.md)

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

### Lady Features (mutually exclusive)

Barefoot / With Mounts / WP Only — one edition at a time. Enabling an edition turns on Lady categories when needed. Default: **Barefoot**. Empty first-run `PathingEnabled` auto-enables Lady categories.

### Map Completion presets

Foot / Griffon / Skyscale (mutually exclusive Tekkit route edition) + hearts/POIs/vistas. Skyscale editions exist only for some expansions (see pathing README).

### Bulk toggles

All Tekkit / All Lady / All Hero / All off.

Settings persist as `PathingEnabled` (still loads legacy `TekkitEnabled` / `ShowTekkitTrails`).

---

## 4. Marker behaviors

Implemented: behaviors 0–7 and 101, AutoTrigger, hide=/show=, tips, info, copy clipboard.  
**Not supported:** Lua `script-*` (use Blish HUD Pathing for those).  
Interact: **Ctrl+Shift+F** (`KB_HELPER_MARKER_INTERACT`). State file: `marker_behaviors.txt`.

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
- Along-path sampling; sticky cache + hysteresis reduce blink / incomplete sparse routes (match by geometry; prefer full TacO sections for nearby hearts).
- Trail textures (including `Line - Heart`) are prioritized in the icon queue; hearts without a loaded texture are skipped (no solid-color fallback).

### Module map

| File | Role |
|------|------|
| `WorldOverlay.cpp` | Thin orchestrator |
| `WorldGpsMath.*` | View-proj helpers, width, fade, UV constants |
| `WorldGpsD3dDevice.cpp` | Device/shaders from SwapChain; runtime `D3DCompile` |
| `WorldGpsD3dDraw.cpp` | Ribbon build/draw, textures, flow |
| `WorldGpsImgui.*` | Markers only (no trail billboards) |

If D3D init fails (missing `d3dcompiler` under some Wine setups), world trails **do not** fall back to ImGui billboards.

### HLSL note

Use `mul(M,v)` consistently. Wrong multiply order can “succeed” while drawing off-screen and skipping ImGui fallbacks — treat as severity-high GPS bug.

---

## 9. Contributor edit guide

| Change | Start here |
|--------|------------|
| Pack download / stamps | `PathingPacks*` |
| `.trl` / zip parse | `PathingParse*` |
| Categories / Features UI | `PathingTrailsUi` / presets TUs |
| GPS math / width | `WorldGpsMath` |
| GPS D3D | `WorldGpsD3d*` — compliance review if touching device acquisition |
| Markers | `MarkerBehaviors*` |
| Waypoint index | `WaypointsData*` |

Prefer ≤500 lines per `.cpp` ([`MODULES.md`](MODULES.md)).
