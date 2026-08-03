# Embedding a Contemporary Chromium Browser in a Live Game Client

## Design Rationale, Constraints, and Trade-offs of an Out-of-Process Off-Screen Overlay for Guild Wars 2

| Field | Value |
|-------|-------|
| Document type | Technical report (engineering whitepaper) |
| Product | GW2 In-Game Helper |
| Revision described | 2.2.0.0 |
| Nexus signature | `HELP` (`0x48454C50`) |
| IPC contract | `HLI5` (`0x484C4935`) |
| Runtime | Chromium Embedded Framework (CEF) Stable 150.0.14 / Chromium 150.0.7871.129 |
| Author | xydroc |
| Affiliation | Independent open-source project (MIT License) |
| Peer review | None (project technical report; not a journal article) |
| Companions | [`ARCHITECTURE.md`](ARCHITECTURE.md), [`COMPLIANCE.md`](COMPLIANCE.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`../CONTRIBUTING.md`](../CONTRIBUTING.md) |

---

## Abstract

In-game overlays that surface the live web—wikis, build repositories, and community guides—must simultaneously satisfy three hard constraints. First, contemporary game clients and community norms typically forbid third-party hooks on the presentation path. Second, modern web content requires a current Chromium stack rather than the game’s aged embedded browser. Third, a growing fraction of players execute Windows titles under Proton or Wine, where GPU sharing and process sandboxing diverge from native Windows behaviour.

This report documents the architecture of **GW2 In-Game Helper**, a Raidcore Nexus ImGui addon that embeds **stock CEF 150** as a **separate process**, renders via **windowless off-screen rendering (OSR)** into **PID-scoped shared memory**, and composites frames through the host’s existing Direct3D 11 device without Present hooks. We analyse the inter-process communication (IPC) contract, the adaptive CPU-to-GPU upload pipeline, navigation and advertisement click-through policy, the security posture (including intentional sandbox disablement under Wine), compliance boundaries relative to Guild Wars 2 and the Nexus ecosystem, and performance implications for constrained hardware. We argue that shared-memory OSR is not an optimal GPU path in the abstract, but that it constitutes a **rational engineering equilibrium** given Nexus API surfaces, coexistence with ArcDPS and ReShade, single-DLL distribution, and Proton viability.

**Keywords:** Chromium Embedded Framework; off-screen rendering; game overlay; shared-memory IPC; Direct3D 11; Wine; Proton; advertisement attribution; process isolation.

---

## 1. Introduction

### 1.1 Motivation

Players of massively multiplayer online games routinely consult external knowledge bases during play: skill definitions on a wiki, raid rotations on community sites, fractal checklists, and consumable planners. Context-switching to a desktop browser interrupts immersion and, in timed or competitive content, can cost attempts. An *in-client* browser therefore has clear utility—provided it can be distributed without violating platform rules, destabilising the renderer, or excluding Linux users who run the title under compatibility layers.

Guild Wars 2 (ArenaNet) is commonly extended on Windows via community loaders. **Raidcore Nexus** provides a supported ImGui surface, input routing, QuickAccess icons, and a swap-chain pointer, without requiring addons to detour `IDXGISwapChain::Present`. Independently, many players already run **ArcDPS** and/or **ReShade**, which occupy the Present and DXGI hook niche. Any browser overlay that also hooks Present therefore enters a multi-party conflict on the GPU path.

### 1.2 Problem statement

We require a system that:

1. Displays interactive, contemporary web content inside the game overlay.
2. Uses only Nexus-sanctioned host APIs (no game-memory read/write; no Present hooks).
3. Isolates Chromium crashes from the game process.
4. Ships as a **single DLL** for end users, with heavy dependencies fetched once.
5. Executes on native Windows **and** Steam Proton / Wine.
6. Preserves publisher advertisement economics where feasible (load ads; do not truncate click trackers).
7. Coexists with ArcDPS and ReShade.

### 1.3 Contributions

This document contributes:

1. A full-system description of an **out-of-process CEF OSR** overlay hosted under Nexus.
2. A precise account of the **HLI5** shared-memory protocol (commands, input, paint, Open Ext).
3. An analysis of **why DXGI shared textures were rejected** despite superior bandwidth properties.
4. A navigation policy for **advertisement click-through** under OSR popup constraints.
5. A security and performance evaluation, including sandbox limitations and hardware bounds.
6. An explicit statement of evaluation criteria and threats to validity for qualitative claims.

