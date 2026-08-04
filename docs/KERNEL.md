# Kernel playbook — WikiBrowser + CEF helper

**Audience:** anyone who must change Browse rendering, IPC, launch/extract, or navigation policy.  
**Companions:** [`ARCHITECTURE.md`](ARCHITECTURE.md), [`WHITEPAPER.md`](WHITEPAPER.md), [`NAV_AND_ADS.md`](NAV_AND_ADS.md), [`WikiIpc.h`](../src/browser/WikiIpc.h), [`CONTRIBUTING.md`](../CONTRIBUTING.md).

This is the **restricted kernel**. Pad/Sites edits do not belong here. Prefer paired review for any PR that touches these files.

**Freeze note:** Ads / Open Ext / BootJs clickability are player-revenue sensitive. Do not “clean up” ad iframes, shrink Open Ext buffers, or move ShellExecute exclusively into the helper without Proton validation.

---

## 1. Ownership map (where to edit)

### DLL host (`src/browser/`)

| File | Own this |
|------|----------|
| `WikiBrowser.cpp` | Lifecycle (`Init`/`Tick`/`Shutdown`), visibility/bounds, navigate/tabs/find, input feed, status getters |
| `WikiBrowserHelper.cpp` (+ Lifecycle / Launch) | Extract embedded EXE, IPC maps/events, `CreateProcess`, job object, quit/relaunch, Open Ext / Open Tab drains |
| `WikiBrowserIpc.cpp` | Command/input rings, URL resolve (`about:` → file), pending navigate flush, status strings |
| `WikiBrowserPresent.cpp` | D3D11 staging → DEFAULT upload, `PresentFrame`, frame SRV/UV getters |
| `WikiBrowserShared.h` | Shared `WikiBrowserDetail` state — do not redefine globals in a second TU |
| `WikiBrowser.h` | Public API only — keep stable |
| `WikiIpc.h` | Packed IPC contract (`HLI5`) |
| `CefRuntime.*` (+ Fs / Http / Verify) | Private CEF zip download / verify / extract |

Feature domains use the same Shared pattern — see [`MODULES.md`](MODULES.md), [`ARCHITECTURE.md`](ARCHITECTURE.md) §7.

### Helper process (`src/helper/`)

| File | Own this |
|------|----------|
| `main.cpp` (+ State / Tabs / Handlers / Commands) | `LoadCef`, globals, tabs/create queue, load/display/find/resource handlers, IPC drain, `wWinMain` boot |
| `HelperNavPolicy.cpp` (+ Handlers) | Ad/popup/Open Ext / in-addon tab URL policy |
| `HelperOsrRender.cpp` | OSR `OnPaint`, popup composite, `GetViewRect` / `GetScreenInfo` |
| `HelperInternal.h` | Shared `HelperDetail` state + cross-TU decls |
| `BootJs.h` / `CssCompat.*` / `CssProxy.*` | Injected JS / response filters |

**Not kernel (but compliance-adjacent):** `src/pathing/WorldGpsD3d*` — SwapChain-only world GPS. Changes need compliance review if they introduce Present hooks or memory camera reads. Playbook lives in [`PATHING.md`](PATHING.md).

---

## 2. Stamp / contract checklist (ship or break)

When behavior of the **embedded helper EXE** changes, bump **all** that apply and keep them aligned:

| Token | Where | When |
|-------|--------|------|
| `kHelperStamp` | `WikiBrowserHelper.cpp` (`ExtractHelper`) | Any helper binary behavior change (forces re-extract) |
| Addon / Nexus version | `entry.cpp` + docs | User-visible ship (**only when asked**) |
| Homepage `.ver` | `HomePage.cpp` `kHomePageVersion` | Cached HTML content change |
| Sites stamp | `SitesLoadParse.cpp` `kSitesStamp` | Catalog extract invalidation |
| Cheatsheets stamp | `CheatSheets.cpp` `kPackStamp` | Sheet pack change |
| IPC magic `HLI5` | `WikiIpc.h` `kWikiIpcMagic` | **Any** `WikiIpcState` layout change — DLL + helper must ship together |
| CEF `cef.ver` | `CefRuntime` | Runtime tree identity (`150.0.14`) |

**Rules:**
1. Never change `WikiIpcState` field order/size without bumping magic **and** rebuilding DLL + helper in one commit.
2. Never bump helper stamp without embedding a new helper (`make all` rebuilds the blob).
3. After install: prefer a **full GW2 restart**. Nexus Disable can unload the addon (`AF_None`); restart if Browse/CEF misbehaves after a DLL replace.
4. Open Ext capacity is **8192**; do not shrink without a migration plan.

