# Code audit — GW2 In-Game Helper

**Audit revision:** 2.0.0.21  
**Date:** 2026-07-27  
**Scope:** Full tree under `src/`, helper, IPC, present/input paths, Sites/Browse, compliance surface  
**Method:** Source review (not a runtime pen-test). Re-run after large CEF / present / input changes.

Companion: [`ARCHITECTURE.md`](ARCHITECTURE.md) · [`COMPLIANCE.md`](COMPLIANCE.md)

---

## 1. Executive summary

| Area | Verdict |
|------|---------|
| Host integration | Healthy — Nexus APIs only; no Present hook; no game memory |
| Process isolation | Helper out-of-process + Job Object + host death watch |
| Present path | Staging → DEFAULT upload; first-paint diagnostics; Windows stall mitigated |
| Input | Game keys blocked for CEF focus **and** ImGui `WantTextInput` |
| Ads | Intentionally allowed site-wide |
| YouTube | Forced off-page (cards + subframe cancel) — CEF 103 OSR cannot play reliably |
| Catalog | Snow Crows removed; MetaBattle / Guildjen / Accessibility Wars / GW2.app (under Tools) |
| Residual risk | Unsigned MinGW Defender FP; `no-sandbox`; OAuth in CEF; RT_Render first-open cost |

**Overall:** Fit for Nexus distribution as an overlay browser. Do not reintroduce Present hooks, game memory, or in-CEF YouTube playback without a new audit.

---

## 2. Critical / high findings

### 2.1 Resolved (keep regressions out)

| ID | Issue | Fix location |
|----|--------|----------------|
| P1 | Native Windows stuck on “Waiting for first paint…” + Ready | Staging texture + blocking first Map; `was_resized` / SET_BOUNDS kick (`WikiBrowser.cpp`, helper load handler) |
| P2 | Typing in Browse/Search/Find leaked to GW2 (autorun `R`) | `gBlockGameKeyboard` includes `WantTextInput`; paired key-up sink (`UI.cpp`, `entry.cpp`) |
| P3 | YouTube Play refreshed Guildjen guides | HTML rewrite + BootJs cards + subframe cancel (`CssCompat`, `BootJs`, `OnBeforeResourceLoad`) |
| P4 | Ad-strip deleted Snow Crows article bodies (`nitro-article-*`) | Ads path disabled; Snow Crows links removed at their request |
| P5 | Exit / unload freeze | Join launch worker before teardown; Job Object; helper watches host PID |

### 2.2 Open / accepted risks

| ID | Severity | Issue | Mitigation / note |
|----|----------|--------|-------------------|
| R1 | Med | Windows Defender `Wacatac.B!ml` on unsigned MinGW static DLL | Documented FAQ; allowlist / WDSI submit; long-term code signing |
| R2 | Med | CEF started with `no-sandbox` | Required for this layout; helper is still a separate process |
| R3 | Med | Google / Discord / gw2.app OAuth often fails in embedded CEF | Open Ext tips; sessions diverge from in-game tabs — by design |
| R4 | Low | Ads / trackers load in CEF profile under `%TEMP%` | Intentional (2.0.0.20); cookie banners left alone |
| R5 | Low | Named IPC objects are same-user discoverable | PID-scoped names; not a secrecy boundary |
| R6 | Low | First open / extract / CreateProcess still costs frames | Deferred to worker; no `memset` of ~18MB frame map; chunked idle upload |
| R7 | Low | `Sites.cpp` huge static table | `validate_sites`; prefer contiguous categories; JSON codegen still aspirational |
| R8 | Info | CMake `project()` version ≠ addon Revision | Makefile shipping path is canonical for releases |

---

## 3. Component audit

### 3.1 `entry.cpp` — Nexus surface

- Registers `GetAddonDef` (name, author, description, **2.0.0.21**, signature `HELP`).
- `AF_DisableHotloading` set.
- WndProc: routes keys to CEF or ImGui; blocks game input when UI flags say so; swallows toggle chord.
- Unload: request helper stop, join launch thread, shutdown WikiBrowser / settings.

**Audit notes:** Keep TerminateProcess scoped to helper PID only. Do not add MinHook / Present hooks here.

### 3.2 `WikiBrowser.cpp` — host browser control

- Extract / stamp / launch / Job Object.
- IPC create/map; cmd + input posting; PresentFrame staging path.
- `PaintWaitReasonCStr` for stuck-panel reports.

**Audit notes:** Never Map the ImGui-bound DEFAULT texture. Keep first-paint Map blocking. Do not `CreateProcess` on the ImGui mid-frame path (already deferred).

### 3.3 `UI.cpp` — ImGui chrome

- Browse tree, favorites, tabs chrome, OSR `Image` + hit testing.
- Section maps for Wiki / Guides / Builds / Tools (**GW2.app** subsection) / Discord / etc.

**Audit notes:** ListClipper under nested headers previously blanked sections — preserve row-height guards. Tools → GW2.app uses `strncmp(id, "gw2app", 6)`.