### 1.4 Non-goals

We do not claim: combat automation; reading player or entity memory; bypassing Cloudflare or Google OAuth in embedded contexts; AAA-grade zero-copy GPU compositing; or a rebuilt or forked Chromium. Feature pads (Account, Pathing, DPS Logs, and related modules) are treated as *application layer* atop the browser kernel and are summarised only where they affect IPC, present, or compliance.

### 1.5 Document organisation

Section 2 situates the work against CEF and common overlay strategies. Section 3 states design goals and accepted trade-offs. Sections 4–6 describe process topology, IPC, and rendering. Sections 7–8 cover input and navigation policy. Sections 9–13 address security, content adaptation, cross-platform behaviour, performance, and reliability. Section 14 states qualitative evaluation criteria; Section 15 outlines future work; Section 16 concludes. Appendices record constants, a source map, and document control.

---

## 2. Background and related approaches

### 2.1 Chromium Embedded Framework

CEF wraps Chromium for embedding [1], [2]. Two rendering modes are relevant:

- **Windowed.** Chromium owns an OS window. This is a poor fit for an ImGui panel inside a fullscreen or borderless game.
- **Windowless / OSR.** Chromium paints into a CPU (or, in advanced configurations, GPU) buffer via `OnPaint`. The host composites.

Official CEF binaries—commonly distributed via Spotify’s automated builds [2]—ship as versioned archives. GW2 In-Game Helper consumes the **minimal Windows x64** package for Stable 150 and **rehosts** a flattened zip; it does **not** patch Chromium sources.

### 2.2 Overlay strategies in PC games

| Approach | Advantages | Disadvantages relative to our constraints |
|----------|------------|-------------------------------------------|
| In-process CEF in the game DLL | Simple IPC | Chromium crash implies game crash; D3D device contention; antivirus scrutiny of a large DLL |
| Hook Present and inject an HWND | Familiar to overlay tooling | Conflicts with ArcDPS/ReShade; community and ToS norms |
| Game’s own CEF (`bin64/cef`) | Already on disk | Wrong CEF generation relative to our headers; writing there is forbidden by project policy |
| External desktop browser with capture | Isolation | High latency; fragile capture; not “in panel” |
| **Out-of-process OSR with shared memory** | Isolation; Nexus-compatible | CPU upload cost; IPC complexity |

### 2.3 Wine and Proton as first-class targets

Steam Deck and Linux desktop users run Guild Wars 2 under Proton [6]. Win32 APIs are translated; GPU features (sandboxed Chromium GPU process, DXGI shared handles, some `ShellExecute` paths) are incomplete or unreliable. Designs validated only on native Windows fail silently in this environment. Several Helper policies—software OSR switches, DLL-side URL opens, and process caps—exist primarily for Proton survivability.

### 2.4 Positioning

This work is a **systems engineering report** for a shipping community addon, not a comparative benchmark study. Related commercial and open overlays are acknowledged as design alternatives (Table in §2.2); quantitative head-to-head evaluation is out of scope. Normative compliance rules are maintained separately in [`COMPLIANCE.md`](COMPLIANCE.md); operational module maps live in [`ARCHITECTURE.md`](ARCHITECTURE.md).

---

## 3. Design goals and explicit trade-offs

| Goal | Decision | Trade-off accepted |
|------|----------|--------------------|
| Crash isolation | Separate `GW2HelperBrowser.exe` | Cross-process IPC cost |
| One-file install | Embed helper and HTML in the DLL; download CEF once | First-open bandwidth (~170 MB zip) |
| No Present hook | Nexus `SwapChain` → create host textures | No DXGI shared surface with CEF GPU |
| Modern web | Stock CEF 150, not game CEF trees | Large runtime; antivirus attention |
| Proton viability | `disable-gpu*`, `no-sandbox`, Open Ext via game process | Weaker sandbox; more CPU paint |
| Multi-client | PID-scoped IPC object names | Slightly more complex naming |
| Partner ad revenue | Allow ad loads; externalise click URLs when detected | Mixed in-tab versus external UX |
| Hot-reload | `AF_DisableHotloading` | Users must restart Guild Wars 2 after DLL updates |

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

The game process never loads `libcef.dll`. The helper loads CEF from `addons/GW2-InGame-Helper/cef/` after SHA-256-verified installation [3].

### 4.2 Lifecycle

