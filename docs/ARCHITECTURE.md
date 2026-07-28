# Architecture — GW2 In-Game Helper

**Current addon revision:** `2.0.1.1` · **IPC:** `HLI5` (v5) · **CEF:** Guild Wars 2 `bin64/cef` (103)

This document describes how the pieces fit together. Local `CODE_AUDIT.md` *(gitignored)* holds risks and a review checklist when you keep one. For Nexus listing constraints see [`COMPLIANCE.md`](COMPLIANCE.md).

---

## 1. One-line summary

A **Raidcore Nexus** ImGui DLL opens an **out-of-process CEF off-screen (OSR)** browser that paints BGRA frames into **PID-scoped shared memory**. The DLL uploads those frames to a D3D11 texture (via Nexus `SwapChain`) and draws them with ImGui. Players ship **one DLL**; the helper EXE and homepage assets are embedded and extracted on first use. Runtime Chromium comes from the game — not WebView2, not a private Chromium build.

---

## 2. Process model

```text
Guild Wars 2.exe
 └─ Nexus loads GW2-InGame-Helper.dll
      ├─ RT_Render → UI_Render → WikiBrowser::PresentFrame / Tick
      ├─ WndProc → keyboard/mouse routing (CEF vs ImGui vs game)
      └─ CreateProcess → GW2HelperBrowser.exe
           └─ LoadLibrary(bin64/cef/libcef.dll) — windowless OSR
```

| Component | Role |
|-----------|------|
| `GW2-InGame-Helper.dll` | Nexus addon: ImGui UI, IPC host, D3D11 present, settings |
| `GW2HelperBrowser.exe` | CEF client: navigate, paint, find-in-page, BootJs |
| `bin64/cef/` | Game-owned CEF 103 runtime (read-only for us) |

### Embedding and extract

1. **Build:** helper is compiled, copied to `build/helper_blob.exe`, linked into the DLL as a binary blob (`_binary_build_helper_blob_exe_*`).
2. **Runtime:** `WikiBrowser::ExtractHelper()` writes `addons/GW2-InGame-Helper/GW2HelperBrowser.exe` plus a sibling `.ver` stamp (`kHelperStamp`, currently `"2011"`).
3. **Reuse:** skip rewrite when file size matches the blob **and** `.ver` matches the stamp (forces re-extract after helper code changes).
4. **Launch:** deferred off `RT_Render` via a worker thread (`TickLaunchPending` → `StartHelper`). Args include `--cef-dir=...` and `--host-pid=<GW2 PID>`.

### Job Object and host watch

- Helper is assigned to a Win32 **Job Object** with `KILL_ON_JOB_CLOSE` so unload/exit does not leave orphans.
- Helper opens the GW2 process with `SYNCHRONIZE` and exits when the host dies.
- `AF_DisableHotloading` is set: CEF + IPC are not safe for Nexus hot-reload. Restart GW2 after updating the DLL.

### Multi-client

IPC object names are scoped by **host GW2 PID** (`WikiIpcFormatNames` in `WikiIpc.h`):

```text
Local\GW2InGameHelper_CEF_IPC_v5_<pid>
Local\GW2InGameHelper_CEF_FRAME_v5_<pid>
Local\GW2InGameHelper_CEF_WAKE_v5_<pid>
```

Two GW2 clients get two helpers and two independent frame maps.

---

## 3. IPC (`src/WikiIpc.h`)

| Constant | Value |
|----------|--------|
| Magic | `0x484C4935` (`HLI5`) |
| Max frame | 1920×1200 BGRA, 2 flip buffers |
| Input ring | 256 events |
| Cmd ring | 32 events |
| Tabs | 8 |

### Important fields on `WikiIpcState`

- Lifecycle: `ready`, `alive`, `visible`, `can_back` / `can_forward`
- View / paint: `view_w/h`, `frame_w/h`, `frame_seq`, `frame_front`, `frame_reading` (DLL pins a buffer while copying; `0xFFFFFFFF` = idle)
- Dirty rect: `dirty_x/y/w/h`
- Fenced strings: `url` / `title` with `*_seq` (odd = writer busy)
- Coalesced mouse: `mouse_x/y`, `mouse_mods`, `mouse_leave`, `mouse_seq`
- Rings: `input_q`, `cmd_q`
- Tabs: `active_tab`, `tab_mask`
- Find: `find_count`, `find_ordinal`

### Commands

`NAVIGATE`, `BACK`, `FORWARD`, `RELOAD`, `HOME`, `QUIT`, `SET_BOUNDS`, `SET_VISIBLE`, `CREATE_TAB`, `ACTIVATE_TAB`, `CLOSE_TAB`, `FIND`, `STOP_FIND`.

Wake: auto-reset event; helper waits with `MsgWaitForMultipleObjects` (wake + host process).

---

## 4. Render / present path

```text
Nexus RT_Render
  → UI_Render()
      → WikiBrowser::Tick()              // quit finish + deferred launch
      → ImGui chrome (tabs, toolbar, Browse)
      → WikiBrowser::SetBounds / PresentFrame
      → ImGui::Image(FrameSrv()) when HasFrame()
```

### GPU upload (v2.0.0.20+)

| Texture | Usage |
|---------|--------|
| Display | `D3D11_USAGE_DEFAULT` + SRV — **never Mapped**; bound to ImGui |
| Staging | `D3D11_USAGE_STAGING` + CPU write — Map → memcpy → `CopySubresourceRegion` |

Allocated once at max OSR size (avoids recreate hitch on every window drag).

**Why staging:** Mapping a DYNAMIC texture that ImGui also samples failed forever on some native Windows setups (`DO_NOT_WAIT` → stuck on “Waiting for first paint…” while status said Ready).

