# Compliance & resilience notes

GW2 In-Game Helper is a **Raidcore Nexus** ImGui addon with an
out-of-process CEF browser helper using a **private CEF Stable 150** runtime.

Current policy snapshot: **v2.0.2.2** — see [`ARCHITECTURE.md`](ARCHITECTURE.md).

## Allowed

- Nexus APIs only for host integration (`RT_Render`, WndProc, keybinds, QuickAccess, paths, swap chain)
- Local IPC shared memory between the DLL and `GW2HelperBrowser.exe`
- Official `api.guildwars2.com` reads from injected BootJs (credentials omitted; batched; 429 backoff) where pages use them
- `OpenProcess(PROCESS_TERMINATE)` **only** for the helper PID owned by this addon
- **Private CEF 150** under `addons/GW2-InGame-Helper/cef/` (first-run download + SHA-256 verify)
- Site ads / consent / analytics loads in CEF (do not re-strip without review)
- Deep links to third-party sites as Browse hyperlinks

## Forbidden

- Game memory reads/writes, MinHook, MumbleLink scraping for automation
- Present / `d3d11.dll` wrapper hooks (would conflict with ArcDPS / ReShade)
- `SendInput` / keybd_event into Guild Wars 2 (1-to-many input / bots)
- Combat automation or account-action automation via BootJs
- **Writing into `bin64/cef`** (private tree stays under the addon data folder)
- Re-adding Snow Crows catalog links without an explicit product decision

## Hot-reload

`AF_DisableHotloading` is set intentionally. Restart Guild Wars 2 after updating the DLL.

## Dual-load testing

Smoke-test with **ArcDPS** and/or **ReShade** alongside Nexus after engine changes.

## Sign-in / OAuth

Embedded CEF often cannot complete Google / Discord / site OAuth. Prefer **Open Ext**.
Newer CEF helps modern CSS/JS; it does not magically fix Discord OAuth or OSR ad viewability.

## Windows Defender

Unsigned MinGW builds may be ML-flagged. A private Chromium tree under `addons/`
may draw extra AV scrutiny — keep the runtime out of the DLL blob and verify SHA-256.
