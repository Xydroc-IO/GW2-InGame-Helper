# Embedding a Contemporary Chromium Browser in a Live Game Client:
## Design, Constraints, and Trade-offs of an Out-of-Process OSR Overlay for Guild Wars 2

**Technical report (engineering whitepaper)**  
**Product:** GW2 In-Game Helper  
**Revision described:** 2.1.0.2 (architecture shared with Beta channel)  
**Signature:** `0x48454C50` (`HELP`)  
**IPC contract:** `HLI5` (`0x484C4935`)  
**Runtime:** Chromium Embedded Framework (CEF) Stable 150.0.14 / Chromium 150.0.7871.129  
**Author / maintainer:** xydroc  
**Affiliation:** independent / open-source (MIT)  
**Status:** Published technical report in-repo — not a peer-reviewed journal article  
**Companion notes:** `ARCHITECTURE.md`, `COMPLIANCE.md`, `CEF_RUNTIME.md`, `../CONTRIBUTING.md`

---

## Abstract

In-game overlays that surface the live web (wikis, build sites, community guides) collide with three hard constraints: (1) game clients forbid or break when third-party code hooks the presentation path; (2) modern web content requires a current Chromium, not the game’s aged embedded browser; and (3) Linux players increasingly run Windows games under Proton/Wine, where GPU sharing and sandboxing behave differently from native Windows.

This whitepaper documents the architecture of **GW2 In-Game Helper**, a Raidcore Nexus ImGui addon that embeds **stock CEF 150** as a **separate process**, renders via **windowless off-screen rendering (OSR)** into **PID-scoped shared memory**, and composites frames through the host’s existing Direct3D 11 device without Present hooks. We analyze the IPC contract, adaptive CPU→GPU upload pipeline, navigation and advertisement click-through policy, security posture (including intentional `no-sandbox` under Wine), compliance boundaries relative to Guild Wars 2 and the Nexus ecosystem, and the performance implications for lower-tier hardware. We argue that shared-memory OSR is not an “elite” GPU path, but that it is a *rational* engineering equilibrium given Nexus, ArcDPS/ReShade coexistence, single-DLL distribution, and Proton.

**Keywords:** Chromium Embedded Framework; off-screen rendering; game overlay; shared-memory IPC; Direct3D 11; Wine/Proton; advertising click attribution; process isolation.

---

## 1. Introduction

### 1.1 Motivation

Players of massively multiplayer online games routinely consult external knowledge bases during play: skill tooltips on a wiki, raid rotations on community sites, fractal checklists, food and utility planners. Context switching to a desktop browser breaks immersion and, in competitive or timed content, costs attempts. An *in-client* browser therefore has clear utility—if it can be shipped without violating platform rules, destabilizing the renderer, or stranding Linux users.

Guild Wars 2 (ArenaNet) is typically extended on Windows via community loaders. **Raidcore Nexus** provides a supported ImGui surface, input routing, QuickAccess icons, and a swap-chain pointer—without requiring addons to detour `IDXGISwapChain::Present`. Separately, many players already run **ArcDPS** and/or **ReShade**, which *do* occupy the Present/DXGI hook niche. Any browser overlay that also hooks Present therefore enters a multi-party conflict on the GPU path.

### 1.2 Problem statement

We require a system that:

1. Displays interactive, modern web content inside the game overlay.
2. Uses only Nexus-sanctioned host APIs (no game memory R/W, no Present hooks).
3. Isolates Chromium crashes from the game process.
4. Ships as a **single DLL** for players, with heavy dependencies fetched once.
5. Runs on native Windows **and** Steam Proton / Wine.
6. Preserves publisher advertisement economics where feasible (load ads; do not truncate click trackers).
7. Coexists with ArcDPS and ReShade.

### 1.3 Contributions

This document contributes:

- A full-system description of an **out-of-process CEF OSR** overlay under Nexus.
- A precise account of the **HLI5** shared-memory protocol (commands, input, paint, Open Ext).
- An analysis of **why DXGI shared textures were rejected** despite superior bandwidth properties.
- A navigation policy for **ad click-through** under OSR popup constraints.
- An honest security and performance evaluation, including sandbox and hardware limits.