1. **AddonLoad.** Register Nexus render, WndProc, keybind, and QuickAccess handlers; disable hot-loading.
2. **First open.** Ensure the addon data directory; `CefRuntime::EnsureInstalled`; extract the helper executable if the stamp mismatches; `CreateProcess` with `--cef-dir` and `--host-pid`.
3. **Steady state.** The UI posts commands; the helper paints; the DLL presents; Open Ext sequences are drained via `ShellExecute` in the **game** process.
4. **Unload / quit.** Post `QUIT`; grace period; Job Object `KILL_ON_JOB_CLOSE` reaps children; the helper also watches the host handle and exits if Guild Wars 2 terminates.

### 4.3 On-disk layout (deployment)

```text
<GW2>/addons/GW2-InGame-Helper.dll          # sole required install artefact
<GW2>/addons/GW2-InGame-Helper/
  GW2HelperBrowser.exe (+ .ver stamp)
  cef/   libcef.dll, pak/dat, locales/, cef.ver
  settings.ini
  helper-home.html, cheat-sheet HTML, pathing/, …
%LOCALAPPDATA%/GW2-InGame-Helper/cef-cache/ # Chromium profile/cache (not under addons/)
```

### 4.4 Build and distribution pipeline

- **Toolchain.** `x86_64-w64-mingw32-g++` (C++17), with static `libgcc` / `libstdc++` for the shipping DLL and helper executable.
- **Helper.** Compiled against `deps/cef` 150 C API headers; embedded as a binary blob in the DLL.
- **CEF archive.** `scripts/pack-cef-runtime.sh` downloads the official minimal tarball, flattens `Release/` and `Resources/`, omits unused bootstrap binaries, and publishes `cef-runtime-150-windows64.zip` with SHA-256 recorded in `CefRuntime.h`.
- **Updates.** Nexus `UP_GitHub` points at the project repository for DLL updates; CEF zip updates are version-stamped separately (`cef.ver` = `150.0.14`).

Operational detail and source maps are maintained in [`ARCHITECTURE.md`](ARCHITECTURE.md) and [`BUILD.md`](BUILD.md).

---

## 5. Inter-process communication

### 5.1 Rationale for shared memory

Named pipes or a schema-oriented RPC layer would ease evolution and tooling. They introduce additional copies, scheduling latency, and Wine edge cases on a 60 Hz paint path. The Helper therefore uses:

1. One **control** mapping: packed `WikiIpcState` (`#pragma pack(1)`, magic `HLI5`).
2. One **frame** mapping: double-buffered BGRA with capacity  
   \(1920 \times 1200 \times 4 \times 2 \approx 18.4\,\mathrm{MiB}\).
3. One **wake** event for low-latency helper wakeup (avoiding busy-spin under Proton).

Object names include the host Guild Wars 2 process identifier so concurrent clients do not collide.

### 5.2 Control plane

| Subsystem | Design |
|-----------|--------|
| Commands | Ring buffer of size 32 (`NAVIGATE`, tab create/activate/close, find, bounds, visible, quit, …) |
| Discrete input | Ring of size 256 (clicks, wheel, keys, focus); drop oldest on overflow |
| Coalesced mouse | `mouse_x/y/mods/seq` updated continuously; wake throttled |
| URL / title | Length plus odd/even sequence protocol to avoid torn reads |
| Status | UTF-8 status line for UI chips |
| Open Ext | `open_ext_seq` + `open_ext_url[8192]` |

Because both sides share a MinGW-built Win32 ABI and the struct is packed, divergent compiler padding between DLL and executable is not a realistic failure mode. **Version skew is**—hence magic and helper stamp coordination.

**Locking.** There is no long-held mutex on the frame buffer. The DLL sets `frame_reading` to the front index while copying; the helper skips overwriting that buffer. Sequence numbers detect torn or superseded frames.

### 5.3 Open Ext as a Wine correctness feature

`ShellExecute` from the CEF helper process frequently no-ops under Proton. The helper therefore writes the URL into IPC; the DLL performs `ShellExecuteA` in the game process. Advertisement click trackers routinely exceed 2 KiB; the Open Ext buffer was enlarged to **8 KiB**, and handoff is **refused** rather than truncated. Truncation previously produced blank advertiser landings and zero billable clicks.

### 5.4 Limits of the IPC style

- Schema changes require coordinated DLL and helper deployment (stamp bump).
- Cross-process data races are invisible to ThreadSanitizer.
- The channel is intentionally narrow; it is not a general RPC framework.