Host CI checks packed IPC layout: `make test-ipc` (magic + `sizeof` + queue constants).

---

## 3. Change playbooks

### A. Add an IPC command
1. Add enum in `WikiIpc.h`.
2. DLL: post via `WikiBrowserDetail::PostCmd` (`WikiBrowserIpc.cpp`) or a public wrapper in `WikiBrowser.cpp`.
3. Helper: handle in command drain (`HelperCommands` / handlers).
4. If layout of `WikiCmdEvent` / state changes → magic bump.
5. `make test-ipc && make ci`; in-game: one navigate + one new-cmd smoke.

### B. Change present / upload
1. Edit `WikiBrowserPresent.cpp` only if possible.
2. Do not Map the ImGui-bound DEFAULT texture.
3. Watch `frame_reading` pin / dirty-rect / first-paint Map flags.
4. In-game: scroll, resize panel, first open on Windows (not only Wine).

### C. Change launch / extract
1. `WikiBrowserHelper*` — keep CreateProcess off `RT_Render` (worker + `Tick`).
2. Stamp bump if EXE bytes change.
3. Verify quick-death guard still stops crash loops.
4. Verify Job Object + host watch still reap orphans under Proton.

### D. Change nav / ads / Open Ext
1. Prefer `HelperNavPolicy.cpp` (+ Handlers).
2. Open Ext URLs must fit `open_ext_url[8192]`; DLL drains via `ShellExecute`.
3. Enable optional `navlog.on` next to the helper for traces (**never ship** the marker).
4. In-game: ad click → system browser; Discord deep link; same-site in-tab nav.
5. Read [`NAV_AND_ADS.md`](NAV_AND_ADS.md) non-regression list before merge.
6. Do not reintroduce `pointer-events: none` overlays that swallow ad clicks (see BootJs comments).

### E. Change OSR / `<select>` / popup
1. `HelperOsrRender.cpp` + BootJs polyfill coordination.
2. Mouse path must keep popup offset / swallow-after-hide behavior.
3. In-game: native-looking dropdowns, account Save forms, no helper lock loop.
4. Keep `GetScreenInfo` on real monitor geometry (not panel-sized `window.screen`).

### F. Change CssCompat / BootJs content adaptation
1. Prefer smallest filter that unblocks first paint.
2. Never strip ad/consent/analytics hosts as a “drive-by cleanup.”
3. YouTube/Twitch: prefer Watch cards / Open Ext (no proprietary codecs in stock CEF).

---

## 4. Failure modes (quick triage)

| Symptom | Likely area |
|---------|-------------|
| “Waiting for first paint…” forever | Present first Map / staging path |
| Helper crash loop on open | Launch guard; CEF extract; Wine file lock on EXE |
| Clicks do nothing on ads | Nav policy; BootJs overlays; Open Ext refuse |
| Open Ext no-op on Proton | DLL drain missing; helper-only ShellExecute |
| Torn URL / title in UI | seq odd/even protocol |
| IPC nonsense after update | magic / stamp skew — full reinstall helper+DLL |
| Black / hitchy scroll | present cadence; dirty rects; CPU upload |

---

## 5. In-game verification (minimum)

After any kernel PR:

1. `make ci` green.
2. `make install` → full restart GW2.
3. Open helper (`Ctrl+Shift+H`): homepage paints (not stuck on “Waiting for first paint…”).
4. Browse a wiki page; back/forward; new tab; find-in-page.
5. Collapse/expand or hide/show with KeepHelperWarm on/off once each.
6. One Open Ext (YouTube or Discord OAuth path if available).
7. If nav/ads touched: one ad click path with `navlog.on` locally.

---

## 6. What not to do

- Rely on loading Beta and shipping DLLs together without understanding separate signatures (`HELB` vs `HELP`) and data folders — prefer one channel when testing.
- Write CEF into `bin64/cef`.
- `Sleep` / `TerminateProcess` on the render thread for quit (use `Tick` + quit pending).
- Edit `WikiBrowserShared.h` / `HelperInternal.h` globals without checking every TU that links them.
- “Modernize” helper globals/JSON/concurrency in the name of style reviews without ads/Proton smoke — see [`WHITEPAPER.md`](WHITEPAPER.md) §18.