### 1.4 Non-goals

We do not claim: combat automation; reading player or entity memory; bypassing Cloudflare or Google OAuth in embedded contexts; AAA-grade zero-copy GPU compositing; or a rebuilt/forked Chromium.

---

## 2. Background and related approaches

### 2.1 Chromium Embedded Framework (CEF)

CEF wraps Chromium for embedding. Two rendering modes matter:

- **Windowed:** Chromium owns an OS window. Poor fit for an ImGui panel inside a fullscreen/borderless game.
- **Windowless / OSR:** Chromium paints into a CPU (or, in advanced setups, GPU) buffer via `OnPaint`. The host composites.

Official CEF binaries (commonly distributed via Spotify’s `cef-builds` CDN) ship as versioned archives. GW2 In-Game Helper consumes the **minimal Windows x64** package for Stable 150, then **rehosts** a flattened zip; it does **not** patch Chromium sources.

### 2.2 Overlay strategies in PC games

| Approach | Pros | Cons vs. our constraints |
|----------|------|---------------------------|
| In-process CEF in the game DLL | Simple IPC | Chromium crash = game crash; D3D device contention; AV scrutiny of huge DLL |
| Hook Present + inject HWND | Familiar to overlay tools | Conflicts with ArcDPS/ReShade; ToS/community norms |
| Game’s own CEF (`bin64/cef`) | Already on disk | Wrong CEF generation vs. our headers; writing there is forbidden in our policy |
| External desktop browser + capture | Isolation | High latency; fragile capture; not “in panel” |
| **Out-of-process OSR + shared memory** | Isolation; Nexus-friendly | CPU upload cost; IPC complexity |

### 2.3 Wine / Proton as a first-class target

Steam Deck and Linux desktop users run Guild Wars 2 under Proton. Win32 APIs are translated; GPU features (sandboxed Chromium GPU process, DXGI shared handles, some ShellExecute paths) are incomplete or unreliable. Designs validated only on native Windows silently fail here. Several Helper policies (software OSR switches, DLL-side URL opens, process caps) exist primarily for Proton survivability.

---

## 3. Design goals and explicit trade-offs

| Goal | Decision | Trade-off accepted |
|------|----------|-------------------|
| Crash isolation | Separate `GW2HelperBrowser.exe` | Cross-process IPC tax |
| One-file install | Embed helper + HTML in DLL; download CEF once | First-open bandwidth (~170 MB zip) |
| No Present hook | Nexus `SwapChain` → create own textures | No DXGI shared surface with CEF GPU |
| Modern web | Stock CEF 150, not game CEF 103-era trees | Large runtime; AV attention |
| Proton | `disable-gpu*`, `no-sandbox`, Open Ext via game process | Weaker sandbox; more CPU paint |
| Multi-client | PID-scoped IPC object names | Slightly more complex naming |
| Ad revenue for partner sites | Allow ad loads; externalize click URLs when detected | Mixed in-tab vs external UX |
| Hot-reload | `AF_DisableHotloading` | Users must restart GW2 after DLL updates |

---

## 4. System architecture

### 4.1 Process topology

```text
Gw2-64.exe
 └─ Nexus (loader)
     └─ GW2-InGame-Helper.dll          [IPC host, ImGui, D3D present, CEF installer]
          └─ CreateProcess
              └─ GW2HelperBrowser.exe  [CEF browser process]
                   ├─ libcef.dll (private tree under addons/.../cef/)
                   ├─ utility / network / storage children
                   └─ renderer (capped; software OSR path)
```

The game process never `LoadLibrary`s `libcef.dll`. The helper loads CEF from  
`addons/GW2-InGame-Helper/cef/` after SHA-256-verified install.

### 4.2 Lifecycle

