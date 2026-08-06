# Compliance & resilience notes

GW2 In-Game Helper is a **Raidcore Nexus** ImGui addon with an
out-of-process CEF browser helper using a **private CEF Stable 150** runtime.

Current policy snapshot: **v2.2.3.2** — process/IPC notes: [`ARCHITECTURE.md`](ARCHITECTURE.md); design rationale: [`WHITEPAPER.md`](WHITEPAPER.md); nav/ads ops: [`NAV_AND_ADS.md`](NAV_AND_ADS.md); pathing: [`PATHING.md`](PATHING.md); public overview [`../README.md`](../README.md) + [`CEF_RUNTIME.md`](CEF_RUNTIME.md).

This file is **normative** (allowed / forbidden). Analysis belongs in the whitepaper.

## Allowed

- Nexus APIs only for host integration (`RT_Render`, WndProc, keybinds, QuickAccess, paths, swap chain)
- **Read-only MumbleLink / Nexus DataLink** for display overlays (Pathing compass trails + in-world GPS + direction compass + Completion proximity auto-tick + GPS arrow / zone banner) — never for automation or movement bots
- **In-world GPS** draws Blish-style ribbons via the Nexus **`SwapChain`** D3D11 device (runtime HLSL compile). No Present hooks, no game depth buffer / camera-matrix reads from process memory
- Local IPC shared memory between the DLL and `GW2HelperBrowser.exe` (current-user DACL on named maps/events when ACL APIs succeed)
- Official `api.guildwars2.com` reads from injected BootJs (credentials omitted; batched; 429 backoff) where pages use them
- DLL WinHTTP reads to `api.guildwars2.com`, `guildwars2.com` news feed, and wiki MediaWiki API for **Live** Browse panels and ImGui pads (read-only; optional account API key stored only in local `settings.ini`)
- DLL WinHTTP reads to **killproof.me** (`/api/kp/…`) for the DPS Logs **KillProof** tab — public profiles only; no killproof.me login; results cached in-memory
- Local Notes pad (`notes.json` under the addon folder) with clipboard copy helpers — no game injection
- **Completion** / **Farming** companion pads — local checklist / favorites / fishing log under `config/`; Pathing search-guide handoff only (no memory scraping)
- **PanelBinds** — addon-owned panel chords in Settings → Keybinds (`GetAsyncKeyState` poll); helper open stays Nexus (`Ctrl+Shift+H` / QuickAccess)
- Item Lookup pad (public `/v2/items` + wiki search), Wallet & Stash pad (`/v2/account/wallet`, materials, bank, shared inventory, character inventories), and Vault pad (Wizard’s Vault / dailies — same Live panel API) — read-only; item name cache in `stash-names.cache`
- Tekkit’s All-In-One `.taco` (© Tekkit's Workshop, used with permission), Lady Elyssa Guides / Achievements packs, and Hero's Marker Pack (QuitarHero) — curated downloads into `pathing/` for local display; user `.taco` files kept
- Opt-in PathingLua (`EnablePathingLua`, default off) for Blish-shaped pack `script-*` — display/scripting only; never game input or memory automation
- **Embedded curated GW2 UI chrome** (`data/ui-chrome` → DLL zip → `ui-chrome/` under the addon data folder) for Immersive ImGui pads and HTML backgrounds — ArenaNet retains ownership of those textures; they are **not** relicensed under MIT
- `OpenProcess(PROCESS_TERMINATE)` **only** for the helper PID owned by this addon
- **Private CEF 150** under `addons/GW2-InGame-Helper/cef/` (first-run download + SHA-256 verify)
- Optional **Elite Insights CLI** under `addons/GW2-InGame-Helper/ei/` (on-demand download of upstream `GW2EICLI.zip` + SHA-256 verify; MIT, baaron4; requires user-installed .NET 8)
- Site ads / consent / analytics loads in CEF (do not re-strip without review) — see [`NAV_AND_ADS.md`](NAV_AND_ADS.md)
- Deep links to third-party sites as Browse hyperlinks
- Stable User-Agent product token `GW2-InGame-Helper` so publishers can allow/deny
  this client (instructions: [`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md))

## Forbidden

- Game memory reads/writes, MinHook, MumbleLink **scraping for automation** (combat/bots/input)
- Present / `d3d11.dll` wrapper hooks (would conflict with ArcDPS / ReShade)
- `SendInput` / keybd_event into Guild Wars 2 (1-to-many input / bots)
- Combat automation or account-action automation via BootJs
- **Writing into `bin64/cef`** (private tree stays under the addon data folder)
- Truncating advertisement click tracker URLs in Open Ext (refuse if too long instead)

## Hot-reload

Prefer a full Guild Wars 2 restart after updating the DLL. Nexus may Disable/unload
the addon (`AF_None`); unload stops the CEF helper first.

## Dual-load testing

Smoke-test with **ArcDPS** and/or **ReShade** alongside Nexus after engine changes.

## Sign-in / OAuth

Embedded CEF often cannot complete Google / Discord / site OAuth. Prefer **Open Ext**.
Newer CEF helps modern CSS/JS; it does not magically fix Discord OAuth or OSR ad viewability.

## Video codecs

Official CEF binaries (including the Spotify CDN builds `pack-cef-runtime.sh` uses) are built
without proprietary codecs, so **H.264 / AAC playback is unavailable**. Twitch reports this as
`Error #4000`; MP4 sources fail the same way. YouTube and Twitch are routed to **Open Ext** with
a Watch card instead.

Enabling them requires building Chromium from source with
`proprietary_codecs=true ffmpeg_branding=Chrome` **and** securing codec licensing before
redistribution — treat that as a product/legal decision, not a build flag.

## Windows Defender

Unsigned MinGW builds may be ML-flagged. A private Chromium tree under `addons/`
may draw extra AV scrutiny — keep the runtime out of the DLL blob and verify SHA-256.
