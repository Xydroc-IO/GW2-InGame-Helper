# Embedding a Contemporary Chromium Browser in a Live Game Client

## Design Rationale, Constraints, and Trade-offs of an Out-of-Process Off-Screen Overlay for Guild Wars 2

| Field | Value |
|-------|-------|
| Document type | Technical report (engineering whitepaper) |
| Product | GW2 In-Game Helper |
| Revision described | 2.2.0.11 |
| Nexus signature | `HELP` (`0x48454C50`) |
| IPC contract | `HLI5` (`0x484C4935`) |
| Runtime | Chromium Embedded Framework (CEF) Stable 150.0.14 / Chromium 150.0.7871.129 |
| Author | xydroc |
| Affiliation | Independent open-source project (MIT License) |
| Peer review | None (project technical report; not a journal article) |
| License of this document | MIT (with the repository) |
| Companions | [`ARCHITECTURE.md`](ARCHITECTURE.md), [`KERNEL.md`](KERNEL.md), [`COMPLIANCE.md`](COMPLIANCE.md), [`NAV_AND_ADS.md`](NAV_AND_ADS.md), [`PATHING.md`](PATHING.md), [`MODULES.md`](MODULES.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`../CONTRIBUTING.md`](../CONTRIBUTING.md) |

### Revision history (document)

| Report rev | Addon | Salient documentation focus |
|------------|-------|-----------------------------|
| 2.2.0.11 | 2.2.0.11 | Legendary Ledger auto armory refresh; Wiki opens in new helper tab |
| 2.2.0.10 | 2.2.0.10 | Nested pages/live/cache/cmds addon data layout + upgrade migration |
| 2.2.0.9 | 2.2.0.9 | Live Legendary Ledger; Cheat Sheets hub; craft-tree Sync; about: nav hardening |
| 2.2.0.8 | 2.2.0.8 | GW2 Legendary Ledger sheet; Crafting Ledger Tools link |
| 2.2.0.7 | 2.2.0.7 | JsonView; World GPS controls; marker clear; pad scrollbar gutter |
| 2.2.0.6 | 2.2.0.6 | Side-rail GW2 API Check; parallel official API probes |
| 2.2.0.5 | 2.2.0.5 | Pad text wrap + DPS filter layout; Lady/Tekkit enable merge; MC presets |
| 2.2.0.4 | 2.2.0.4 | Lady Features Hearts/HP Train; WP full GPS; marker soft-clear |
| 2.2.0.3 | 2.2.0.3 | Settings pad; DPS upload drain; heart GPS flow/width; AspectLayout |
| 2.2.0.2 | 2.2.0.2 | D3D SwapChain world GPS; ≤500-line module layout; expanded IPC/ads/application-layer chapters |
| 2.2.0.1 | 2.2.0.1 | Lady Features exclusivity; sticky GPS; UI scale; Nexus Disable unload |
| 2.2.0.0 | 2.2.0.0 | Side-rail chrome; Account/Pathing overhaul; shipping cut of Beta UI |
| earlier | ≤2.1.x | Private CEF 150 kernel; HLI5 IPC; Open Ext attribution |

### Glossary

| Term | Meaning in this report |
|------|------------------------|
| **Nexus** | Raidcore Guild Wars 2 addon loader providing ImGui, WndProc, keybinds, QuickAccess, paths, and SwapChain access |
| **Host DLL** | `GW2-InGame-Helper.dll` loaded into the game process |
| **Helper** | Out-of-process `GW2HelperBrowser.exe` that loads private `libcef.dll` |
| **OSR** | Windowless off-screen rendering; CEF paints BGRA into a CPU buffer |
| **HLI5** | Packed shared-memory IPC magic (`0x484C4935`) between DLL and helper |
| **Open Ext** | Helper→DLL request to `ShellExecute` a URL in the system browser |
| **Present hook** | Detouring `IDXGISwapChain::Present` — **forbidden** by project policy |
| **SwapChain device** | D3D11 device obtained via Nexus `AddonAPI::SwapChain` for host-side drawing |
| **Stamp** | Version string written beside extracted assets (helper `.ver`, homepage, sites, CEF) |
| **World GPS** | In-world trail ribbons drawn with D3D11 upright geometry (not minimap-only) |
| **MumbleLink** | Shared-memory player/camera pose used read-only for overlays (never for automation) |
| **Proton / Wine** | Compatibility layers running the Windows GW2 client on Linux |

---

## Abstract

In-game overlays that surface the live web—wikis, build repositories, and community guides—must simultaneously satisfy three hard constraints. First, contemporary game clients and community norms typically forbid third-party hooks on the presentation path. Second, modern web content requires a current Chromium stack rather than the game’s aged embedded browser. Third, a growing fraction of players execute Windows titles under Proton or Wine, where GPU sharing and process sandboxing diverge from native Windows behaviour.

This report documents the architecture of **GW2 In-Game Helper**, a Raidcore Nexus ImGui addon that embeds **stock CEF 150** as a **separate process**, renders via **windowless off-screen rendering (OSR)** into **PID-scoped shared memory**, and composites frames through the host’s existing Direct3D 11 device without Present hooks. We analyse the inter-process communication (IPC) contract, the adaptive CPU-to-GPU upload pipeline, navigation and advertisement click-through policy, the security posture (including intentional sandbox disablement under Wine), compliance boundaries relative to Guild Wars 2 and the Nexus ecosystem, and performance implications for constrained hardware.

Beyond the browser kernel, revision **2.2.0.11** documents the **application layer**: Account pads on the official ArenaNet API; Pathing packs with Blish/TacO behaviors; **D3D world GPS** ribbons drawn exclusively via the Nexus SwapChain device; DPS Logs via Elite Insights; and Events/Notes. We argue that shared-memory OSR is not an optimal GPU path in the abstract, but that it constitutes a **rational engineering equilibrium** given Nexus API surfaces, coexistence with ArcDPS and ReShade, single-DLL distribution, and Proton viability. Likewise, world GPS uses host SwapChain drawing rather than Present hooks or process-memory camera reads, preserving the same coexistence invariants.

**Keywords:** Chromium Embedded Framework; off-screen rendering; game overlay; shared-memory IPC; Direct3D 11; Wine; Proton; advertisement attribution; process isolation; Guild Wars 2; Raidcore Nexus; TacO pathing; Blish-style trails.

---

## 1. Introduction

### 1.1 Motivation

Players of massively multiplayer online games routinely consult external knowledge bases during play: skill definitions on a wiki, raid rotations on community sites, fractal checklists, and consumable planners. Context-switching to a desktop browser interrupts immersion and, in timed or competitive content, can cost attempts. An *in-client* browser therefore has clear utility—provided it can be distributed without violating platform rules, destabilising the renderer, or excluding Linux users who run the title under compatibility layers.

Guild Wars 2 (ArenaNet) is commonly extended on Windows via community loaders. **Raidcore Nexus** provides a supported ImGui surface, input routing, QuickAccess icons, and a swap-chain pointer, without requiring addons to detour `IDXGISwapChain::Present`. Independently, many players already run **ArcDPS** and/or **ReShade**, which occupy the Present and DXGI hook niche. Any browser overlay that also hooks Present therefore enters a multi-party conflict on the GPU path.

The same ecosystem also hosts **pathing overlays** (TacO, Blish HUD Pathing, Taimi). Players expect in-world trail visualization comparable to those tools. A Nexus addon that draws trails must do so without Present hooks and without reading the game’s private camera matrices from process memory—constraints that shape the D3D world GPS design described in §17.