These properties are accepted costs of a local, latency-sensitive paint channel.

---

## 6. Rendering pipeline

### 6.1 OSR paint (helper)

CEF invokes `OnPaint` with BGRA. The helper copies into the back buffer of the shared frame map, publishes dirty rectangles, advances `frame_seq` / `frame_front`, and signals the wake event. Inactive tabs do not drive the shared paint surface; up to **eight** browsers may exist (`kWikiMaxTabs`), but only the active slot presents.

`windowless_frame_rate` is set to **60**. Software GPU flags avoid contention with the game’s D3D device under Wine:

- `disable-gpu`, `disable-gpu-compositing`, `disable-gpu-vsync`
- `disable-d3d11`, `disable-direct-composition`
- `in-process-gpu`
- renderer process limit = 1 (Proton process fan-out can stall the host)

### 6.2 Present (DLL)

`WikiBrowser::PresentFrame` executes on the Nexus render tick:

1. Read front-buffer metadata; pin via `frame_reading`.
2. Adaptive cadence: approximately **120 Hz** during recent wheel input, **60 Hz** while interacting, **30 Hz** when idle.
3. Map a **STAGING** texture (`DO_NOT_WAIT` after first paint; blocking `Map` permitted on first paint to avoid the Windows “Ready but Waiting for first paint…” stall).
4. Copy dirty rows (or chunked full frames) into staging.
5. `CopySubresourceRegion` into a **DEFAULT** shader-resource texture sized once to a maximum of \(1920 \times 1200\); ImGui samples with UV crop (`FrameUvMax`).

**Invariant:** never `Map` the ImGui-bound DEFAULT texture.

### 6.3 Rejection of DXGI shared surfaces

A zero-copy path (CEF GPU texture shared with the game device via NT handles) would reduce PCIe and CPU traffic [5]. It was rejected for this product because:

1. Nexus exposes a swap chain for **host** drawing, not a contract for importing foreign CEF GPU surfaces.
2. CEF OSR’s portable contract is a CPU bitmap; GPU OSR sharing is fragile across CEF versions.
3. Proton and Wine shared-handle support is incomplete.
4. ArcDPS and ReShade already stress the DXGI stack; an additional GPU importer raises failure modes.

Thus the CPU upload path is a **constraint-driven** choice. Mitigations—idle throttle, dirty rectangles, chunking, and a fixed texture—address the worst hitches without claiming the path is free.

### 6.4 Bandwidth sketch

A full \(1920 \times 1200\) BGRA frame occupies  
\(1920 \times 1200 \times 4 = 9{,}216{,}000\) bytes ≈ 8.8 MiB.  
At 60 Hz full frames, raw copy pressure approaches ≈ 528 MiB/s before dirty-rectangle savings. In practice, idle 30 Hz operation plus dirty rectangles keep the average far lower; interactive scrolling is the stress case—especially on dual-core CPUs concurrent with Guild Wars 2 simulation.

---

## 7. Input, focus, and user-interface chrome

The ImGui layer owns window chrome: tabs, Browse catalog, find-in-page, Open Ext, and settings [4]. Mouse and keyboard destined for the page are serialised into IPC. Design pressures include:

- Nexus WndProc ordering versus ImGui capture (historical drag and resize regressions).
- Guild Wars 2 chat: Space and related keys must not be swallowed incorrectly when the overlay is open but chat is focused.
- Fast typing: synthesise characters via `ToUnicode` when Nexus skips `TranslateMessage`.
- Tab find uses CEF find handlers mirrored into IPC status fields.

Bundled `file://` pages (home, cheat sheets, raid food) are first-class; version stamps force re-extract when HTML changes.

---

## 8. Navigation policy and advertisement economics

### 8.1 OSR popup constraint

OSR cannot host real popup windows. `OnBeforePopup` always cancels native popups. Without a replacement policy, `target=_blank` advertisement clicks become silent no-ops—catastrophic for publisher cost-per-click economics [7].

### 8.2 Externalisation rules

The helper routes to the system browser when user-gesture navigations match any of:

1. Known **click-tracker** URL shapes (`pagead/aclk`, DoubleClick `/pcs/click`, and related forms).
2. Landing URLs carrying **network click identifiers** (`gclid`, `gad_source`, `msclkid`, …).
3. Main-frame navigations onto **ad-network hosts**.
4. Navigations whose **referrer** is an ad frame (SafeFrame / DoubleClick), even if the destination is a plain advertiser domain.
5. Subframe-originated promotable URLs from known ad iframes.
6. Cross-site `target=_blank` popups (same-site and bundled `file://` may remain in-tab).