1. **AddonLoad:** register Nexus render/WndProc/keybind/QuickAccess; disable hot-loading.
2. **First open:** ensure addon data dir; `CefRuntime::EnsureInstalled`; extract helper EXE if stamp mismatch; `CreateProcess` with `--cef-dir` and `--host-pid`.
3. **Steady state:** UI posts commands; helper paints; DLL presents; Open Ext seq drained via `ShellExecute` in the **game** process.
4. **Unload / quit:** post `QUIT`; grace period; Job Object `KILL_ON_JOB_CLOSE` reaps children; helper also watches host handle and self-exits if GW2 dies.

### 4.3 On-disk layout (player machine)

```text
<GW2>/addons/GW2-InGame-Helper.dll          # sole required install artifact
<GW2>/addons/GW2-InGame-Helper/
  GW2HelperBrowser.exe (+ .ver stamp)
  cef/   libcef.dll, pak/dat, locales/, cef.ver
  settings.ini
  helper-home.html, cheat-sheet HTML, …
%LOCALAPPDATA%/GW2-InGame-Helper/cef-cache/ # Chromium profile/cache (persists; not under addons/)
```

### 4.4 Build and distribution pipeline

- **Toolchain:** `x86_64-w64-mingw32-g++` (C++17), static libgcc/libstdc++ for the shipping DLL/EXE.
- **Helper:** compiled against `deps/cef` 150 C API headers; embedded as a binary blob in the DLL.
- **CEF zip:** `scripts/pack-cef-runtime.sh` downloads official minimal tarball, flattens `Release/`+`Resources/`, omits unused bootstrap binaries, publishes `cef-runtime-150-windows64.zip` with SHA-256 recorded in `CefRuntime.h`.
- **Updates:** Nexus `UP_GitHub` points at the project repository for DLL updates; CEF zip updates are version-stamped separately (`cef.ver` = `150.0.14`).

---

## 5. Inter-process communication (IPC)

### 5.1 Rationale for shared memory

Named pipes or protobuf RPC would ease schema evolution and tooling. They add copies, scheduling latency, and Wine edge cases for a 60 Hz paint path. Helper instead uses:

- One **control** mapping: packed `WikiIpcState` (`#pragma pack(1)`, magic `HLI5`).
- One **frame** mapping: double-buffered BGRA, capacity \(1920 \times 1200 \times 4 \times 2 \approx 18.4\,\mathrm{MiB}\).
- One **wake** event for low-latency helper wakeup (avoid busy-spin under Proton).

Object names include the host GW2 PID so two game clients do not collide.

### 5.2 Control plane

| Subsystem | Design |
|-----------|--------|
| Commands | Ring buffer size 32 (`NAVIGATE`, tab create/activate/close, find, bounds, visible, quit, …) |
| Discrete input | Ring size 256 (clicks, wheel, keys, focus) — drop oldest on overflow |
| Coalesced mouse | `mouse_x/y/mods/seq` updated continuously; wake throttled |
| URL / title | Length + odd/even sequence protocol to avoid torn reads |
| Status | UTF-8 status line for UI chips |
| Open Ext | `open_ext_seq` + `open_ext_url[8192]` |

**Padding myth:** Because both sides are the same MinGW-built Win32 ABI and the struct is packed, divergent compiler padding between DLL and EXE is not a realistic failure mode. Version skew *is*—hence magic + helper stamp.

**Locking:** There is no long-held mutex on the frame buffer. The DLL sets `frame_reading` to the front index while copying; the helper skips overwriting that buffer. Sequence numbers detect torn or superseded frames.

### 5.3 Open Ext as a Wine correctness feature

`ShellExecute` from the CEF helper process frequently no-ops under Proton. The helper therefore writes the URL into IPC; the DLL performs `ShellExecuteA` in the game process. Ad click trackers routinely exceed 2 KiB; the Open Ext buffer was enlarged to **8 KiB**, and handoff is **refused** rather than truncated (truncation produced blank advertiser landings and zero billable clicks).

### 5.4 Limits of the IPC style