### 1.2 Problem statement

We require a system that:

1. Displays interactive, contemporary web content inside the game overlay.
2. Uses only Nexus-sanctioned host APIs (no game-memory read/write for cheating; no Present hooks).
3. Isolates Chromium crashes from the game process.
4. Ships as a **single DLL** for end users, with heavy dependencies fetched once.
5. Executes on native Windows **and** Steam Proton / Wine.
6. Preserves publisher advertisement economics where feasible (load ads; do not truncate click trackers).
7. Coexists with ArcDPS and ReShade.
8. Supports application-layer QoL (Account, Pathing/GPS, DPS Logs, Events, Notes) without weakening (1)–(7).

### 1.3 Contributions

This document contributes:

1. A full-system description of an **out-of-process CEF OSR** overlay hosted under Nexus.
2. A precise account of the **HLI5** shared-memory protocol (commands, input, paint, Open Ext), including field-level semantics and sequence protocols.
3. An analysis of **why DXGI shared textures were rejected** despite superior bandwidth properties.
4. A navigation policy for **advertisement click-through** under OSR popup constraints, with explicit non-regression criteria.
5. A security and performance evaluation, including sandbox limitations and hardware bounds.
6. An explicit statement of evaluation criteria and threats to validity for qualitative claims.
7. Documentation of the **application layer** at architecture impact level: Pathing + D3D world GPS, Account API pads, DPS Logs / Elite Insights.
8. A maintainability narrative: modular ≤500-line translation units, intentional micro-level C++ style versus Fortune-500 ideals, and restricted kernel ownership.

### 1.4 Non-goals

We do not claim: combat automation; reading player or entity memory for advantage; bypassing Cloudflare or Google OAuth in embedded contexts; AAA-grade zero-copy GPU compositing; a rebuilt or forked Chromium; Lua `script-*` Blish Pathing feature parity; proprietary video codecs in stock CEF; or peer-reviewed empirical HCI studies.

Feature pads are *application layer* atop the browser kernel. This report describes them where they affect IPC, present, compliance, or player-facing architecture—not as exhaustive product manuals (see [`PATHING.md`](PATHING.md), [`ACCOUNT.md`](ACCOUNT.md), [`DPS_LOGS.md`](DPS_LOGS.md)).

### 1.5 Document organisation

Section 2 situates the work against CEF and overlay strategies. Section 3 states design goals and trade-offs. Sections 4–6 describe process topology, IPC, and rendering. Sections 7–8 cover input and navigation/ads. Sections 9–13 address security, content adaptation, cross-platform behaviour, performance, and reliability. Sections 14–16 cover evaluation, future work, and conclusion for the **kernel**. Section 17 expands the **application layer**. Section 18 discusses maintainability and intentional implementation style. Appendices record constants, source maps, IPC command sketches, smoke checklists, and document control.

---

## 2. Background and related approaches

### 2.1 Chromium Embedded Framework

CEF wraps Chromium for embedding [1], [2]. Two rendering modes are relevant:

- **Windowed.** Chromium owns an OS window. This is a poor fit for an ImGui panel inside a fullscreen or borderless game.
- **Windowless / OSR.** Chromium paints into a CPU (or, in advanced configurations, GPU) buffer via `OnPaint`. The host composites.

Official CEF binaries—commonly distributed via Spotify’s automated builds [2]—ship as versioned archives. GW2 In-Game Helper consumes the **minimal Windows x64** package for Stable 150 and **rehosts** a flattened zip; it does **not** patch Chromium sources. Proprietary codecs are absent from those binaries (§9.5 / [`COMPLIANCE.md`](COMPLIANCE.md)).

### 2.2 Overlay strategies in PC games

| Approach | Advantages | Disadvantages relative to our constraints |
|----------|------------|-------------------------------------------|
| In-process CEF in the game DLL | Simple IPC | Chromium crash implies game crash; D3D device contention; antivirus scrutiny of a large DLL |
| Hook Present and inject an HWND | Familiar to overlay tooling | Conflicts with ArcDPS/ReShade; community and ToS norms |
| Game’s own CEF (`bin64/cef`) | Already on disk | Wrong CEF generation relative to our headers; writing there is forbidden by project policy |
| External desktop browser with capture | Isolation | High latency; fragile capture; not “in panel” |
| **Out-of-process OSR with shared memory** | Isolation; Nexus-compatible | CPU upload cost; IPC complexity |

### 2.3 Pathing overlays in Guild Wars 2

TacO, Blish HUD Pathing, and related tools popularised `.taco` / `.trl` marker packs and in-world trails. They often integrate deeply with game state. A Nexus-hosted helper cannot assume the same privileges. Our Pathing domain loads packs for **display**, implements a subset of Blish marker behaviors, and draws GPS ribbons via **SwapChain-only** D3D11 (§17.2). Lua scripted behaviors remain out of scope.

### 2.4 Wine and Proton as first-class targets

Steam Deck and Linux desktop users run Guild Wars 2 under Proton [6]. Win32 APIs are translated; GPU features (sandboxed Chromium GPU process, DXGI shared handles, some `ShellExecute` paths) are incomplete or unreliable. Designs validated only on native Windows fail silently in this environment. Several Helper policies—software OSR switches, DLL-side URL opens, and process caps—exist primarily for Proton survivability.

### 2.5 Positioning

This work is a **systems engineering report** for a shipping community addon, not a comparative benchmark study. Related commercial and open overlays are acknowledged as design alternatives (§2.2); quantitative head-to-head evaluation is out of scope. Normative compliance rules are maintained separately in [`COMPLIANCE.md`](COMPLIANCE.md); operational module maps live in [`ARCHITECTURE.md`](ARCHITECTURE.md); contributor playbooks in [`KERNEL.md`](KERNEL.md).

---

## 3. Design goals and explicit trade-offs

| Goal | Decision | Trade-off accepted |
|------|----------|--------------------|
| Crash isolation | Separate `GW2HelperBrowser.exe` | Cross-process IPC cost |
| One-file install | Embed helper and HTML in the DLL; download CEF once | First-open bandwidth (~170 MB zip) |
| No Present hook | Nexus `SwapChain` → create host textures; world GPS via same device | No DXGI shared surface with CEF GPU; no game depth buffer |
| Modern web | Stock CEF 150, not game CEF trees | Large runtime; antivirus attention |
| Proton viability | `disable-gpu*`, `no-sandbox`, Open Ext via game process | Weaker sandbox; more CPU paint |
| Multi-client | PID-scoped IPC object names | Slightly more complex naming |
| Partner ad revenue | Allow ad loads; externalise click URLs when detected | Mixed in-tab versus external UX |
| Hot-reload | `AF_None` (unload stops CEF first) | Prefer full GW2 restart after DLL replace |
| World GPS parity | D3D upright ribbons + pack textures | Requires `d3dcompiler`; no ImGui billboard fallback if init fails |
| Maintainability | ≤500-line `.cpp` preference; Shared/Internal splits | Higher file count; more include discipline |

### 3.1 Objective function

Critics correctly note that zero-copy GPU sharing and a full Chromium sandbox would be preferable *in the abstract*. The product’s objective function is different: **ship a single community DLL that (a) coexists with ArcDPS/ReShade, (b) survives Proton, (c) does not write `bin64/cef`, and (d) preserves publisher click economics**. Under that function, conservatism on the GPU and sandbox axes is rational.

