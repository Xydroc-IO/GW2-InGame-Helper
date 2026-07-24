# :rocket: GW2 In-Game Helper — **v2.0.0.3** is here!

> Smoother first open, lighter Browse, and a full Legendary Armory section — still zero memory reads.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`. That's it.
Needs **Raidcore Nexus**. Works on Windows + Wine/Proton. **No memory reads** — official APIs only.

---

## :sparkles: What's New

**:zap: Engine Audit (2.0.0.3)**
- **No launch hitch** — helper `CreateProcess` runs on the next tick, not mid-frame
- **Smoother paints** — large full-frame GPU uploads split across frames
- **Faster Browse** — filter results and Food / Minis lists are cached
- **Kinder API use** — Snow Crows armory fetches are serialized with 429 backoff
- **Legendary Armory** — armor, weapons (by gen), accessories, amulet, rings, back items, legendary upgrades under one Wiki section

**:shield: Rock-Solid Stability (2.0.0.0–2.0.0.2)**
- Multi-client safe IPC · graceful helper quit · no settings freeze on close · keep-warm no longer wakes CEF every frame

---

## :video_game: Quick Reminders

`Ctrl+Shift+H` open/close · `Ctrl+T` new tab · `Ctrl+W` close tab · `Ctrl+Tab` cycle · `Ctrl+Shift+T` reopen · `Ctrl+F` find

Wiki, Snowcrows, MetaBattle, builds, guides, cheat sheets, and community Discords — all inside the game. :heart:

*Made by Xydroc — feedback and bug reports always welcome!*