- Schema changes require coordinated DLL+helper deploy (stamp bump).
- Cross-process data races are invisible to ThreadSanitizer.
- Not a general RPC framework; intentionally narrow.

These are accepted costs of a local, latency-sensitive paint channel—not evidence that the design is accidental.

---

## 6. Rendering pipeline

### 6.1 OSR paint (helper)

CEF invokes `OnPaint` with BGRA. The helper copies into the back buffer of the shared frame map, publishes dirty rects, advances `frame_seq` / `frame_front`, and signals the wake event. Inactive tabs do not drive the shared paint surface; up to **eight** browsers may exist (`kWikiMaxTabs`), but only the active slot presents.

`windowless_frame_rate` is set to **60**. Software GPU flags avoid fighting the game’s D3D device under Wine:

- `disable-gpu`, `disable-gpu-compositing`, `disable-gpu-vsync`
- `disable-d3d11`, `disable-direct-composition`
- `in-process-gpu`
- renderer process limit = 1 (Proton process fan-out can stall the host)

### 6.2 Present (DLL)

`WikiBrowser::PresentFrame` runs on the Nexus render tick:

1. Read front buffer metadata; pin via `frame_reading`.
2. Adaptive cadence: ~**120 Hz** during recent wheel input, ~**60 Hz** while interacting, ~**30 Hz** when idle.
3. Map a **STAGING** texture (`DO_NOT_WAIT` after first paint; blocking Map allowed on first paint to avoid the Windows “Ready but Waiting for first paint…” stall).
4. Copy dirty rows (or chunked full frames) into staging.
5. `CopySubresourceRegion` into a **DEFAULT** shader-resource texture sized once to max 1920×1200; ImGui samples with UV crop (`FrameUvMax`).

**Critical rule:** never `Map` the ImGui-bound DEFAULT texture.

### 6.3 Why not DXGI shared surfaces?

A zero-copy path (CEF GPU texture ↔ game device via shared NT handles) would reduce PCIe/CPU traffic. It was rejected for this product because:

1. Nexus exposes a swap chain for **host** drawing, not a contract for importing foreign CEF GPU surfaces.
2. CEF OSR’s portable contract is a CPU bitmap; GPU OSR sharing is fragile across CEF versions.
3. Proton/Wine shared-handle support is incomplete.
4. ArcDPS/ReShade already stress the DXGI stack; adding another GPU importer raises failure modes.

Thus the “sledgehammer” CPU upload is a **constraint-driven** choice. Mitigations (idle throttle, dirty rects, chunking, fixed texture) address the worst hitches without pretending the path is free.

### 6.4 Bandwidth sketch

A full 1920×1200 BGRA frame is \(1920 \times 1200 \times 4 = 9{,}216{,}000\) bytes ≈ 8.8 MiB. At 60 Hz full frames, raw copy pressure approaches ~528 MiB/s before dirty-rect savings. In practice, idle 30 Hz + dirty rects keep average far lower; interactive scrolling is the stress case—especially on dual-core CPUs concurrent with GW2 simulation.

---

## 7. Input, focus, and UI chrome

The ImGui layer (`UI.cpp`) owns window chrome: tabs, Browse catalog, find-in-page, Open Ext, settings. Mouse and keyboard destined for the page are serialized into IPC. Design pressures include:

- Nexus WndProc ordering vs. ImGui capture (drag/resize regressions are documented historically).
- GW2 chat: Space and other keys must not be swallowed incorrectly when the overlay is open but chat is focused.
- Fast typing: synthesize characters via `ToUnicode` when Nexus skips `TranslateMessage`.
- Tab find uses CEF find handlers mirrored into IPC status fields.

Bundled `file://` pages (home, cheat sheets, raid food) are first-class; their stamps force re-extract when HTML changes.

---

## 8. Navigation policy and advertisement economics

### 8.1 OSR popup constraint

OSR cannot host real popup windows. `OnBeforePopup` always cancels native popups. Without a replacement policy, `target=_blank` ad clicks become silent no-ops—catastrophic for publisher CPC.