YouTube and `discord://` deep links also prefer external handling where OSR cannot complete the flow.

### 8.3 Screen versus viewport geometry

OSR `GetViewRect` reports the ImGui panel. `GetScreenInfo` reports the primary monitor and work area with `device_scale_factor` fixed at 1.0 (IPC mouse and paint are view pixels). Matching screen size to the tiny panel made `window.screen` equal the overlay—an easy non-human signal for impression filters. Separating them improves fingerprint hygiene; it does not claim desktop-Chrome viewability.

### 8.4 Attribution honesty

**Billable publisher clicks** generally require the ad network’s tracker URL to be *requested*. Opening a final landing page that already contains `gclid` is necessary but not always sufficient if the `aclk` request never occurred. Empirical navigation logs on Proton showed both gold-standard `aclk` handoffs and landing-only paths. Mixed in-tab versus external UX is acceptable when the tracker (or a completed redirect chain) ran; truncated URLs are not.

Optional `navlog.on` enables decision tracing for maintainers; it must not ship enabled.

---

## 9. Security and trust model

### 9.1 Process boundary

Page JavaScript executes in Chromium child processes of the **helper**, not inside `GW2-InGame-Helper.dll`. A renderer compromise is closer to “code as the helper user” than “arbitrary code inserted into the game module”—though still severe.

### 9.2 Sandbox

Shipping configuration sets `no_sandbox` / `--no-sandbox`. Reasons:

1. CEF 150+ Windows sandbox expects bootstrap and module packaging not used in our single-executable helper layout.
2. Wine and Proton sandboxing is unreliable.
3. Many embedded CEF applications make the same trade.

This is a **conscious downgrade** from desktop Chrome, not an accidental flag flip for advertisements. Site-isolation experiments for OSR iframe input were considered and **not** retained as policy.

### 9.3 Supply chain

- CEF zip SHA-256 verified before extract.
- Runtime kept out of the DLL blob (reducing the likelihood that Defender ML treats the whole addon as a giant packed binary).
- Unsigned MinGW builds may still be quarantined; code signing remains future work.

### 9.4 Compliance (game ecosystem)

**Allowed.** Nexus APIs; private CEF under the addon directory; local IPC; official `api.guildwars2.com` reads with backoff; terminate only our helper PID; load site ads and consent; curated pathing packs for display; optional Elite Insights CLI under the addon tree.

**Forbidden.** Game memory read/write; MinHook Present or `d3d11` wrappers; synthetic input into Guild Wars 2; combat or account automation; writing `bin64/cef`.

These rules are product ethics as well as engineering: they preserve coexistence and reduce ban-surface area. Authoritative normative text: [`COMPLIANCE.md`](COMPLIANCE.md).

---

## 10. Content adaptation layer

Because OSR is not desktop Chrome:

- **BootJs** injects tips (Open Ext for Google and Cloudflare), armory chip fills via the official API, and embed reshaping.
- **CSS compatibility** rewrites problematic constructs (for example Material `color-mix`) before first paint via response filters where needed.
- **Guildjen / YouTube:** iframes are promoted to Watch cards / Open Ext rather than unreliable in-OSR playback under software CEF.

Advertisement, consent, and analytics hosts are intentionally **not** stripped—partner sites depend on them.

---

## 11. Cross-platform behaviour

| Concern | Native Windows | Proton / Wine |
|---------|----------------|---------------|
| ISA / PE | Native | Translated |
| CEF GPU | Software flags retained (conservative) | Required for stability |
| Open Ext | `ShellExecute` usually sufficient | **Must** proceed through the game process |
| First paint | Historical `Map` / `DO_NOT_WAIT` stall addressed | Different failure modes (file locks, process caps) |
| Antivirus | Defender ML on MinGW | Less relevant |
| Extract helper | Straightforward | Wine may hold an old executable; stamp and delete on install |

The architecture is Windows-first; Linux success is evidence of Proton resilience, not of a Linux-native port.

---

## 12. Performance and lower-tier hardware

### 12.1 Cost centres

1. Chromium helper resident set size (hundreds of megabytes with a heavy page).
2. Shared frame mappings (~18 MiB plus control block).
3. CPU `memcpy` and staging upload while scrolling.
4. CEF software raster under current flags.

