# Architecture — GW2 In-Game Helper

**Status:** Published engineering reference (tracked in git).  
Keep this document synchronized when IPC, present, CEF launch, navigation policy, world GPS compliance surface, or module boundaries change.

| Field | Value |
|-------|-------|
| Addon revision (shipping) | `2.3.0.5` |
| Signature | `0x48454C50` (`HELP`) |
| IPC | `HLI5` (`0x484C4935`) |
| Helper / home / sites / cheatsheets stamps | `2247` / `2238` / `s2215` / `c2228` |
| Live panel stamp | `86` |
| Raid food stamp | `9` |
| ui-chrome stamp | `uc36` |
| watchd stamp | `w10` |
| User-Agent product token | trailing `GW2-InGame-Helper` ([`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md)) |
| CEF | stock Stable **150.0.14** / Chromium **150.0.7871.129** |

Shipping install names are `GW2-InGame-Helper` (DLL + data folder). An optional **Beta** branch (`GW2-InGame-Helper-Beta`) may share this architecture with a distinct `ADDON_NAME` / signature `HELB` for side-by-side testing. Never loads game CEF and never writes into `bin64/cef`.

**Companion documents:** [`WHITEPAPER.md`](WHITEPAPER.md) (design rationale), [`KERNEL.md`](KERNEL.md) (change playbooks), [`COMPLIANCE.md`](COMPLIANCE.md), [`BROWSER_LIMITS.md`](BROWSER_LIMITS.md), [`NAV_AND_ADS.md`](NAV_AND_ADS.md), [`PATHING.md`](PATHING.md), [`ACCOUNT.md`](ACCOUNT.md), [`MODULES.md`](MODULES.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`BUILD.md`](BUILD.md), [`ONBOARDING.md`](ONBOARDING.md).

---

## 1. One-line summary

A **Raidcore Nexus** ImGui DLL opens an **out-of-process CEF off-screen (OSR)** browser that paints BGRA frames into **PID-scoped shared memory**. The DLL uploads those frames to a D3D11 texture (via Nexus `SwapChain`) and draws them with ImGui. Separately, **Pathing world GPS** draws Blish-style camera-facing ribbons with the same SwapChain device (no Present hooks). Players ship **one DLL**; the helper EXE and homepage assets are embedded and extracted on first use. Chromium is a **private copy of stock CEF 150** downloaded once into `addons/<addon-name>/cef/`.

---

## 2. Process model

```text
Guild Wars 2.exe
 └─ Nexus loads GW2-InGame-Helper.dll
      ├─ RT_Render → UI_Render → WikiBrowser::PresentFrame / Tick
      │                └─ Pathing WorldOverlay / WorldGpsD3d draw (SwapChain device)
      ├─ WndProc → keyboard/mouse routing (CEF vs ImGui vs game)
      └─ CreateProcess → GW2HelperBrowser.exe
           └─ LoadLibrary(addons/.../cef/libcef.dll) — windowless OSR
                ├─ renderer / utility children (capped)
                └─ IPC: shared memory + wake event (host PID scoped)
```

| Component | Role |
|-----------|------|
| Host DLL | Nexus addon: ImGui UI, IPC host, D3D11 CEF present, world GPS, CEF download, settings, Open Ext, pads |
| `GW2HelperBrowser.exe` | CEF client: navigate, OSR paint, find-in-page, BootJs, ad/nav policy |
| `addons/<name>/cef/` | Stock CEF 150 runtime (first-run download / local zip) |

### Embedding and extract

1. **Build:** helper compiled → `build/bin/GW2HelperBrowser.exe` → flattened copy `build/embed/helper_blob.exe` → linked into the DLL as a binary blob.
2. **Runtime:** `ExtractHelper()` writes `GW2HelperBrowser.exe` plus `.ver` (`kHelperStamp`).
3. **CEF:** `CefRuntime::EnsureInstalled()` finds or downloads `cef-runtime-150-windows64.zip`, verifies SHA-256, extracts to `cef/`, writes `cef.ver`.
4. **Launch:** `--cef-dir=<addon>/cef` and `--host-pid=<GW2 PID>`. No `bin64/cef` fallback. CreateProcess stays off `RT_Render`.
5. **Homepage / cheat sheets / sites:** embedded assets extracted with their own `.ver` stamps.

### Packaging CEF

```bash
make pack-cef   # scripts/pack-cef-runtime.sh
```

Keep `src/browser/CefRuntime.h` URL + SHA256 in sync after uploading a new zip. Upload to the same `gw2-helper-catalog` pre-release as the public catalog pack (not shipping DLL tags). Details: [`CATALOG_RELEASE.md`](CATALOG_RELEASE.md).

### GW2 Helper Catalog + CEF (GitHub, no API keys)

Public item/currency names, render icons, station recipes, achievement groups/defs, and the CEF runtime zip are **not** bundled in the DLL. The addon downloads them from **one pre-release** tag `gw2-helper-catalog` (title **GW2 Helper Catalog**; do **not** attach these to every shipping DLL tag):

- `gw2-helper-catalog.manifest` — `{ "catalog", "icons", "cef" }` cheap freshness check
- `gw2-helper-catalog.igh` — IGH1 (not zip) with raw `names-en.tsv` + `recipes.tsv` (~8MB; not gzip members)
- `gw2-helper-recipes.igh` — IGH1 raw `recipes.tsv` (~700KB); crafting can load this before the full names pack
- `gw2-helper-achievements.igh` — IGH1 raw `groups.tsv` / `categories.tsv` / `defs.tsv`; Achievements pad uses this, then `/v2/account/achievements` for progress
- `gw2-helper-icons.igh` — IGH1 unique `render.guildwars2.com` PNGs (~22k); missing icons still use the CDN
- `cef-runtime-150-windows64.zip` — private CEF 150 (SHA-256 in `CefRuntime.h`; stays a zip)

Catalog URLs in `src/api/Gw2Catalog.h`; CEF URL in `src/browser/CefRuntime.h`. Cache: `addons/.../cache/gw2-helper-catalog.manifest` + names/recipes TSV + catalog/recipes/achievements/icon packs. A cold first fetch pulls every missing `.igh` in one worker. The GitHub manifest is GETed **once per shipping DLL version** (not every 6 hours); later sessions skip packs whose ids already match. Rebuild: `python3 scripts/build-gw2-catalog.py -o dist` and `python3 scripts/build-gw2-icons.py -o dist` (icons are hand-uploaded; daily Action refreshes the names, recipes, and achievements packs — players pick those up on the next DLL). Account bank/mats still use the player’s key on the PC.

### Job Object and host watch

- Helper joined to a Job Object with `KILL_ON_JOB_CLOSE` so children die with the game.
- Helper watches host PID (`SYNCHRONIZE`) and exits if GW2 exits.
- Hot-unload allowed (`AF_None`) — `AddonUnload` closes pads and stops CEF before deregistering UI. Prefer a full GW2 restart after replacing the DLL on disk.

### Multi-client

IPC map / frame / wake names are PID-scoped (`WikiIpcFormatNames` in `WikiIpc.h`):

```text
Local\GW2InGameHelper_CEF_IPC_v5_<pid>
Local\GW2InGameHelper_CEF_FRAME_v5_<pid>
Local\GW2InGameHelper_CEF_WAKE_v5_<pid>
```

Host creates maps/events with a **current-user DACL** (`WikiBrowserIpcSec.cpp`); falls back to
Win32 defaults if ACL APIs fail (Wine/Proton). Helper opens as the same user.

CEF profile / disk cache: `%LOCALAPPDATA%\<addon-name>\cef-cache` (never under `addons/` or `bin64/cef`).

---

## 3. On-disk layout (shipping names)

```text
<GW2>/addons/GW2-InGame-Helper.dll     # only file players copy
<GW2>/addons/GW2-InGame-Helper/        # runtime data
  GW2HelperBrowser.exe (+ .ver)
  settings.ini
  sites.json (+ .ver)
  config/                  # notes, profiles, themes/, session history, waypoints,
                           # log-index, marker behaviors, ei-helper.conf,
                           # favorites.json
  pages/                   # generated HTML + home assets
    helper-home.html (+ .ver), home-logo.png, home-cover.jpg
    raid-food.html, live-*.html / .ver / .ok, gw2-api-check.*
  live/cache/              # live-*.json API caches + live-leg-craft-*.json
  cache/                   # unlocks-*.cache, stash-names.cache, waypoints-index.cache,
                           # gw2-names-en.tsv, gw2-recipes.tsv, gw2-helper-catalog.manifest,
                           # gw2-helper-catalog.igh, gw2-helper-recipes.igh,
                           # gw2-helper-achievements.igh,
                           # gw2-helper-icons.igh
  cmds/                    # *-cmd.txt (helper ↔ DLL IPC)
  cef/                     # private CEF 150 (downloaded)
  cheatsheets/
  pathing/                 # curated Tekkit + Lady + Hero + HasKha .taco (+ user packs)
  ei/                      # Elite Insights CLI (optional)
  ei-out/                  # Elite Insights parse output (optional)
```

Beta substitutes `GW2-InGame-Helper-Beta` for DLL basename and data folder.

---

## 4. IPC (`WikiIpcState`, packed)

Shared header: [`src/browser/WikiIpc.h`](../src/browser/WikiIpc.h) (`#pragma pack(1)`, magic `HLI5`).

### 4.1 Subsystems

| Area | Mechanism |
|------|-----------|
| Commands | Ring `cmd_q[32]` — navigate, tabs, find, quit, bounds, visible, … |
| Input | Live mouse fields + ring `input_q[256]` (click / wheel / key / focus) |
| Paint | Second mapping: double-buffered BGRA, max **1920×1200**, dirty rect metadata |
| Sync | `frame_seq`, `frame_front`, `frame_reading` (pin while DLL copies) |
| URL / title | Seq-guarded UTF-8 buffers (odd = writing, even = stable) |
| Open Ext | `open_ext_seq` + `open_ext_url[8192]` — helper asks DLL to `ShellExecute` |
| Open Tab | `open_tab_seq` + `open_tab_url[2048]` — new in-addon tab |

### 4.2 Command enum (sketch)

`NONE`, `NAVIGATE`, `BACK`, `FORWARD`, `RELOAD`, `HOME`, `QUIT`, `SET_BOUNDS`, `SET_VISIBLE`, `CREATE_TAB`, `ACTIVATE_TAB`, `CLOSE_TAB`, `FIND`, `STOP_FIND`.

### 4.3 Frame pin protocol

1. Helper finishes painting buffer *i*, sets `frame_front = i`, bumps `frame_seq`, publishes dirty rect, signals wake.
2. DLL sets `frame_reading = frame_front`, copies, then sets `frame_reading = 0xFFFFFFFF`.
3. Helper must not overwrite a buffer whose index equals `frame_reading`.

### 4.4 Schema discipline

Schema changes require bumping magic / coordinating DLL + helper; helper stamp forces re-extract when behavior changes. Treat this triad as a **restricted kernel** — [`KERNEL.md`](KERNEL.md). Host CI: `make test-ipc`.

Analytical depth: [`WHITEPAPER.md`](WHITEPAPER.md) §5.

---

## 5. Present / input (DLL)

### 5.1 CEF present (`WikiBrowser::PresentFrame`)

- Adaptive upload: ~120 Hz wheel-scrolling, ~60 Hz interacting, ~30 Hz idle.
- Staging texture → `CopySubresourceRegion` into DEFAULT (**never Map** the ImGui-bound tex).
- Dirty-rect / chunked row uploads; first paint may block Map (Windows first-paint stall).
- Fixed max-size texture + UV crop (`FrameUvMax`).

### 5.2 Input

- Overlay WndProc / ImGui feed CEF via IPC; chat-safe routing when helper open.
- Open Ext and Discord deep links prefer **DLL-side** `ShellExecute` (helper often no-ops under Proton).
- Prefer in-window chips/radios over ImGui combo popups where Nexus ate clicks.
- Fast typing: `ToUnicode` synthesis when TranslateMessage is skipped.

### 5.3 World GPS present (separate pipeline)

`WorldGpsD3d*` obtains `ID3D11Device` / context from Nexus SwapChain, runtime-compiles HLSL, draws camera-facing ribbons (lift + no near-plane clip). Markers: `WorldGpsImgui`. Orchestrator: `WorldOverlay`. **No** Present hooks. See [`PATHING.md`](PATHING.md).

---

## 6. CEF helper policy

Stock `libcef.dll`; customization is **client-only** (`src/helper/*`, BootJs, CssCompat/CssProxy).

**Command-line (current):** software OSR (`disable-gpu*`, `disable-d3d11`, `in-process-gpu`), `no-sandbox`, process caps, Discord localhost RPC feature disables, file access for bundled HTML.

**Navigation / ads** (detail: [`NAV_AND_ADS.md`](NAV_AND_ADS.md)):

- Popups always cancelled (OSR has no native windows).
- Ad / tracker / click-id routes → `OpenExternalUrl` (full URL; refuse if still > 8 KB).
- Same-site new-window links may navigate in-tab.
- `file://` / `about:` new tabs only from bundled helper pages (`file:` / `about:` other than `about:blank`). Web origins cannot open local files. Host chrome (Home / rail) may `load_url` only helper `pages/*.html` and `cheatsheets/*.html`.
- Native `<select>` polyfilled in-page (BootJs) — PET_POPUP under OSR crashed the helper on Windows.
- `GetViewRect` = ImGui panel; `GetScreenInfo` = primary monitor + work area (`device_scale_factor` = 1.0).

---

## 7. Source map (hybrid layout)

`src/` is organized as **shared layers** (`app`, `ui`, `api`, `browse`, `browser`, `helper`) plus **feature domains** (`account`, `pathing`, `logs`, `economy`, `instances`, `completion`, `overlay`, `events`, `notes`). Includes stay flat (`#include "Foo.h"`) via multiple `-Isrc/...` paths.

**Module size:** Prefer **≤500 lines** per `.cpp`. Split by concern (pad vs data vs fetch vs parse). Generated / blob headers (`BootJs.h`, icon embeds) are exempt. See [`MODULES.md`](MODULES.md).

### Kernel (high blast radius — paired review)

| Path | Responsibility |
|------|----------------|
| `src/entry.cpp` (+ `entryLoad` / `entryUnload` / `entryWndProc` / `entryHotkeys`) | Nexus load/unload, WndProc, hotkeys, version |
| `src/browser/WikiBrowser.cpp` (+ `WikiBrowserApi`) | Lifecycle, navigate/tabs/input, status |
| `src/browser/WikiBrowserHelper.cpp` (+ Lifecycle / Launch) | Extract, launch, IPC maps, Open Ext/Tab drains |
| `src/browser/WikiBrowserIpc.cpp` | Cmd/input rings, `about:` URL resolve |
| `src/browser/WikiBrowserPresent.cpp` | D3D11 present / frame getters |
| `src/browser/WikiBrowserShared.h` | Shared DLL host state |
| `src/browser/WikiIpc.h` | Shared memory contract (`HLI5`) |
| `src/browser/CefRuntime.*` (+ Fs / Http / Verify) | CEF zip download / verify / extract |
| `src/helper/main.cpp` (+ State / Paths / Resolve / BrowseActions / Tabs / Handlers / Commands) | CEF boot, tabs, IPC drain, resource handlers |
| `src/helper/HelperNavPolicy.cpp` (+ Handlers) | Nav / ad / Open Ext policy |
| `src/helper/HelperOsrRender.cpp` | OSR paint + popup composite |
| `src/helper/HelperInternal.h` | Shared helper state |
| `src/helper/BootJs.h` / `CssCompat.*` / `CssProxy.*` | Injected JS / CSS filters |

### Shared layers

| Path | Responsibility |
|------|----------------|
| `src/app/` | `Globals`, `Settings`, `AddonPaths`, **`AddonVersion.h`**, `HelperTheme`, **`UserTheme`** (config/themes), `PadDock`, `PadNav`, `PadLayout`, `MumbleIdentity`, **`AspectLayout`**, **`PanelBinds`**, **`UiChrome`**, **`Gw2Ui`** / **`Gw2Icons`**, **`GameLive`** (UITick / Mumble freshness for overlays) |
| `src/ui/` | `chrome/` (incl. side rail), `browse/`, `settings/`, `quickaccess/` (ImGui helper chrome + SettingsPad) |
| `src/api/` | `Gw2Http` (blocking WinHTTP — worker threads only; identity encoding, no `Accept-Encoding` gzip — Wine otherwise returns compressed JSON); **`ApiBudget`** (max concurrent HTTP); **`BgFetch`** (channel wanted/busy arbitration); **`JsonView.h`** (bounds-checked GW2 JSON scrapers) |
| `src/browse/` | Homepage / CheatSheets / RaidFood at root; `sites/`, `livepanels/`, `tabs/` for catalog, HTML builders, tabs |

### Feature domains

| Path | Responsibility |
|------|----------------|
| `src/account/` | Account hub + profiles/inventory/history; feature pads under `crafting/`, `tpwatch/`, `unlocks/`, `progress/` (Ledger-grouped armory), `wallet/` (stash/materials), … |
| `src/pathing/` | Feature subfolders: `packs/`, `trails/`, `world/`, `lua/`, `waypoints/`, `mapassist/` |
| `src/pathing/world/` (`WorldOverlay*`, `WorldGps*`, …) | GPS orchestrator, math, D3D device/draw, ImGui markers |
| `src/logs/` | `logmanager/` (DPS Logs: list, Stats, career Players/Guilds, EI parse, dps.report, KillProof) + `eiruntime/` |
| `src/economy/` | Flip Finder, local charts, crafting cart (read-only) |
| `src/instances/` | Story / fractal / raid / strike journal |
| `src/completion/` | Achievements pad (catalog pack groups/defs + account overlay) |
| `src/overlay/` | Floating GPS arrow (`GpsArrow`) + zone-entry banner (`ZoneBanner`) |
| `src/events/` | World Events pad + schedule data |
| `src/notes/` | Notes + waypoint snippets pad |

Same pattern as `WikiBrowserShared.h`: public `.h` stable; `*Shared.h` / `PathingIndex.h` for cross-TU decls; **one** TU defines Shared globals.

### Catalog data (not under `src/`)

| Path | Responsibility |
|------|----------------|
| `data/sites.json` | **Canonical** Browse registry (schema v2) |
| `data/cheatsheets/` | Offline about: sheets (embedded zip → runtime extract) |
| `tools/validate_sites.py` / `enrich_sites_browse.py` | Integrity + hierarchy enrichment |
| `pathing/` (repo root) | Curated `.taco` packs + README (runtime copies under addons) |

---

## 8. Platforms

| Target | Notes |
|--------|--------|
| **Windows** | Native PE; primary design target. |
| **Linux (Proton/Wine)** | Same binaries; software CEF flags and DLL-side Open Ext exist largely for this. World GPS needs `d3dcompiler_*.dll`. Soft-open / soft-stop and `CrashTrail` (`Crash-Logs/` + timestamped tip folders) pin hard tips that leave empty SEH dumps. |

Performance: OSR is CPU-upload heavy; fine for light wiki use; large overlay + scroll on weak CPUs can hitch.

---

## 9. Compliance boundaries

Allowed: Nexus APIs, private CEF under addon dir, local IPC, official API reads, MumbleLink read-only for overlays, SwapChain D3D world GPS, terminate **only** our helper PID; opt-in world-map assist (`MapAssist`, default off — see COMPLIANCE).  
Forbidden: game memory R/W for cheating, MinHook Present/`d3d11` wrappers, writing `bin64/cef`, synthetic input into GW2 **outside** the documented map-assist exception.  
Details: [`COMPLIANCE.md`](COMPLIANCE.md).

---

## 10. Document control

| Field | Value |
|-------|-------|
| Maintainer | xydroc |
| License | MIT |
| Last architecture sync | 2.3.0.5 — Stash CharFetchInv UAF fix; + / Ctrl+T → Browse hub; stamps unchanged (helper 2247; live 86) |
| Change trigger | IPC, present, CEF launch, module boundaries, stamps, GPS compliance surface |