### 8.2 Externalization rules (v2.0.2.4)

The helper routes to the system browser when user-gesture navigations match any of:

1. Known **click-tracker** URL shapes (`pagead/aclk`, DoubleClick `/pcs/click`, etc.).
2. Landing URLs carrying **network click identifiers** (`gclid`, `gad_source`, `msclkid`, …).
3. Main-frame navigations onto **ad-network hosts**.
4. Navigations whose **referrer** is an ad frame (SafeFrame / DoubleClick), even if the destination is a plain advertiser domain.
5. Subframe-originated promotable URLs from known ad iframes.
6. Cross-site `target=_blank` popups (same-site and bundled `file://` may stay in-tab).

YouTube and `discord://` deep links also prefer external handling where OSR cannot complete the flow.

### 8.3 Screen vs viewport (v2.0.2.8)

OSR `GetViewRect` reports the ImGui panel. `GetScreenInfo` reports the primary monitor and work area with `device_scale_factor` fixed at 1.0 (IPC mouse and paint are view pixels). Matching screen size to the tiny panel made `window.screen` equal the overlay—an easy non-human signal for impression filters. Separating them improves fingerprint hygiene; it does not claim desktop-Chrome viewability.

### 8.4 Attribution honesty

**Billable publisher clicks** generally require the ad network’s tracker URL to be *requested*. Opening a final landing page that already contains `gclid` is necessary but not always sufficient if the `aclk` request never occurred. Empirical nav logs on Proton showed both gold-standard `aclk` handoffs and landing-only paths. Mixed in-tab vs external UX is acceptable when the tracker (or a completed redirect chain) ran; truncated URLs are not.

Optional `navlog.on` enables decision tracing for maintainers; it must not ship enabled.

---

## 9. Security and trust model

### 9.1 Process boundary

Page JavaScript executes in Chromium child processes of the **helper**, not inside `GW2-InGame-Helper.dll`. A renderer compromise is closer to “code as the helper user” than “arbitrary code inserted into the game module”—though still severe.

### 9.2 Sandbox

Shipping configuration sets `no_sandbox` / `--no-sandbox`. Reasons:

- CEF 150+ Windows sandbox expects bootstrap/module packaging not used in our single-EXE helper layout.
- Wine/Proton sandboxing is unreliable.
- Many embedded CEF apps make the same trade.

This is a **conscious downgrade** from desktop Chrome, not an accidental flag flip for ads. Site-isolation experiments for OSR iframe input were considered and **not** retained as policy.

### 9.3 Supply chain

- CEF zip SHA-256 verified before extract.
- Runtime kept out of the DLL blob (Defender ML less likely to treat the whole addon as a giant packed binary).
- Unsigned MinGW builds may still be quarantined; code signing remains future work.

### 9.4 Compliance (game ecosystem)

**Allowed:** Nexus APIs; private CEF under addon dir; local IPC; official `api.guildwars2.com` from BootJs with backoff; terminate only our helper PID; load site ads/consent.

**Forbidden:** game memory R/W; MinHook Present/`d3d11` wrappers; synthetic input into GW2; combat/account automation; writing `bin64/cef`.

These rules are product ethics as much as engineering: they preserve coexistence and reduce ban-surface area.

---

## 10. Content adaptation layer

Because OSR ≠ desktop Chrome:

- **BootJs** injects tips (Open Ext for Google/Cloudflare), armory chip fills via official API, and embed reshaping.
- **CSS compat** rewrites problematic constructs (e.g. Material `color-mix`) before first paint via response filters where needed.
- **Guildjen / YouTube:** iframes promoted to Watch cards / Open Ext rather than unreliable in-OSR playback under software CEF.

Ads, consent, and analytics hosts are intentionally **not** stripped—partner sites depend on them.

---

## 11. Cross-platform behavior