### 12.2 Expected experience

- Light wiki use with the overlay closed in combat: acceptable on hardware that already runs Guild Wars 2 with Nexus.
- Large overlay with rapid scroll and advertisement-heavy pages: micro-stutters on weak CPUs.
- In-overlay video: best-effort; Open Ext is the reliable path.
- Dual Guild Wars 2 clients each with a helper: multiplicative RAM.

Mitigations already shipped (adaptive frame rate, dirty rectangles, chunked upload, warm-hide coalescing) improve the curve but cannot make Chromium free.

---

## 13. Reliability engineering

Observed and addressed classes of failure (non-exhaustive):

- Helper/DLL version skew → stamp and magic.
- IPC ring overflow → drop oldest input; coalesce bounds.
- Proton busy-wait → event-driven wake and idle timeouts.
- Orphan helpers → Job Object and host wait.
- Windows first-paint stall → blocking first `Map`; staging versus DEFAULT separation.
- Advertisement URL truncation → 8 KiB Open Ext and refuse-if-still-too-long.
- Launch storms under Proton → crash-loop brakes and process caps.

Memory-safety tooling (ASan, TSan) cannot fully validate this stack: the DLL loads into a non-instrumented game; CEF is prebuilt; races are cross-process. Helper-only ASan and stress tests remain the realistic assurance path.

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

### 14.2 Threats to validity

1. **Qualitative evaluation.** Claims in §14.1 are based on maintainer observation and in-game smoke testing, not a controlled user study or automated visual regression suite.
2. **Hardware heterogeneity.** Bandwidth sketches (§6.4) are analytical upper bounds; measured hitch rates vary with CPU, GPU, and concurrent overlays.
3. **Proton variance.** Steam runtime and Wine versions change behaviour; policies that hold on one Proton build may regress on another.
4. **Advertisement attribution.** Network-side billability cannot be verified from the client alone; we report decision rules and observed navigation shapes, not publisher invoices.
5. **Scope creep.** Application-layer pads (Account, Pathing, DPS Logs) evolve independently of the CEF kernel; this report’s kernel claims should not be read as coverage of every pad feature.

---

## 15. Future work

1. A Windows sandbox path using CEF bootstrap / `--module` packaging, retaining `no-sandbox` only under Wine detection.
2. An optional DXGI shared-texture experiment behind a native-Windows flag (never default on Proton).
3. Code signing to reduce Defender false positives.
4. A helper-only ASan Makefile target for continuous smoke testing.
5. Stricter advertisement attribution: prefer completing network click navigations before cancel/replace with landing-only Open Ext.
6. Schema evolution aids (explicit IPC version negotiation beyond magic).
7. Continuous integration on public pushes and pull requests (`make ci`); local hooks for the same gate offline.
8. Further decomposition of remaining helper boot and IPC paths.

### 15.1 Maintainability trajectory

As of revision 2.2.0.0 the Browse catalog is data-driven (`data/sites.json` → embedded and runtime extract via `SitesLoad`), and former monolithic translation units (`UI`, LogManager, Tekkit/Pathing, LivePanels, CheatSheets, WikiBrowser, helper) are split into focused units. This reduces merge-conflict surface for feature work but does **not** remove the need for restricted ownership of the CEF, IPC, and present path. See [`ARCHITECTURE.md`](ARCHITECTURE.md) §7 and [`CONTRIBUTING.md`](../CONTRIBUTING.md).

---

## 16. Conclusion

GW2 In-Game Helper demonstrates that a **contemporary Chromium** can be productised inside a **live MMO client** without Present hooks or game-memory intrusion by combining: Nexus as the host UI substrate; **out-of-process CEF OSR**; **PID-scoped shared memory**; **adaptive Direct3D 11 staging uploads**; and **Proton-aware** process and `ShellExecute` policies. Relative to AAA GPU-sharing designs, the architecture is deliberately conservative—and that conservatism is why it fits the constraint set of the Guild Wars 2 addon ecosystem.

IPC rigidity, CPU upload cost, and sandbox gaps are correctly noted by critics. Those properties are **priced-in trade-offs**, not unrecognised accidents. The appropriate engineering question is not whether a zero-copy sandboxed compositor would be preferable in the abstract—it would—but whether it can be shipped as a single community DLL beside ArcDPS on Windows and Steam Deck without breaking the game. Under that objective function, the present design is coherent.