---

## 4. System architecture

### 4.1 Process topology

```text
Gw2-64.exe
 └─ Nexus (loader)
     └─ GW2-InGame-Helper.dll          [IPC host, ImGui, D3D present, CEF installer, pads]
          ├─ RT_Render → UI + WikiBrowser::PresentFrame + Pathing world GPS draw
          ├─ WndProc → input routing
          └─ CreateProcess
              └─ GW2HelperBrowser.exe  [CEF browser process]
                   ├─ libcef.dll (private tree under addons/.../cef/)
                   ├─ utility / network / storage children
                   └─ renderer (capped; software OSR path)
```

The game process never loads `libcef.dll`. The helper loads CEF from `addons/GW2-InGame-Helper/cef/` after SHA-256-verified installation [3].

### 4.2 Lifecycle (detailed)

1. **AddonLoad.** Register Nexus render, WndProc, keybind, and QuickAccess handlers. Signature `HELP`. Version stamped in `AddonDef`. Unload allowed (`AF_None`).
2. **Idle.** No CEF process until the player opens the helper (or another path forces launch).
3. **First open.** Ensure addon data directory; `CefRuntime::EnsureInstalled`; extract helper EXE if `kHelperStamp` mismatches; `CreateProcess` with `--cef-dir` and `--host-pid` **off the render thread** (worker + Tick).
4. **IPC attach.** PID-scoped map names via `WikiIpcFormatNames`; helper sets `ready` when accepting commands (must not deadlock waiting for first browser create).
5. **Steady state.** UI posts commands; helper paints; DLL presents; Open Ext sequences drained via `ShellExecute` in the **game** process; pads use MumbleLink / API independently of CEF when possible.
6. **Unload / quit.** Post `QUIT`; grace period; Job Object `KILL_ON_JOB_CLOSE` reaps children; helper watches host handle and exits if Guild Wars 2 terminates.

### 4.3 Job Object and host watch

Orphan Chromium trees after a hard game kill are a historical failure mode under Proton. The host joins the helper to a Job Object with kill-on-close. Independently, the helper opens the host PID with `SYNCHRONIZE` and exits when the host dies. Both mechanisms are necessary; neither alone covers all crash orderings.

### 4.4 On-disk layout (deployment)

```text
<GW2>/addons/GW2-InGame-Helper.dll          # sole required install artefact
<GW2>/addons/GW2-InGame-Helper/
  GW2HelperBrowser.exe (+ .ver stamp)
  settings.ini, sites.json (+ .ver), profiles/notes/…
  pages/       generated HTML + home assets (helper-home, live-*.html, …)
  live/cache/  live-*.json API caches
  cache/       unlocks / waypoints / stash caches
  cmds/        helper ↔ DLL *-cmd.txt
  cef/         libcef.dll, pak/dat, locales/, cef.ver
  cheatsheets/
  pathing/     curated + user .taco packs
  ei/          optional Elite Insights CLI
%LOCALAPPDATA%/GW2-InGame-Helper/cef-cache/ # Chromium profile/cache (not under addons/)
```

**Rationale for LocalAppData cache:** Guild Wars 2 addon folders are often synced, backed up, or wiped by users; Chromium profiles under `addons/` also increase AV surface. The profile must never land in `bin64/cef`.

### 4.5 Build and distribution pipeline

- **Toolchain.** `x86_64-w64-mingw32-g++` (C++17), static `libgcc` / `libstdc++` for shipping PE.
- **Helper.** Compiled against `deps/cef` 150 C API headers; embedded as a binary blob in the DLL.
- **CEF archive.** `scripts/pack-cef-runtime.sh` flattens official minimal packages; SHA-256 in `CefRuntime.h`.
- **Catalog.** `data/sites.json` embedded and extracted at runtime.
- **Updates.** Nexus `UP_GitHub` for DLL; CEF stamped separately (`cef.ver` = `150.0.14`).

Operational detail: [`ARCHITECTURE.md`](ARCHITECTURE.md), [`BUILD.md`](BUILD.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md).

### 4.6 Multi-client

Two Guild Wars 2 processes must not share IPC maps. Object names include the host PID:

```text
Local\GW2InGameHelper_CEF_IPC_v5_<pid>
Local\GW2InGameHelper_CEF_FRAME_v5_<pid>
Local\GW2InGameHelper_CEF_WAKE_v5_<pid>
```

RAM cost is multiplicative (each client may spawn its own helper + CEF tree).

---

## 5. Inter-process communication

### 5.1 Rationale for shared memory

Named pipes or schema-oriented RPC would ease evolution and tooling. They introduce additional copies, scheduling latency, and Wine edge cases on a 60 Hz paint path. The Helper therefore uses:

1. One **control** mapping: packed `WikiIpcState` (`#pragma pack(1)`, magic `HLI5`).
2. One **frame** mapping: double-buffered BGRA with capacity  
   \(1920 \times 1200 \times 4 \times 2 \approx 18.4\,\mathrm{MiB}\).
3. One **wake** event for low-latency helper wakeup (avoiding busy-spin under Proton).

### 5.2 Command plane

Commands travel on a ring of size **32** (`WikiCmdEvent`: `cmd`, `a`, `arg[1536]`).

| Command | Role |
|---------|------|
| `NAVIGATE` | Load URL / `about:` resolve |
| `BACK` / `FORWARD` / `RELOAD` / `HOME` | History and home |
| `QUIT` | Graceful helper shutdown |
| `SET_BOUNDS` / `SET_VISIBLE` | View size and visibility (coalesce under load) |
| `CREATE_TAB` / `ACTIVATE_TAB` / `CLOSE_TAB` | Up to 8 OSR browsers |
| `FIND` / `STOP_FIND` | Find-in-page |

Overflow drops oldest carefully; CREATE/NAVIGATE stomps historically motivated the ring design over a single slot.

### 5.3 Input plane

- **Discrete ring (256):** clicks, wheel, keys, focus (`WikiInputEvent`). Drop oldest on overflow.
- **Coalesced mouse:** `mouse_x/y/mods/seq` updated continuously; wake throttled so Proton is not busy-spun.

Key synthesis uses `ToUnicode` when Nexus skips `TranslateMessage` (fast typing).

### 5.4 Paint plane

| Field | Semantics |
|-------|-----------|
| `frame_w/h` | Active content dimensions |
| `frame_seq` | Monotonic publish counter |
| `frame_front` | Index (0/1) DLL should read |
| `frame_reading` | Pin while DLL copies; `0xFFFFFFFF` when idle; helper must not overwrite pinned buffer |
| `dirty_*` | Union dirty rectangle in frame pixels |

There is **no long-held mutex** on the frame buffer. Correctness relies on the pin protocol and sequence numbers.

### 5.5 URL, title, status

UTF-8 buffers use **odd/even sequence** protocols (`url_seq`, `title_seq`): odd means writer in progress; even means stable. Readers retry on odd.

### 5.6 Open Ext and Open Tab

```text
Helper detects externalisable navigation
        │
        ▼
Write open_ext_url[≤8192] + bump open_ext_seq
        │
        ▼
DLL Tick drains → ShellExecuteA in game process
```

- **Refuse** if URL still exceeds 8192 after encoding—never truncate.
- Truncation previously produced blank advertiser landings and zero billable clicks.
- `open_tab_*` opens an in-addon tab instead of the system browser.

`ShellExecute` from the CEF helper frequently no-ops under Proton; game-process handoff is a **correctness** feature, not a convenience.