| Concern | Native Windows | Proton / Wine |
|---------|----------------|---------------|
| ISA / PE | Native | Translated |
| CEF GPU | Software flags still on (conservative) | Required for stability |
| Open Ext | ShellExecute usually fine | **Must** go through game process |
| First paint | Historical Map/`DO_NOT_WAIT` stall fixed | Different failure modes (file locks, process caps) |
| AV | Defender ML on MinGW | Less relevant |
| Extract helper | Straightforward | Wine may hold old EXE; stamp + delete on install |

Architecture is Windows-first; Linux success is evidence of Proton resilience, not of a Linux-native port.

---

## 12. Performance and accessibility of lower-tier hardware

### 12.1 Cost centers

1. Chromium helper RSS (hundreds of MB with a heavy page).
2. Shared frame mappings (~18 MiB plus control block).
3. CPU memcpy + staging upload while scrolling.
4. CEF software raster under current flags.

### 12.2 Expected user experience

- Light wiki use, overlay closed in combat: acceptable on hardware that already runs GW2 + Nexus.
- Large overlay + rapid scroll + ad-heavy pages: micro-stutters on weak CPUs.
- In-overlay video: best-effort; Open Ext is the reliable path.
- Dual GW2 clients each with a helper: multiplicative RAM.

Mitigations already shipped (adaptive FPS, dirty rects, chunked upload, warm-hide coalescing) improve the curve but cannot make Chromium free.

---

## 13. Reliability engineering

Observed and addressed classes of failure (non-exhaustive):

- Helper/DLL version skew → stamp + magic.
- IPC ring overflow → drop oldest input; coalesce bounds.
- Proton busy-wait → event-driven wake + idle timeouts.
- Orphan helpers → Job Object + host wait.
- Windows first-paint stall → blocking first Map; staging vs DEFAULT separation.
- Ad URL truncation → 8 KiB Open Ext + refuse-if-still-too-long.
- Launch storms under Proton → crash-loop brakes / process caps.

Memory safety tooling (ASan/TSan) cannot fully validate this stack: the DLL loads into a non-instrumented game; CEF is prebuilt; races are cross-process. Helper-only ASan and stress tests remain the realistic assurance path.

---

## 14. Evaluation criteria (qualitative)

| Criterion | Assessment |
|-----------|------------|
| Functional web browsing in-overlay | Met for primary content sites |
| Crash isolation | Met (helper death ≠ mandatory game death) |
| Nexus / ArcDPS / ReShade coexistence | Met by construction (no Present hook) |
| Proton viability | Met with software CEF + Open Ext IPC |
| Publisher ad clicks | Partially met; tracker-intact paths strong; landing-only paths uncertain |
| Zero-copy GPU | Not met (explicitly deferred) |
| Sandbox parity with Chrome | Not met (documented) |
| Low-end hitch-free | Not guaranteed under heavy overlay use |

---

## 15. Future work

1. **Windows sandbox path** using CEF bootstrap/`--module` packaging, retaining `no-sandbox` only under Wine detection.
2. **Optional DXGI shared texture** experiment behind a native-Windows flag (never default on Proton).
3. **Code signing** to reduce Defender false positives.
4. **Helper-only ASan** Makefile target for continuous smoke.
5. **Stricter ad attribution:** prefer completing network click navigations before cancel/replace with landing-only Open Ext.
6. Schema evolution aids (explicit IPC version negotiation beyond magic).
7. **CI:** Local `make ci` (sites validate + CSS tests + MinGW smoke); optional `.githooks/pre-push`. GitHub Actions are not used.
8. **Further decomposition** of `WikiBrowser.cpp` and `helper/main.cpp` once fixture harnesses exist for present/IPC races.

### 15.1 Maintainability trajectory (2.1.x)

As of 2.1.0.2 the Browse catalog is data-driven (`data/sites.json` → generated C++), and former “god files” (`UI`, LogManager, Tekkit, LivePanels, CheatSheets) are split into parse/build/upload/index translation units. This reduces merge conflict surface for feature work but does **not** remove the need for restricted ownership of the CEF/IPC/present path. See `ARCHITECTURE.md` §7 and `CONTRIBUTING.md`.

---