---

## References

1. Chromium Embedded Framework. Project site. Available: https://bitbucket.org/chromiumembedded/cef  
2. CEF Automated Builds (Spotify CDN). Available: https://cef-builds.spotifycdn.com/  
3. Raidcore Nexus. Guild Wars 2 addon loader. Available: https://raidcore.gg/gw2/nexus  
4. O. Cornut, *Dear ImGui*. Available: https://github.com/ocornut/imgui  
5. Microsoft Corporation. DXGI / Direct3D 11 documentation (shared resources; staging textures). Available: https://learn.microsoft.com/windows/win32/direct3ddxgi/  
6. Wine Project; Valve Corporation, *Proton*. Compatibility layers for Windows applications on Linux.  
7. Google Ads Help. Click tracking and `gclid` attribution (industry practice).  
8. GW2 In-Game Helper project companions: [`ARCHITECTURE.md`](ARCHITECTURE.md), [`COMPLIANCE.md`](COMPLIANCE.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`BUILD.md`](BUILD.md), [`RELEASE_NOTES.md`](RELEASE_NOTES.md).

---

## Appendix A — Quantitative constants (revision 2.2.0.0)

| Constant | Value |
|----------|-------|
| Addon version | 2.2.0.0 |
| Nexus signature | `HELP` / `0x48454C50` |
| IPC magic | `HLI5` / `0x484C4935` |
| Maximum frame | \(1920 \times 1200\) BGRA |
| Frame buffers | 2 |
| Maximum tabs | 8 |
| Input ring | 256 |
| Command ring | 32 |
| Open Ext URL capacity | 8192 bytes |
| OSR frame-rate setting | 60 |
| Present idle / interact / wheel | ≈ 30 / 60 / 120 Hz |
| CEF stamp | 150.0.14 |
| Chromium | 150.0.7871.129 |
| Helper / home / sites stamps | 2200 / 2200 / s2200 |
| OSR `device_scale_factor` | 1.0 (view-pixel mouse and paint) |
| User-Agent product token | `GW2-InGame-Helper` |
| Browse catalog source | `data/sites.json` (≈ 2718 entries) |
| Curated pathing packs | Tekkit All-In-One; Lady Elyssa Guides + Achievements; Hero's Marker Pack |

## Appendix B — Source map (kernel)

| Path | Role |
|------|------|
| `src/entry.cpp` | Nexus entry, version, WndProc |
| `src/ui/UI.cpp` / `UI_Browse.cpp` / `UI_Options.cpp` | ImGui chrome, Browse, options |
| `src/browser/WikiBrowser*.cpp` | Host: lifecycle, helper launch, IPC, present |
| `src/helper/main.cpp` + `HelperNavPolicy` / `HelperOsrRender` | CEF client, navigation and advertisement policy, OSR |
| `src/browser/WikiIpc.h` | Shared contract |
| `src/browser/CefRuntime.*` | Download, verify, extract |
| `data/sites.json` → runtime `sites.json` + `src/browse/SitesLoad.cpp` | Catalog |
| `data/cheatsheets/` → zip extract + `src/browse/CheatSheets.cpp` | Offline `about:` sheets |
| `src/helper/BootJs.h` | Injected page logic |
| `scripts/pack-cef-runtime.sh` | Rehost stock CEF |

Application-layer modules live under `src/account/`, `src/pathing/`, `src/logs/`, `src/events/`, and `src/notes/`. See [`ARCHITECTURE.md`](ARCHITECTURE.md); they are outside the CEF kernel ownership zone defined in [`CONTRIBUTING.md`](../CONTRIBUTING.md).

## Appendix C — Document control

| Field | Value |
|-------|-------|
| Title | Embedding a Contemporary Chromium Browser in a Live Game Client |
| Form | Technical report / engineering whitepaper |
| Register | Systems software / interactive entertainment tooling |
| Peer review | None (project documentation aiming at academic technical-report quality) |
| Distribution | Tracked in git with the repository |
| Last sync | 2.2.0.0 — academic rewrite; curated Tekkit / Lady / Hero Pathing noted in Appendix A |
| Update trigger | IPC magic bump; present-path change; CEF major; sandbox policy change; advertisement-routing change; module-boundary change |
| How to cite (informal) | xydroc, “Embedding a Contemporary Chromium Browser in a Live Game Client,” GW2 In-Game Helper technical report, rev. 2.2.0.0, 2026. |