### `HasFrame` / diagnostics

- `HasFrame()` — `gSrv && frame_seq > 0 && gContentW/H > 0`
- `PaintWaitReasonCStr()` — distinguishes CEF never painted (`frame_seq==0`) vs Map/upload failure
- If Ready but `frame_seq==0`, DLL periodically posts `SET_BOUNDS` (helper `NotifyWasResized`)

Present rate: ~120 Hz while wheel-scrolling, ~60 Hz while interacting, ~30 Hz idle. Idle full-frame uploads may chunk by row budget; interactive uploads do not.

---

## 5. Input path

```text
WndProc / ImGui
  ├─ CEF page focused → WikiBrowser::FeedKey / FeedMouse* → IPC → helper DrainInput → CEF
  ├─ ImGui WantTextInput (Browse filter, Search, Find) → ImGui only; game never sees keys
  └─ Outside overlay → game input
```

| Flag | When |
|------|------|
| `gBlockGameKeyboard` | Overlay open **and** (CEF focused **or** `io.WantTextInput`) |
| `gBlockGameMouse` | Pointer over wiki window / page (or collapsed title bar) |

Key-up is paired with the same sink that ate key-down (`sAteKeyDest`) so focus flips do not leave GW2 with a stuck key (e.g. autorun `R`).

Toggle hotkey: **Ctrl+Shift+H** (or **K**). The chord is swallowed so it does not leak to the game or CEF.

---

## 6. CEF helper

### Init highlights (`src/helper/main.cpp`)

- Windowless rendering; `shared_texture_enabled = 0`; frame rate 60
- `multi_threaded_message_loop = 0` (input must run on the CEF UI thread)
- Software path: `--disable-gpu`, `--disable-gpu-compositing`, `--disable-d3d11`, …
- Cache only under `%TEMP%\GW2-InGame-Helper-cef` (never under `addons/` or `bin64/cef`)
- User-Agent spoofed as Chrome/120 (engine remains CEF 103)

### BootJs (`src/helper/BootJs.h`)

Injected on main-frame `OnLoadEnd`:

- CSS downlevel for modern hosts (MetaBattle, gw2efficiency, Guildjen, gw2.app, Google/DDG)
- **Ads allowed** — `killAds()` is a no-op
- YouTube embeds → “Watch on YouTube” cards (Open Ext / system browser)
- Login tips for Google and gw2.app → use **Open Ext** (sessions do not share with CEF)

### Network policy

| Concern | Behavior |
|---------|----------|
| Ads / analytics / NitroPay | **Allowed** (`ShouldBlockUrl` always false) |
| YouTube / googlevideo as subframe on guide pages | **Cancelled** (prevents guide “refresh”) |
| Guildjen HTML | Response filter rewrites YouTube iframes before paint |
| WinHTTP CSS proxy | Disabled (Wine/Proton fragility) |

---

## 7. Browse / sites

Browse entries are **labeled hyperlinks**: `SiteDef { id, category, label, title, homeUrl, search* }` in `src/Sites.cpp`. Clicking one navigates the CEF tab.

- Categories must stay **contiguous** in the table.
- Subsection headers live in `UI.cpp` (`BrowseSection` / `BrowseSectionsForCategory`).
- After edits: `make validate-sites` (`tools/validate_sites.py`).
- Built-in pages use `about:…` URLs resolved to `file:///` under the addon data dir (homepage, cheat sheets).

**GW2.app** (since v2.0.0.21 / release **2.0.1.0**): deep links under **Browse → Tools → GW2.app** (not a separate top-level category).

**Snow Crows:** removed at their request (v2.0.0.20); MetaBattle / Guildjen / Accessibility Wars cover builds and raid guides.

---

## 8. Settings, tabs, stamps

| Item | Location |
|------|----------|
| Settings | `addons/GW2-InGame-Helper/settings.ini` (debounced save) |
| Tabs | Up to 8; pin; closed-tab stack; URL/title from IPC |
| Addon version | `src/entry.cpp` → `G::AddonDef.Version` (`2.0.1.1`) |
| Helper extract stamp | `WikiBrowser.cpp` → `kHelperStamp` (`"2011"`) |
| Homepage cache stamp | `HomePage.cpp` → `kHomePageVersion` (`"2011"`) |

When shipping a release, bump **Version** (Major/Minor/Build/Revision), helper stamp, homepage stamp, and docs together (see `.cursor/rules/no-version-bump.mdc` — only when explicitly asked).

Options: show window, default landing site, opacity, font scale, **Keep browser warm when closed**.

---

## 9. Player install layout

```text
<GW2>/addons/GW2-InGame-Helper.dll          # only file players copy
<GW2>/addons/GW2-InGame-Helper/             # extracted at runtime
    GW2HelperBrowser.exe
    GW2HelperBrowser.exe.ver
    helper-home.html / .ver + logo/cover
    settings.ini
    cheat-sheet *.html / .ver
```

CEF libraries stay in `<GW2>/bin64/cef/`.

---

## 10. Related docs

| Doc | Purpose |
|-----|---------|
| [`DOCUMENTATION.md`](DOCUMENTATION.md) | Index of all project docs |
| `CODE_AUDIT.md` *(gitignored)* | Local risks / regression checklist |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Allowed / forbidden host patterns |
| [`BUILD.md`](BUILD.md) | Cross-compile and install |
| [`CATALOG.md`](CATALOG.md) | Browse catalog outline |
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Version history |
| `RAIDCORE.md` *(gitignored)* | Local Nexus listing draft |
| `DISCORD.md` *(gitignored)* | Local Discord announcement draft |