## 16. Conclusion

GW2 In-Game Helper demonstrates that a **contemporary Chromium** can be productized inside a **live MMO client** without Present hooks or game-memory intrusion by combining: Nexus as the host UI substrate; **out-of-process CEF OSR**; **PID-scoped shared memory**; **adaptive D3D11 staging uploads**; and **Proton-aware** process and ShellExecute policies. The architecture is deliberately “unglamorous” relative to AAA GPU sharing—and that unglamorous path is why it fits the actual constraint set of the Guild Wars 2 addon ecosystem.

Critics correctly note IPC rigidity, CPU upload cost, and sandbox gaps. Those are **priced-in trade-offs**, not unrecognized accidents. The appropriate research and engineering question is not whether a zero-copy sandboxed compositor would be nicer in the abstract—it would—but whether it can be shipped as a single community DLL beside ArcDPS on Windows and Steam Deck without breaking the game. Under that objective function, the present design is coherent.

---

## References (informal)

1. Chromium Embedded Framework — https://bitbucket.org/chromiumembedded/cef  
2. CEF Automated Builds — https://cef-builds.spotifycdn.com/  
3. Raidcore Nexus — https://raidcore.gg/gw2/nexus  
4. Dear ImGui — https://github.com/ocornut/imgui  
5. Microsoft DXGI / Direct3D 11 documentation (shared resources; staging textures)  
6. Wine / Steam Proton compatibility layers  
7. Google Ads click tracking / `gclid` attribution (industry practice)  
8. Project companions: `ARCHITECTURE.md`, `COMPLIANCE.md`, `CEF_RUNTIME.md`, `BUILD.md`, `RELEASE_NOTES.md`

---

## Appendix A — Quantitative constants (v2.1.0.2)

| Constant | Value |
|----------|-------|
| Addon version | 2.1.0.2 |
| Nexus signature | `HELP` / `0x48454C50` |
| IPC magic | `HLI5` / `0x484C4935` |
| Max frame | 1920 × 1200 BGRA |
| Frame buffers | 2 |
| Max tabs | 8 |
| Input ring | 256 |
| Cmd ring | 32 |
| Open Ext URL cap | 8192 bytes |
| OSR frame rate setting | 60 |
| Present idle / interact / wheel | ~30 / ~60 / ~120 Hz |
| CEF stamp | 150.0.14 |
| Chromium | 150.0.7871.129 |
| Helper / home stamps | 2102 / 2102 |
| OSR `device_scale_factor` | 1.0 (view-pixel mouse/paint) |
| User-Agent product token | `GW2-InGame-Helper` |
| Browse catalog source | `data/sites.json` (~2718 entries) |

## Appendix B — Source map

| Path | Role |
|------|------|
| `src/entry.cpp` | Nexus entry, version, WndProc |
| `src/UI.cpp` / `UI_Browse.cpp` / `UI_Options.cpp` | ImGui chrome, Browse, options |
| `src/WikiBrowser.cpp` | IPC host, launch, present, Open Ext |
| `src/WikiIpc.h` | Shared contract |
| `src/CefRuntime.*` | Download / verify / extract |
| `data/sites.json` → `Sites.gen.cpp` + `Sites.cpp` | Catalog + runtime |
| `LogManager*` / `Tekkit*` / `LivePanels*` | Feature modules (split TUs) |
| `src/helper/main.cpp` | CEF client, nav/ad policy, OSR |
| `src/helper/BootJs.h` | Injected page logic |
| `scripts/pack-cef-runtime.sh` | Rehost stock CEF |

## Appendix C — Document control

| Field | Value |
|-------|-------|
| Title | Embedding a Contemporary Chromium Browser in a Live Game Client |
| Form | Technical report / engineering whitepaper |
| Peer review | None (project documentation) |
| Distribution | Tracked in git with the repository |
| Last sync | 2.1.0.2 modular tree |
| Update trigger | IPC magic bump; present-path change; CEF major; sandbox policy change; ad-routing change; module-boundary change |