### 5.7 ABI and version skew

Both sides are MinGW-built Win32 PE with `#pragma pack(1)`. Padding divergence is unlikely. **Layout skew is likely** if DLL and helper ship independently—hence magic `HLI5` and `kHelperStamp`. Host CI includes `make test-ipc` (magic + sizeof + queue constants).

### 5.8 Limits of the IPC style

- Schema changes require coordinated DLL + helper deployment.
- Cross-process races are invisible to ThreadSanitizer.
- The channel is intentionally narrow—not a general RPC framework.

These are accepted costs of a local, latency-sensitive paint channel. Operational playbooks: [`KERNEL.md`](KERNEL.md).


### 5.9 Field catalogue (`WikiIpcState`)

The following catalogue mirrors [`WikiIpc.h`](../src/browser/WikiIpc.h) for documentation purposes. The header remains authoritative.

| Field group | Fields | Notes |
|-------------|--------|-------|
| Identity | `magic`, `ready`, `alive` | Magic must be `HLI5`; `ready` gates command acceptance |
| Navigation chrome | `can_back`, `can_forward`, `visible` | Mirrored to ImGui buttons |
| View | `view_w`, `view_h` | Requested OSR view size |
| Frame | `frame_w/h`, `frame_seq`, `frame_front`, `frame_reading`, `dirty_*` | Paint publish + pin |
| Legacy cmd slot | `cmd`, `cmd_seq`, `last_cmd_seq`, `cmd_arg`, `cmd_a` | Fallback / debug; prefer ring |
| Tabs | `active_tab`, `tab_mask` | Up to 8 slots |
| Find | `find_count`, `find_ordinal` | Find-in-page status |
| Document | `url*`, `title*`, `status` | Seq-guarded UTF-8 |
| Mouse live | `mouse_x/y`, `mouse_mods`, `mouse_leave`, `mouse_seq` | Coalesced |
| Rings | `input_*`, `cmd_*` | Discrete queues |
| Open Ext / Tab | `open_ext_*`, `open_tab_*` | Helper → DLL requests |

### 5.10 Wake event semantics

The wake event exists so the helper can sleep between CEF message-loop iterations without polling at full rate under Proton. The DLL signals wake on input, commands, and visibility changes. Missing wake signals manifest as “sticky” UI (clicks queued but not drained promptly).



---

## 6. Rendering pipeline (CEF present)

### 6.1 OSR paint (helper)

CEF invokes `OnPaint` with BGRA. The helper copies into the back buffer, publishes dirty rectangles, advances `frame_seq` / `frame_front`, and signals wake. Inactive tabs do not drive the shared paint surface; up to **eight** browsers exist (`kWikiMaxTabs`), but only the active slot presents.

`windowless_frame_rate` = **60**. Software GPU flags avoid contention with the game’s D3D device under Wine:

- `disable-gpu`, `disable-gpu-compositing`, `disable-gpu-vsync`
- `disable-d3d11`, `disable-direct-composition`
- `in-process-gpu`
- renderer process limit = 1 (Proton process fan-out can stall the host)

### 6.2 Present (DLL)

`WikiBrowser::PresentFrame` on the Nexus render tick:

1. Read front-buffer metadata; pin via `frame_reading`.
2. Adaptive cadence: ≈**120 Hz** recent wheel, ≈**60 Hz** interacting, ≈**30 Hz** idle.
3. Map **STAGING** (`DO_NOT_WAIT` after first paint; blocking `Map` allowed on first paint to avoid Windows “Waiting for first paint…”).
4. Copy dirty rows (or chunked full frames) into staging.
5. `CopySubresourceRegion` into **DEFAULT** shader-resource texture (max \(1920\times1200\)); ImGui samples with UV crop (`FrameUvMax`).

**Invariant:** never `Map` the ImGui-bound DEFAULT texture.

### 6.3 Rejection of DXGI shared surfaces

A zero-copy path would reduce PCIe and CPU traffic [5]. Rejected because:

1. Nexus exposes a swap chain for **host** drawing, not a contract for importing foreign CEF GPU surfaces.
2. CEF OSR’s portable contract is a CPU bitmap; GPU OSR sharing is fragile across CEF versions.
3. Proton/Wine shared-handle support is incomplete.
4. ArcDPS and ReShade already stress DXGI; an additional importer raises failure modes.

CPU upload is therefore **constraint-driven**. Mitigations (idle throttle, dirty rects, chunking, fixed texture) address hitches without claiming the path is free.

### 6.4 Bandwidth sketch

Full \(1920\times1200\) BGRA ≈ 8.8 MiB. At 60 Hz full frames ≈ 528 MiB/s before dirty-rectangle savings. Idle 30 Hz + dirty rects keep averages far lower; interactive scrolling on dual-core CPUs concurrent with GW2 simulation is the stress case.

### 6.5 Relationship to world GPS drawing

CEF present and Pathing world GPS both use the Nexus SwapChain **device**, but they are separate pipelines: CEF uploads OSR bitmaps to ImGui textures; world GPS builds upright ribbon meshes in world space. Neither hooks Present. See §17.2.



### 6.6 Present cadence pseudocode

```text
on Nexus RT_Render:
  if helper not ready: return
  decide budget Hz from (wheel_recent ? 120 : interacting ? 60 : 30)
  if too soon since last upload: return
  pin frame_reading = frame_front
  map STAGING (block if first_paint else DO_NOT_WAIT)
  copy dirty rows (or chunked full frame)
  unmap; CopySubresourceRegion → DEFAULT
  frame_reading = 0xFFFFFFFF
  ImGui::Image(SRV, uv0..FrameUvMax)
```

Wheel-recent detection prevents under-sampling during fast scroll, which otherwise produces smearing and perceived input lag.

### 6.7 Texture sizing policy

Allocating a DEFAULT texture that resizes every panel drag historically caused hitching and resource churn. The shipping policy sizes once to the IPC maximum and crops with UVs. Unused margins cost VRAM but buy stability.



---

## 7. Input, focus, and user-interface chrome

The ImGui layer owns window chrome: side rail (Browse / Account / Pathing / Events / DPS Logs / Notes / Compass), tabs, find-in-page, Open Ext, and settings [4]. Mouse and keyboard destined for the page are serialised into IPC.

Design pressures:

- Nexus WndProc ordering versus ImGui capture (historical drag/resize regressions).
- Guild Wars 2 chat: Space and related keys must not be swallowed when the overlay is open but chat is focused.
- Fast typing: `ToUnicode` synthesis when Nexus skips `TranslateMessage`.
- Pad combo popups historically lost clicks under Nexus; prefer in-window chips/radios.
- Tab find uses CEF find handlers mirrored into IPC status fields.

Bundled `file://` pages (home, cheat sheets, raid food, API key help) are first-class; version stamps force re-extract when HTML changes.

Per-panel UI scale (`UiScale`) adjusts pad fonts from Options × window size without mutating Nexus `FontGlobalScale` globally.

---

## 8. Navigation policy and advertisement economics

Operational checklist for maintainers: [`NAV_AND_ADS.md`](NAV_AND_ADS.md). This section states the analytical policy.

### 8.1 OSR popup constraint

OSR cannot host real popup windows. `OnBeforePopup` always cancels native popups. Without a replacement policy, `target=_blank` advertisement clicks become silent no-ops—catastrophic for publisher CPC economics [7].

### 8.2 Externalisation decision tree

Route to the system browser (via Open Ext) when user-gesture navigations match any of:

