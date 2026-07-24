# Compliance & resilience notes

GW2 In-Game Helper is a **Raidcore Nexus** ImGui addon with an out-of-process CEF
browser helper. Keep these constraints when changing the code.

## Allowed

- Nexus APIs only for host integration (`RT_Render`, WndProc, keybinds, QuickAccess, paths, swap chain)
- Local IPC shared memory between the DLL and `GW2HelperBrowser.exe`
- Official `api.guildwars2.com` reads from injected BootJs (credentials omitted; batched; 429 backoff)
- `OpenProcess(PROCESS_TERMINATE)` **only** for the helper PID owned by this addon
- CEF loaded from the game’s `bin64/cef` (patch-aligned with Guild Wars 2)

## Forbidden

- Game memory reads/writes, MinHook, MumbleLink scraping for automation
- Present / `d3d11.dll` wrapper hooks (would conflict with ArcDPS / ReShade)
- `SendInput` / keybd_event into Guild Wars 2 (1-to-many input / bots)
- Combat automation or account-action automation via BootJs
- Writing into `bin64/cef` or shipping a private Chromium build as the default path

## Hot-reload

`AF_DisableHotloading` is set intentionally. The CEF helper process and IPC maps
are not safe to tear down via Nexus hot-reload — restart Guild Wars 2 after
updating the DLL.

## Dual-load testing

After engine changes, smoke-test with **ArcDPS** and/or **ReShade** loaded
alongside Nexus to confirm no Present-path conflicts (this addon does not hook
Present; it uses Nexus `SwapChain` for a dynamic texture only).

## Site registry

`src/Sites.cpp` is a large static table. Prefer contiguous categories, unique
ids, and `make validate-sites` after edits. Legendary Armory ids use `wiki_l*`
prefixes; ordinary upgrades use `wiki_relic_` / `wiki_rune_` / `wiki_sigil_`.
