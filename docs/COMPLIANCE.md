# Compliance & resilience notes

GW2 In-Game Helper is a **Raidcore Nexus** ImGui addon with an
out-of-process CEF browser helper using a **private CEF Stable 150** runtime.

Current policy snapshot: **v2.0.2.8** — process/IPC notes live in local `ARCHITECTURE.md` (gitignored) if you keep one; public overview is [`../README.md`](../README.md) + [`CEF_RUNTIME.md`](CEF_RUNTIME.md).

## Allowed

- Nexus APIs only for host integration (`RT_Render`, WndProc, keybinds, QuickAccess, paths, swap chain)
- **Read-only MumbleLink / Nexus DataLink** for display overlays (Tekkit compass trails + in-world GPS) — never for automation
- Local IPC shared memory between the DLL and `GW2HelperBrowser.exe`
- Official `api.guildwars2.com` reads from injected BootJs (credentials omitted; batched; 429 backoff) where pages use them
- DLL WinHTTP reads to `api.guildwars2.com`, `guildwars2.com` news feed, and wiki MediaWiki API for **Live** Browse panels and ImGui pads (read-only; optional account API key stored only in local `settings.ini`)
- Local Notes pad (`notes.json` under the addon folder) with clipboard copy helpers — no game injection
- Item Lookup pad (public `/v2/items` + wiki search), Wallet & Stash pad (`/v2/account/wallet`, materials, bank, shared inventory, character inventories), and Vault pad (Wizard’s Vault / dailies — same Live panel API) — read-only; item name cache in `stash-names.cache`
- Tekkit’s All-In-One `.taco` pathing packs (© Tekkit's Workshop, used with permission) loaded locally for display
- `OpenProcess(PROCESS_TERMINATE)` **only** for the helper PID owned by this addon
- **Private CEF 150** under `addons/GW2-InGame-Helper/cef/` (first-run download + SHA-256 verify)
- Site ads / consent / analytics loads in CEF (do not re-strip without review)
- Deep links to third-party sites as Browse hyperlinks

## Forbidden

- Game memory reads/writes, MinHook, MumbleLink **scraping for automation** (combat/bots/input)
- Present / `d3d11.dll` wrapper hooks (would conflict with ArcDPS / ReShade)
- `SendInput` / keybd_event into Guild Wars 2 (1-to-many input / bots)
- Combat automation or account-action automation via BootJs
- **Writing into `bin64/cef`** (private tree stays under the addon data folder)

## Hot-reload

`AF_DisableHotloading` is set intentionally. Restart Guild Wars 2 after updating the DLL.

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