1. Known **click-tracker** URL shapes (`pagead/aclk`, DoubleClick `/pcs/click`, …).
2. Landing URLs carrying **network click identifiers** (`gclid`, `gad_source`, `msclkid`, …).
3. Main-frame navigations onto **ad-network hosts**.
4. Navigations whose **referrer** is an ad frame (SafeFrame / DoubleClick), even if the destination is a plain advertiser domain.
5. Subframe-originated promotable URLs from known ad iframes.
6. Cross-site `target=_blank` popups (same-site and bundled `file://` may remain in-tab).

YouTube and `discord://` deep links also prefer external handling where OSR cannot complete the flow.

```text
OnBeforeBrowse / OnBeforePopup
        │
        ├─ no user gesture ───────────────────────► usually deny popup / ignore
        ├─ tracker / ad-host / click-id / ad-ref ─► OpenExternalUrl (full URL)
        ├─ cross-site _blank ─────────────────────► OpenExternalUrl
        ├─ same-site / file:// new window ────────► may navigate in-tab
        └─ ordinary same-tab nav ─────────────────► allow in CEF
```

### 8.3 Screen versus viewport geometry

`GetViewRect` reports the ImGui panel. `GetScreenInfo` reports the primary monitor and work area with `device_scale_factor` = **1.0** (IPC mouse and paint are view pixels). Matching `window.screen` to the tiny panel is an easy non-human signal for impression filters. Separating them improves fingerprint hygiene; it does **not** claim desktop-Chrome viewability.

### 8.4 Attribution honesty

**Billable publisher clicks** generally require the ad network’s tracker URL to be *requested*. Opening a final landing that already contains `gclid` is necessary but not always sufficient if `aclk` never ran. Empirical `navlog` traces on Proton showed both gold-standard `aclk` handoffs and landing-only paths. Mixed in-tab versus external UX is acceptable when the tracker (or a completed redirect chain) ran; **truncated URLs are not**.

Optional `navlog.on` enables decision tracing; it must **not** ship enabled.

### 8.5 Non-regression criteria (ads)

A change **fails** ads policy review if it:

1. Truncates Open Ext URLs.
2. Blocks ad/consent/analytics hosts by default.
3. Sets `pointer-events: none` (or equivalent) on ad iframes such that clicks become no-ops.
4. Re-enables native OSR popups without an externalisation path.
5. Moves `ShellExecute` exclusively into the helper process without a DLL fallback under Proton.
6. Shrinks `open_ext_url` below 8192 without a proven replacement.

### 8.6 BootJs interaction

BootJs mounts Open Ext tips for Google login, some site logins, and Cloudflare challenges. Advertisement clicks must remain clickable; historical `pointer-events: none` overlays killed publisher CPC and are explicitly forbidden to reintroduce casually ([`BootJs.h`](../src/helper/BootJs.h) commentary).

---

## 9. Security and trust model

### 9.1 Process boundary

Page JavaScript executes in Chromium children of the **helper**, not inside the game module. A renderer compromise is closer to “code as the helper user” than “arbitrary code in `Gw2-64.exe`”—though still severe.

### 9.2 Threat model (summary)

| Threat | Mitigation | Residual risk |
|--------|------------|---------------|
| Malicious web page | Process isolation; no game memory API from JS | Helper-user-equivalent code execution |
| Compromised CEF zip | SHA-256 verify before extract | If URL+hash both attacker-controlled, game over |
| DLL malware masquerade | GitHub releases; user vigilance | Unsigned MinGW PE |
| Cross-process IPC spoof | Local namespace + PID scope | Same-user local attackers |
| Credential theft via overlay | API key only in local settings; Open Ext for OAuth | Phishing pages still possible in-CEF |

### 9.3 Sandbox

Shipping sets `no_sandbox` / `--no-sandbox` because:

1. CEF 150+ Windows sandbox expects bootstrap/module packaging not used in our single-EXE helper layout.
2. Wine/Proton sandboxing is unreliable.
3. Many embedded CEF apps make the same trade.

This is a **conscious downgrade** from desktop Chrome, not an accidental flag for ads. Site-isolation experiments for OSR iframe input were considered and **not** retained as policy.

### 9.4 Supply chain

- CEF zip SHA-256 verified before extract.
- Runtime kept out of the DLL blob (reduces Defender ML packing heuristics).
- Unsigned MinGW builds may still be quarantined; code signing remains future work.
- Elite Insights CLI similarly SHA-verified when downloaded into `ei/`.

### 9.5 Video codecs

Stock CEF builds lack proprietary codecs; H.264/AAC unavailable. Twitch `Error #4000` and similar failures are expected. YouTube/Twitch route to Open Ext Watch cards. Enabling codecs requires building Chromium with licensing—product/legal, not a casual flag.

### 9.6 Compliance (game ecosystem)

**Allowed.** Nexus APIs; private CEF under addon dir; local IPC; official API reads with backoff; MumbleLink **read-only** for overlays; SwapChain D3D world GPS; terminate only our helper PID; load site ads/consent; curated pathing packs; optional EI CLI.

**Forbidden.** Game memory R/W for cheating; MinHook Present / `d3d11` wrappers; synthetic input into GW2; combat/account automation; writing `bin64/cef`.

Authoritative normative text: [`COMPLIANCE.md`](COMPLIANCE.md).

---

## 10. Content adaptation layer

Because OSR is not desktop Chrome:

- **BootJs** injects tips (Open Ext for Google/Cloudflare), armory chip fills via official API, embed reshaping, `<select>` polyfill (native PET_POPUP crashed helpers on Windows).
- **CSS compatibility** rewrites problematic constructs (e.g. Material `color-mix`) via response filters where needed.
- **Guildjen / YouTube:** iframes promoted to Watch cards / Open Ext rather than unreliable in-OSR playback under software CEF.

Advertisement, consent, and analytics hosts are intentionally **not** stripped—partner sites depend on them.