### 3.4 Helper `main.cpp` + BootJs / Css*

- OSR paint into shared flip buffers; dirty rects; `frame_reading` respect.
- Resource request: ads allowed; YouTube subframes cancelled on non-YT mains.
- BootJs: CSS fix hosts include `gw2.app`; login tips; YouTube cards; `killAds` no-op.

**Audit notes:** Do not re-enable broad DOM ad stripping without host allowlists (historical Snow Crows wipe). Do not enable WinHTTP CSS proxy on Wine without a new Wine regression pass.

### 3.5 `Sites.cpp` / Browse

- Static labeled hyperlinks only — no site APIs.
- Snow Crows IDs removed; MetaBattle raid builds + wing guides; Tools includes GW2.app deep links.
- `make validate-sites` → unique ids, contiguous categories, UI id refs.

**Audit notes:** Favorites pinned to deleted `sc_*` / `snowcrows` IDs go stale — expected.

### 3.6 Settings / tabs / homepage

- Debounced `settings.ini`; tab restore; homepage stamp `221`; helper stamp `21`.
- Cheat sheets / raid food written as versioned HTML under addon dir.

---

## 4. Threat model (brief)

| Threat | Handling |
|--------|----------|
| Malicious page in CEF | Separate process; Job Object; no game memory; limited to browsing / Open Ext |
| Addon spoof / tamper | Unsigned today — Defender FP noise; signing would help trust |
| Helper left running | Job + host SYNCHRONIZE + QUIT path |
| Input injection into GW2 | We block overlay keys; we never `SendInput` into the game |
| Credential phishing on fake sites | Same as any browser; Open Ext for real OAuth when CEF fails |

Out of scope: protecting users from ads they choose to view, or making NitroPay revenue-guarantee impressions.

---

## 5. Regression test checklist

### Build / ship

- [ ] `make -j$(nproc)` → single DLL with embedded helper
- [ ] `make validate-sites` OK after Sites/UI Browse edits
- [ ] Stamps: Revision **21**, helper `.ver` **21**, homepage **221**
- [ ] Docs version strings agree (`README`, `RELEASE_NOTES`, `RAIDCORE`, `DISCORD`, `description.html`)

### Process / IPC

- [ ] First open extracts under `addons/GW2-InGame-Helper/` only
- [ ] Two GW2 clients → two helpers, no shared frame clash
- [ ] Kill GW2 → helper exits
- [ ] Keep warm on/off reopen behavior
- [ ] Exit/unload without freeze

### Present / input

- [ ] Native Windows: paints past Ready (not black forever)
- [ ] If stuck, muted `PaintWaitReason` is actionable
- [ ] Wine/Proton: scroll fluid; no permanent first-paint wait
- [ ] Browse filter / Search / Find: no GW2 autorun from `R`
- [ ] Click page → CEF keys; click outside → game keys
- [ ] Ctrl+Shift+H toggles; chord does not leak

### Content

- [ ] Tools → **GW2.app** subsection opens hub/lists/items/…
- [ ] MetaBattle: ads/consent can show
- [ ] Guildjen: YouTube is a card; Play does not refresh the guide
- [ ] No Browse entries pointing at `snowcrows.com`
- [ ] Homepage / cheat sheets load after stamp bump
- [ ] ArcDPS and/or ReShade dual-load smoke (no Present conflict)

### Security smoke

- [ ] No writes under `bin64/cef`
- [ ] CEF cache under `%TEMP%\GW2-InGame-Helper-cef` only
- [ ] Defender allowlist note still accurate if FP fires

---

## 6. Source map (quick)

| Path | Responsibility |
|------|----------------|
| `src/entry.cpp` | Nexus load/unload, WndProc, version |
| `src/UI.cpp` | ImGui overlay, Browse, input block flags |
| `src/WikiBrowser.cpp` | Helper lifecycle, IPC host, D3D present |
| `src/WikiIpc.h` | Shared protocol |
| `src/Sites.cpp` | Site registry |
| `src/BrowserTabs.cpp` | Tab model + settings keys |
| `src/Settings.cpp` | `settings.ini` |
| `src/HomePage.cpp` | How-to page + stamp |
| `src/helper/main.cpp` | CEF OSR process |
| `src/helper/BootJs.h` | Injected page scripts |
| `src/helper/CssProxy.cpp` | URL block (off) + response filter gate |
| `src/helper/CssCompat.cpp` | CSS/HTML rewrites |
| `tools/validate_sites.py` | Registry integrity |

---

## 7. Audit history

| Date | Rev | Notes |
|------|-----|--------|
| 2026-07-27 | 2.0.0.21 | Full written audit: staging present, ads on, SC removed, GW2.app under Tools |
| (prior) | 2.0.0.x | Informal RT_Render / YouTube / input audits in chat; not previously filed as `CODE_AUDIT.md` |

Update this file when shipping material changes to present, input, CEF network policy, or compliance boundaries.
