# Architecture — GW2 In-Game Helper

**Status:** Published engineering reference (tracked in git).  
Keep this document synchronized when IPC, present, CEF launch, navigation policy, or module boundaries change.

| Field | Value |
|-------|-------|
| Addon revision (shipping) | `2.2.0.0` |
| Signature | `0x48454C50` (`HELP`) |
| IPC | `HLI5` (`0x484C4935`) |
| Helper / home / sites stamps | `2200` / `2200` / `s2200` |
| User-Agent product token | trailing `GW2-InGame-Helper` ([`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md)) |
| CEF | stock Stable **150.0.14** / Chromium **150.0.7871.129** |

Shipping install names are `GW2-InGame-Helper` (DLL + data folder). An optional **Beta** branch (`GW2-InGame-Helper-Beta`) may share this architecture with a distinct `ADDON_NAME` for side-by-side testing. Never loads game CEF and never writes into `bin64/cef`.

**Companion documents:** [`WHITEPAPER.md`](WHITEPAPER.md) (design rationale), [`COMPLIANCE.md`](COMPLIANCE.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`BUILD.md`](BUILD.md), [`../CONTRIBUTING.md`](../CONTRIBUTING.md).

---

## 1. One-line summary

A **Raidcore Nexus** ImGui DLL opens an **out-of-process CEF off-screen (OSR)** browser that paints BGRA frames into **PID-scoped shared memory**. The DLL uploads those frames to a D3D11 texture (via Nexus `SwapChain`) and draws them with ImGui. Players ship **one DLL**; the helper EXE and homepage assets are embedded and extracted on first use. Chromium is a **private copy of stock CEF 150** downloaded once into `addons/<addon-name>/cef/` — not a forked/rebuild Chromium.

---

## 2. Process model

```text
Guild Wars 2.exe
 └─ Nexus loads GW2-InGame-Helper.dll
      ├─ RT_Render → UI_Render → WikiBrowser::PresentFrame / Tick
      ├─ WndProc → keyboard/mouse routing (CEF vs ImGui vs game)
      └─ CreateProcess → GW2HelperBrowser.exe
           └─ LoadLibrary(addons/.../cef/libcef.dll) — windowless OSR
                ├─ renderer / utility children (capped)
                └─ IPC: shared memory + wake event (host PID scoped)
```

| Component | Role |
|-----------|------|
| Host DLL | Nexus addon: ImGui UI, IPC host, D3D11 present, CEF download, settings, Open Ext, pads |
| `GW2HelperBrowser.exe` | CEF client: navigate, OSR paint, find-in-page, BootJs, ad/nav policy |
| `addons/<name>/cef/` | Stock CEF 150 runtime (first-run download / local zip) |

### Embedding and extract

1. **Build:** helper is compiled, copied to `build/helper_blob.exe`, linked into the DLL as a binary blob.
2. **Runtime:** `ExtractHelper()` writes `GW2HelperBrowser.exe` plus `.ver` (`kHelperStamp`).
3. **CEF:** `CefRuntime::EnsureInstalled()` finds or downloads `cef-runtime-150-windows64.zip`, verifies SHA-256, extracts to `cef/`, writes `cef.ver`.
4. **Launch:** `--cef-dir=<addon>/cef` and `--host-pid=<GW2 PID>`. No `bin64/cef` fallback.
5. **Homepage / cheat sheets:** embedded HTML extracted with their own `.ver` stamps (`kHomePageVersion`, etc.).

### Packaging CEF

```bash
make pack-cef   # scripts/pack-cef-runtime.sh
```

Keep `src/CefRuntime.h` URL + SHA256 in sync after uploading a new zip.

### Job Object and host watch

- Helper joined to a Job Object with `KILL_ON_JOB_CLOSE` so children die with the game.
- Helper watches host PID (`SYNCHRONIZE`) and exits if GW2 exits.
- `AF_DisableHotloading` — Nexus must not unload the DLL while CEF is live.

### Multi-client

IPC map / frame / wake names are PID-scoped (`WikiIpcFormatNames` in `WikiIpc.h`).

CEF profile / disk cache: `%LOCALAPPDATA%\<addon-name>\cef-cache` (never under `addons/` or `bin64/cef`).

---

## 3. On-disk layout (shipping names)

```text
<GW2>/addons/GW2-InGame-Helper.dll     # only file players copy
<GW2>/addons/GW2-InGame-Helper/        # runtime data
  GW2HelperBrowser.exe (+ .ver)
  cef/                     # private CEF 150 (downloaded)
  settings.ini
  helper-home.html (+ .ver)
  pathing/                 # curated Tekkit + Lady Elyssa + Hero .taco (+ user packs)
  ei/                      # Elite Insights CLI (optional)
```

See [`BUILD.md`](BUILD.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`COMPLIANCE.md`](COMPLIANCE.md).
The Beta branch (when present) substitutes `GW2-InGame-Helper-Beta` for the DLL basename and data folder.

---

## 4. IPC (`WikiIpcState`, packed)

Shared header: `src/WikiIpc.h` (`#pragma pack(1)`, magic `HLI5`).

| Area | Mechanism |
|------|-----------|
| Commands | Ring `cmd_q` (navigate, tabs, find, quit, bounds, …) |
| Input | Live mouse fields + ring `input_q` (click / wheel / key / focus) |
| Paint | Second mapping: double-buffered BGRA, max **1920×1200**, dirty rect metadata |
| Sync | `frame_seq`, `frame_front`, `frame_reading` (pin while DLL copies) |
| URL / title | Seq-guarded UTF-8 buffers |
| Open Ext | `open_ext_seq` + `open_ext_url[8192]` — helper asks DLL to `ShellExecute` |

Schema changes require bumping magic / coordinating DLL + helper; helper stamp forces re-extract when behavior changes. Treat this triad as a **restricted kernel** — see [`../CONTRIBUTING.md`](../CONTRIBUTING.md).

---

## 5. Present / input (DLL)

**Present (`WikiBrowser::PresentFrame`):**
- Adaptive upload: ~120 Hz while wheel-scrolling, ~60 Hz while interacting, ~30 Hz idle.
- Staging texture → `CopySubresourceRegion` into DEFAULT (never Map the ImGui-bound tex).
- Dirty-rect / chunked row uploads; first paint may block Map so Windows does not stick on “Waiting for first paint…”.
- Fixed max-size texture + UV crop (`FrameUvMax`).

**Input:**
- Overlay WndProc / ImGui feed CEF via IPC; chat-safe routing when helper open.
- Open Ext and Discord deep links prefer **DLL-side** `ShellExecute` (helper often no-ops under Proton).
- Pad ImGui combo popups are avoided where Nexus ate clicks; prefer in-window chips/radios.

---

## 6. CEF helper policy

Stock `libcef.dll`; customization is **client-only** (`src/helper/*`, BootJs, CssCompat/CssProxy).

**Command-line (current):** software OSR (`disable-gpu*`, `disable-d3d11`, `in-process-gpu`), `no-sandbox`, process caps, Discord localhost RPC feature disables, file access for bundled HTML.

**Navigation / ads:**
- Popups always cancelled (OSR has no native windows).
- Ad / tracker / click-id routes → `OpenExternalUrl` (full URL; refuse if still > 8 KB).
- Same-site new-window links may navigate in-tab.
- Native `<select>` polyfilled in-page (BootJs) — PET_POPUP under OSR crashed the helper on Windows.
- `GetViewRect` = ImGui panel; `GetScreenInfo` = primary monitor + work area (`device_scale_factor` = 1.0).

---

## 7. Source map (post-modularization)

### Kernel (high blast radius — paired review)

See [`KERNEL.md`](KERNEL.md) for stamps, playbooks, and in-game checks.

| Path | Responsibility |
|------|----------------|
| `src/entry.cpp` | Nexus load/unload, WndProc, version |
| `src/WikiBrowser.cpp` | Lifecycle, navigate/tabs/input, status |
| `src/WikiBrowserHelper.cpp` | Extract, launch, IPC maps, Open Ext/Tab drains |
| `src/WikiBrowserIpc.cpp` | Cmd/input rings, `about:` URL resolve |
| `src/WikiBrowserPresent.cpp` | D3D11 present / frame getters |
| `src/WikiBrowserShared.h` | Shared DLL host state |
| `src/WikiIpc.h` | Shared memory contract (`HLI5`) |
| `src/helper/main.cpp` | CEF boot, tabs, IPC drain, resource handlers |
| `src/helper/HelperNavPolicy.cpp` | Nav / ad / Open Ext policy |
| `src/helper/HelperOsrRender.cpp` | OSR paint + popup composite |
| `src/helper/HelperInternal.h` | Shared helper state |
| `src/helper/BootJs.h` / `CssCompat.*` / `CssProxy.*` | Injected JS / CSS filters |

### UI chrome

| Path | Responsibility |
|------|----------------|
| `src/UI.cpp` | Orchestration, helper side rail, tabs, CEF input routing |
| `PadNav` / `PadDock` | Side-rail / wrapping chip nav; persisted pad geom |
| `src/UI_Browse.cpp` | Browse picker, section maps, favorites UI |
| `src/UI_Options.cpp` | Nexus options panel |

### Catalog

| Path | Responsibility |
|------|----------------|
| `data/sites.json` | **Canonical** Browse registry (schema v2) |
| `src/SitesLoad.cpp` | Extract/parse runtime `addons/…/sites.json` |
| `src/Sites.cpp` | Active site, favorites, URL match |
| `tools/validate_sites.py` / `enrich_sites_browse.py` | Integrity + hierarchy enrichment |

### Feature modules (pads / data)

| Path | Responsibility |
|------|----------------|
| `LogManagerPad` / `Parse` / `Upload` / `Ei` | DPS Logs UI, EVTC/JSON parse, dps.report, EI CLI |
| `TekkitTrails` / `Parse` / `Index` / `PathingPacks` | Pathing runtime; curated Tekkit + Lady + Hero download; taco/XML/.trl |
| `CompassOverlay` / `WorldOverlay` | Tekkit trail overlays on minimap / in-world |
| `DirectionCompass` | World N/E/S/W + Compass settings pad (independent of Tekkit) |
| `LivePanels` / `Build` / `Html` | about: live digests — workers vs HTML builders |
| `CheatSheets` + `data/cheatsheets/` | Offline about: sheets (embedded zip → runtime extract) |
| `AccountPad`, `WalletPad`, `VaultPad`, `NotesPad`, … | ImGui feature pads |
| `CraftingData`, `ProgressData`, `WaypointsData`, `EventsData` | API / cache helpers |
| `CefRuntime.*` | CEF zip download / verify / extract |
| `Gw2Http.*` | Blocking WinHTTP (worker threads only) |

---

## 8. Platforms

| Target | Notes |
|--------|--------|
| **Windows** | Native PE; primary design target. |
| **Linux (Proton/Wine)** | Same binaries; software CEF flags and DLL-side Open Ext exist largely for this. |

Performance: OSR is CPU-upload heavy; fine for light wiki use; large overlay + scroll on weak CPUs can hitch.

---

## 9. Compliance boundaries

Allowed: Nexus APIs, private CEF under addon dir, local IPC, official API reads, terminate **only** our helper PID.  
Forbidden: game memory R/W, MinHook Present/`d3d11` wrappers, writing `bin64/cef`.  
Details: [`COMPLIANCE.md`](COMPLIANCE.md).

---

## 10. Document control

| Field | Value |
|-------|-------|
| Maintainer | xydroc |
| License | MIT |
| Last architecture sync | 2.2.0.0 / side-rail + pad dock + curated Tekkit/Lady/Hero Pathing + catalog Help News |
| Change trigger | IPC, present, CEF launch, module boundaries, stamps |