User-Agent includes product token `GW2-InGame-Helper` so publishers can allow/deny ([`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md)).

---

## 11. Cross-platform behaviour

| Concern | Native Windows | Proton / Wine |
|---------|----------------|---------------|
| ISA / PE | Native | Translated |
| CEF GPU | Software flags retained (conservative) | Required for stability |
| Open Ext | `ShellExecute` usually OK | **Must** go through game process |
| First paint | Historical Map/`DO_NOT_WAIT` stall addressed | File locks; process caps |
| `d3dcompiler` for world GPS | Typically present | May be missing → GPS init fails (no ImGui trail fallback) |
| Antivirus | Defender ML on MinGW | Less relevant |
| Extract helper | Straightforward | Wine may lock old EXE; stamp/delete on install |

The architecture is Windows-first; Linux success is evidence of Proton resilience, not a Linux-native port.

---

## 12. Performance and lower-tier hardware

### 12.1 Cost centres

1. Chromium helper RSS (hundreds of MB with heavy pages).
2. Shared frame mappings (~18 MiB + control).
3. CPU `memcpy` + staging upload while scrolling.
4. CEF software raster under current flags.
5. World GPS ribbon rebuild/resample when many trails are enabled (usually modest vs CEF).

### 12.2 Expected experience

- Light wiki use, overlay closed in combat: acceptable on hardware that already runs GW2+Nexus.
- Large overlay + rapid scroll + ad-heavy pages: micro-stutters on weak CPUs.
- In-overlay video: best-effort; Open Ext is reliable.
- Dual clients: multiplicative RAM.
- Dense Pathing categories + world GPS: extra draw cost; still SwapChain-only.

Mitigations (adaptive frame rate, dirty rects, chunked upload, warm-hide coalescing, GPS sticky cache/hysteresis) improve the curve but cannot make Chromium free.

---

## 13. Reliability engineering

Observed and addressed failure classes (non-exhaustive):

| Class | Mitigation |
|-------|------------|
| Helper/DLL skew | Stamp + magic |
| IPC ring overflow | Drop oldest input; coalesce bounds |
| Proton busy-wait | Event wake + idle timeouts |
| Orphan helpers | Job Object + host wait |
| Windows first-paint stall | Blocking first Map; staging vs DEFAULT |
| Ad URL truncation | 8 KiB Open Ext; refuse if longer |
| Launch storms | Crash-loop brakes; process caps |
| GPS blink / incomplete trails | Along-path sampling; sticky cache; hysteresis |
| Wrong HLSL mul order | Explicit `mul(M,v)` world transform discipline |

Memory-safety tooling cannot fully validate this stack: the DLL loads into a non-instrumented game; CEF is prebuilt; races are cross-process. Helper-only ASan and stress tests remain the realistic assurance path.

---

## 14. Evaluation

### 14.1 Criteria and qualitative assessment

| Criterion | Assessment |
|-----------|------------|
| Functional web browsing in-overlay | Met for primary content sites |
| Crash isolation | Met (helper death need not imply game death) |
| Nexus / ArcDPS / ReShade coexistence | Met by construction (no Present hook) |
| Proton viability | Met with software CEF and Open Ext IPC |
| Publisher advertisement clicks | Partially met; tracker-intact paths strong; landing-only paths uncertain |
| Zero-copy GPU | Not met (explicitly deferred) |
| Sandbox parity with Chrome | Not met (documented) |
| Low-end hitch-free interaction | Not guaranteed under heavy overlay use |
| World GPS Blish-like readability | Met for upright ribbons + pack chevrons when D3D init succeeds |
| Pathing Lua parity | Not met (documented non-goal) |

### 14.2 Threats to validity

1. **Qualitative evaluation.** Maintainer observation and smoke tests—not controlled user studies or automated visual regression.
2. **Hardware heterogeneity.** Bandwidth sketches are analytical upper bounds.
3. **Proton variance.** Steam/Wine versions change behaviour.
4. **Advertisement attribution.** Network-side billability cannot be verified from the client alone.
5. **Scope creep.** Application-layer pads evolve independently; kernel claims are not coverage of every pad feature.
6. **Documentation lag.** This report tracks revision 2.2.0.11; future commits may land before the next sync.

---

## 15. Future work

1. Windows sandbox path using CEF bootstrap/`--module`, retaining `no-sandbox` only under Wine detection.
2. Optional DXGI shared-texture experiment behind a native-Windows flag (never default on Proton).
3. Code signing to reduce Defender false positives.
4. Helper-only ASan Makefile target for continuous smoke testing.
5. Stricter advertisement attribution: prefer completing network click navigations before landing-only Open Ext.
6. Explicit IPC version negotiation beyond magic.
7. Continue CI discipline (`make ci`) on public pushes/PRs.
8. Further helper boot/IPC decomposition.
9. Optional soft degradation when `d3dcompiler` is missing (today: no ImGui billboard fallback by design).
10. Consolidate duplicated hand-rolled JSON helpers behind one internal utility (Account/Pathing/Logs)—without introducing helper/nav risk.

### 15.1 Maintainability trajectory

As of revision 2.2.0.11 the Browse catalog is data-driven (`data/sites.json`), and former monolithic translation units are split into focused units (prefer ≤500 lines per `.cpp`). This reduces merge-conflict surface for feature work but does **not** remove restricted ownership of the CEF, IPC, and present path. See [`MODULES.md`](MODULES.md), [`ARCHITECTURE.md`](ARCHITECTURE.md) §7, [`CONTRIBUTING.md`](../CONTRIBUTING.md).

---

## 16. Conclusion (kernel)

GW2 In-Game Helper demonstrates that a **contemporary Chromium** can be productised inside a **live MMO client** without Present hooks or game-memory intrusion by combining: Nexus as the host UI substrate; **out-of-process CEF OSR**; **PID-scoped shared memory**; **adaptive Direct3D 11 staging uploads**; and **Proton-aware** process and `ShellExecute` policies. Relative to AAA GPU-sharing designs, the architecture is deliberately conservative—and that conservatism is why it fits the Guild Wars 2 addon ecosystem.

IPC rigidity, CPU upload cost, and sandbox gaps are correctly noted by critics. Those properties are **priced-in trade-offs**, not unrecognised accidents. The appropriate engineering question is not whether a zero-copy sandboxed compositor would be preferable in the abstract—it would—but whether it can be shipped as a single community DLL beside ArcDPS on Windows and Steam Deck without breaking the game. Under that objective function, the present design is coherent.

---

## 17. Application layer (architecture impact)

### 17.1 Account hub

Account pads consume the **official** `api.guildwars2.com` surface via WinHTTP on worker threads (`Gw2Http`). An optional API key in `settings.ini` unlocks personal scopes (wallet, inventories, characters, progression, unlocks, tradingpost). Public endpoints (item lookup, some TP prices) work without a key.

Design constraints:

- No account-action automation (purchases, mail sends, etc.).
- 429 backoff and caching to respect API etiquette.
- UI uses wrapping chips / in-window radios (Nexus-safe).

See [`ACCOUNT.md`](ACCOUNT.md), [`API_KEY.md`](API_KEY.md).

### 17.2 Pathing and D3D world GPS

**Packs.** Curated Tekkit All-In-One, Lady Elyssa Guides/Achievements, and Hero's Marker Pack download into `pathing/` with `.ver` stamps; user `.taco` files are retained. Marker behaviors cover TacO/Blish 0–7 and 101, AutoTrigger, hide/show, tips, info, copy—not Lua `script-*`.

**Lady Features.** Barefoot / With Mounts / WP Only are mutually exclusive map-completion editions.

**Compass vs world GPS.** Direction compass is an ImGui/Nexus-font world N/E/S/W pad. World GPS draws **upright D3D ribbons** (Blish-style chevrons, fixed UV period, animspeed flow, soft player clear). Markers remain ImGui.

**Compliance-critical properties of world GPS:**

- Device from Nexus **SwapChain only**.
- **No** Present / `d3d11.dll` hooks.
- **No** game depth buffer or camera-matrix reads from process memory; pose from MumbleLink/DataLink read-only.
- Width: base half-width \(20''\) (`kBlishHalfM`) × soft-clamped pack `trailScale` × user GPS width (1.0× = authored scale).
- If D3D/HLSL init fails (e.g. missing `d3dcompiler` under some Wine setups), world trails do **not** fall back to ImGui billboards (avoids a second, divergent visual path).

Module map: `WorldOverlay` orchestrates `WorldGpsMath`, `WorldGpsD3dDevice` / `WorldGpsD3dDraw`, `WorldGpsImgui`. Details: [`PATHING.md`](PATHING.md), [`pathing/README.md`](../pathing/README.md).

### 17.3 DPS Logs

ArcDPS EVTC browsing via optional Elite Insights CLI under `ei/` (SHA-verified download; requires user .NET 8). Upload to dps.report; KillProof.me public profiles. Parse fixtures gated in `make test-parse`. See [`DPS_LOGS.md`](DPS_LOGS.md).

### 17.4 Events and Notes

World Events schedule pad; Notes + waypoint snippets with local `notes.json`. No game injection.

### 17.5 Browse catalog

Schema-v2 `data/sites.json` (~2718 entries) embeds and extracts to runtime `sites.json`. Live digests use official/news/wiki APIs. Browse rows are labeled hyperlinks—not private partner APIs.

---

## 18. Maintainability and intentional micro-style

### 18.1 Modularization

Prefer ≤500 lines per `.cpp`. Domains use public headers + `*Shared.h` / `*Internal.h` with **one** defining TU for Shared globals. Generated/blob headers (`BootJs.h`, icon embeds) are exempt.

### 18.2 Restricted kernel

CEF launch, IPC, present, WndProc, and nav/ads require paired review ([`KERNEL.md`](KERNEL.md), [`CONTRIBUTING.md`](../CONTRIBUTING.md)). Pad work must not casually edit these files.

### 18.3 Corporate micro-critique versus product reality

A Fortune-500 review would criticise globals, hand-rolled JSON parsers, C-string buffers, and `GetTickCount` polling. Those critiques are stylistically fair and **orthogonal** to the macro-architecture grade. For this product:

- Globals in helper/browser match single-instance process lifetimes.
- Hand-rolled JSON avoids a large dependency in a hostile injected-DLL environment; duplication across Account/Pathing/Logs is technical debt, not a kernel risk.
- Tick polling is simple and adequate for UI timeouts and GPS flow animation.
- Rewriting the helper for DI/`std::format`/mutex-heavy concurrency risks **ads and Proton regressions** with little player-visible gain.

Documented stance: improve Account-side JSON consolidation carefully if desired; **freeze** helper nav/ads unless fixing a proven bug.

---



## Appendix G — Present-path state machine (narrative)

```text
[idle] --open helper--> [ensure CEF] --ok--> [extract helper if stamp mismatch]
        --> [CreateProcess worker] --> [Tick: wait ready]
        --> [steady: cmd/input/paint/OpenExt]
        --> [QUIT / unload / host death] --> [job kill / helper exit]
```

First-paint special case: if the DEFAULT/staging path uses non-blocking Map exclusively from frame zero, Windows builds historically stuck on “Ready but Waiting for first paint…”. The shipping path permits a blocking Map on the first successful paint, then prefers `DO_NOT_WAIT`.

Warm-hide: collapsing the UI may keep the helper alive (`KeepHelperWarm`) to avoid cold-start cost; visibility commands coalesce so Proton does not see launch storms.

## Appendix H — Advertisement economics worked example

1. User clicks a display ad in an OSR iframe.
2. Network wants a navigation to `https://adclick.g.doubleclick.net/pcs/click?...` (often >2 KiB).
3. OSR cannot show a real popup; `OnBeforePopup` cancels.
4. Policy classifies the URL as tracker → helper writes full URL into `open_ext_url`.
5. If `strlen > 8191`, helper **refuses** and surfaces status — it does not silently chop query parameters.
6. DLL `ShellExecuteA` opens the system browser; the tracker hop can complete; publisher CPC has a chance to count.

Failure modes that look like “ads broken” but are policy/environment:

- Proton helper-side ShellExecute with no DLL drain.
- CSS/JS overlay with `pointer-events: none` over the creative.
- Blocking `doubleclick` / `googlesyndication` hosts in a “cleaner” filter.
- Opening only the final advertiser landing without the `aclk` request.

## Appendix I — World GPS geometry notes

Upright ribbons are camera-facing strips in world space, not ground-projected decals. UV period derives from Blish-like half-width so chevron textures tile consistently. Flow animation uses time (`GetTickCount64`) scaled by pack `animspeed` with a sign chosen so chevrons travel **along** the route forward direction.

Sampling expands along the polyline by path meters from the nearest vertex (not only Euclidean ball), which matters for sparse WP Only editions. Sticky merge + label hysteresis reduce blink when the player hovers at range edges.

Transform bug class: HLSL `mul(v,M)` versus `mul(M,v)` can yield “successful” draws that never appear on screen while also suppressing ImGui fallbacks—treat matrix convention as a high-severity GPS regression class.

## Appendix J — Related work notes (extended)

In-process overlays and Present-hook injectors are mature in the broader PC game ecosystem but poorly aligned with Nexus’s coexistence goals. External browser capture (OBS-style) provides isolation at the cost of latency and UX. Game-shipped CEF trees lag Chromium Stable and are policy-forbidden as write targets. Blish HUD Pathing remains the reference for Lua-scripted marker intelligence; this addon intentionally implements a **display-first** subset.

This report does not benchmark against Blish or TacO frame times; claims of “Blish-style” refer to visual idiom (upright textured ribbons, pack scale), not identity of implementation.




## 19. Methods note (how this report was produced)

This report is a **design reconstruction** from the shipping codebase and maintainer operational knowledge at revision 2.2.0.11. It is not the output of a formal measurement campaign. Where quantitative figures appear (frame bytes, ring sizes, bandwidth upper bounds), they are derived from constants in [`WikiIpc.h`](../src/browser/WikiIpc.h) and elementary arithmetic unless otherwise stated.

Claims about Proton behaviour are based on repeated smoke testing across Steam Proton / Wine configurations used by the maintainer and early users; they are **not** guaranteed for every Proton experimental build. Claims about advertisement billability are mechanistic (URL must be requested) rather than accounting audits against publisher invoices.

## 20. End-to-end scenarios

### 20.1 Cold start on a new machine

1. Player copies `GW2-InGame-Helper.dll` into `addons/`.
2. Nexus loads the DLL; signature `HELP` registers UI and keybinds.
3. Player opens the helper; DLL creates data folder; downloads ~170 MB CEF zip if needed; verifies SHA-256; extracts; stamps `cef.ver`.
4. Helper EXE extracts if stamp mismatches; process launches with `--cef-dir` / `--host-pid`.
5. Homepage `file://` paints through OSR → shared memory → staging → ImGui.
6. Later opens skip download when the tree and stamp match.

### 20.2 Advertisement click under Proton

1. Page loads partner content; ad iframe paints in OSR.
2. User clicks; Chromium attempts popup or main-frame navigation to a tracker URL.
3. Policy cancels native popup; classifies tracker; writes full URL to Open Ext.
4. Helper-side ShellExecute would be unreliable; DLL performs ShellExecute in `Gw2-64.exe`.
5. System browser completes the hop; in-overlay tab does not need to display the advertiser page.

### 20.3 World GPS on a Lady Barefoot route

1. Pathing enables Lady Barefoot; categories load `.trl` geometry.
2. `WorldOverlay` samples nearby along-path segments; builds upright ribbon vertices.
3. `WorldGpsD3dDraw` binds pack chevron SRV, updates UV scroll, soft-clears near the player.
4. Markers draw in ImGui; compass pad remains independent.
5. If `D3DCompile` fails, ribbons are absent—no silent ImGui billboard substitute.

## 21. Comparison matrix (constraint fit)

| Requirement | In-process CEF | Present-hook overlay | Game `bin64/cef` | **This design** |
|-------------|----------------|----------------------|------------------|-----------------|
| Crash isolation | Poor | Varies | Poor | **Good** |
| ArcDPS coexistence | Risky | Conflict-prone | Risky | **Good** |
| Modern Chromium | Possible | N/A | Stale | **Good** |
| Single-DLL UX | Hard (huge DLL) | Possible | Already on disk | **Good** (CEF fetched once) |
| Proton | Hard | Hard | Unknown | **Workable** |
| Zero-copy GPU | Possible | Possible | N/A | **Deferred** |
| Full Chrome sandbox | Possible | N/A | Unknown | **Deferred** |

## 22. Glossary expansions (selected)

**DataLink / MumbleLink.** Shared-memory identity published for overlays. Used here strictly for display (compass, GPS fade, map id). Using it to drive synthetic input or combat automation is forbidden.

**KeepHelperWarm.** Setting that may keep the CEF helper alive while the ImGui panel is hidden, trading RAM for faster reopen. Visibility commands must still coalesce under Proton.

**Sticky GPS cache.** Retains recently visible trail snippets across frames with hysteresis so ribbons do not blink when the player straddles range thresholds.

**Refuse-not-truncate.** Open Ext policy when URLs exceed 8192 bytes: fail visibly rather than open a corrupted tracker URL.


## References

1. Chromium Embedded Framework. Project site. Available: https://bitbucket.org/chromiumembedded/cef  
2. CEF Automated Builds (Spotify CDN). Available: https://cef-builds.spotifycdn.com/  
3. Raidcore Nexus. Guild Wars 2 addon loader. Available: https://raidcore.gg/gw2/nexus  
4. O. Cornut, *Dear ImGui*. Available: https://github.com/ocornut/imgui  
5. Microsoft Corporation. DXGI / Direct3D 11 documentation (shared resources; staging textures). Available: https://learn.microsoft.com/windows/win32/direct3ddxgi/  
6. Wine Project; Valve Corporation, *Proton*. Compatibility layers for Windows applications on Linux.  
7. Google Ads Help. Click tracking and `gclid` attribution (industry practice).  
8. ArenaNet. Guild Wars 2 API. Available: https://wiki.guildwars2.com/wiki/API:Main  
9. baaron4 et al. Elite Insights. Available: upstream GW2EI releases (MIT).  
10. GW2 In-Game Helper companions: [`ARCHITECTURE.md`](ARCHITECTURE.md), [`COMPLIANCE.md`](COMPLIANCE.md), [`NAV_AND_ADS.md`](NAV_AND_ADS.md), [`PATHING.md`](PATHING.md), [`ACCOUNT.md`](ACCOUNT.md), [`MODULES.md`](MODULES.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`BUILD.md`](BUILD.md), [`RELEASE_NOTES.md`](RELEASE_NOTES.md).

---

## Appendix A — Quantitative constants (revision 2.2.0.11)

| Constant | Value |
|----------|-------|
| Addon version | 2.2.0.11 |
| Nexus signature | `HELP` / `0x48454C50` |
| IPC magic | `HLI5` / `0x484C4935` |
| Maximum frame | \(1920 \times 1200\) BGRA |
| Frame buffers | 2 |
| Frame map bytes | ≈ 18.4 MiB |
| Maximum tabs | 8 |
| Input ring | 256 |
| Command ring | 32 |
| Cmd arg capacity | 1536 bytes (`WikiCmdEvent::arg`) |
| Open Ext URL capacity | 8192 bytes |
| Open Tab URL capacity | 2048 bytes |
| OSR frame-rate setting | 60 |
| Present idle / interact / wheel | ≈ 30 / 60 / 120 Hz |
| CEF stamp | 150.0.14 |
| Chromium | 150.0.7871.129 |
| Helper / home / sites / cheatsheets stamps | 2209 / 2209 / s2210 / c2210 |
| OSR `device_scale_factor` | 1.0 |
| User-Agent product token | `GW2-InGame-Helper` |
| Browse catalog entries | ≈ 2720 |
| World GPS base half-width | \(20''\) (`kBlishHalfM`) |
| Curated pathing packs | Tekkit AIO; Lady Guides + AP; Hero's Marker Pack |

## Appendix B — Source map (kernel)

| Path | Role |
|------|------|
| `src/entry*.cpp` | Nexus entry, version, WndProc, hotkeys, load/unload |
| `src/ui/UI*.cpp` | ImGui chrome, Browse, options, render |
| `src/browser/WikiBrowser*.cpp` | Host lifecycle, helper launch, IPC, present |
| `src/helper/main.cpp` + State/Tabs/Handlers/Commands | CEF boot, tabs, IPC drain |
| `src/helper/HelperNavPolicy*.cpp` | Navigation and advertisement policy |
| `src/helper/HelperOsrRender.cpp` | OSR paint + screen/view geometry |
| `src/browser/WikiIpc.h` | Shared contract |
| `src/browser/CefRuntime.*` | Download, verify, extract |
| `data/sites.json` + `SitesLoad*` | Catalog |
| `data/cheatsheets/` + `CheatSheets.cpp` | Offline sheets |
| `src/helper/BootJs.h` | Injected page logic |
| `scripts/pack-cef-runtime.sh` | Rehost stock CEF |

## Appendix C — Source map (application domains)

| Path | Role |
|------|------|
| `src/account/*` | Account hub pads + API fetch/parse |
| `src/pathing/*` | Packs, trails, markers, compass, world GPS |
| `src/pathing/WorldGps*` / `WorldOverlay*` | D3D ribbons + ImGui markers |
| `src/logs/*` | DPS Logs + EI runtime |
| `src/events/*` | World Events |
| `src/notes/*` | Notes + waypoints |
| `src/app/*` | Settings, paths, theme, pad dock, Mumble identity |
| `src/api/Gw2Http*` | Blocking WinHTTP (workers only) |

## Appendix D — IPC command and input sketch

See enums `WikiIpcCmd` and `WikiInputType` in [`WikiIpc.h`](../src/browser/WikiIpc.h). Any layout change to `WikiIpcState` requires magic bump and simultaneous DLL+helper ship.

## Appendix E — Smoke checklists

### E.1 Kernel (Browse / CEF)

1. Fresh install: DLL only → first open downloads CEF → homepage paints.
2. Wiki navigate; back/forward; new tab; find-in-page.
3. Collapse/expand; KeepHelperWarm on/off.
4. Open Ext (YouTube card or Discord path).
5. Ad click on a partner page → system browser with full tracker URL (when ads present).
6. Proton: helper launches; Open Ext works from game process.

### E.2 Pathing / GPS

1. Enable Lady Barefoot; world ribbon visible in-world.
2. Chevrons flow forward; width sane at 1.0×.
3. Player clear 0 vs 1; section breaks honored.
4. Find nearest waypoints with categories off.
5. Marker interact (`Ctrl+Shift+F`).

### E.3 Account / Logs

1. API key scopes load wallet/stash.
2. DPS Logs scan → parse (with .NET 8 + EI present).

## Appendix F — Document control

| Field | Value |
|-------|-------|
| Title | Embedding a Contemporary Chromium Browser in a Live Game Client |
| Form | Technical report / engineering whitepaper |
| Register | Systems software / interactive entertainment tooling |
| Peer review | None (project documentation aiming at academic technical-report quality) |
| Distribution | Tracked in git with the repository |
| Last sync | 2.2.0.11 — Legendary Ledger auto armory + Wiki new-tab |
| Update trigger | IPC magic bump; present-path change; CEF major; sandbox policy; advertisement-routing; world GPS compliance surface; module-boundary change |
| How to cite (informal) | xydroc, “Embedding a Contemporary Chromium Browser in a Live Game Client,” GW2 In-Game Helper technical report, rev. 2.2.0.11, 2026. |
