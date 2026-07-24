# :rocket: GW2 In-Game Helper — **v2.0.0.2** is here!

> A massive stability overhaul. The in-game browser now runs smoother, safer, and cleaner than ever — no dropped frames, no crashes, no half-screen popups.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`. That's it.
Needs **Raidcore Nexus**. Works on Windows + Wine/Proton. **No memory reads** — official APIs only.

---

## :sparkles: What's New

**:shield: Rock-Solid Stability (2.0.0.0)**
- **Multi-client safe** — run two GW2 clients? Each one gets its own isolated browser now (IPC v5, PID-scoped)
- **No more close hitches** — closing the helper shuts down gracefully across frames instead of hard-killing the process on the game thread
- **Zero freeze on close** — settings no longer force-write to disk mid-frame
- **Smoother navigation** — the site index warms up in the background and never blocks the render thread
- **Faster page rendering** — dirty-rect GPU uploads only redraw what changed

**:framed_picture: Cleaner Browse Experience (2.0.0.1)**
- **Fits your screen** — the Browse picker is now compact (~540×370 on 1080p) instead of eating half your desktop
- **Anchored dropdown** — Browse opens right under the button and stays put; no more stray floating window

**:zap: Final Polish (2.0.0.2)**
- **No background wake spam** — "Keep browser warm" no longer pokes the helper every single frame
- **Lighter Options** — changing your default site no longer stutters
- **Gentler startup** — smoother first-load and faster unload

---

## :books: What's Inside

The helper now includes **2,645 browse entries** across **10 categories**:

- **2,242 Wiki entries** — including **2,241 direct GW2 Wiki pages**
- **317 guides** — raids, strikes, fractals, achievements, mounts, crafting, and more
- **23 built-in cheat sheets** — available offline in the helper
- **20 build resources**
- **20 community Discords**
- **14 tools**
- Plus official GW2 pages, search providers, farming resources, and the built-in help page

That's thousands of GW2 resources in one searchable in-game picker — with favorites, tabs, pinning, page search, and external-browser fallback.

---

## :video_game: Quick Reminders

`Ctrl+Shift+H` open/close · `Ctrl+T` new tab · `Ctrl+W` close tab · `Ctrl+Tab` cycle · `Ctrl+Shift+T` reopen · `Ctrl+F` find

Wiki, Snowcrows, MetaBattle, builds, guides, cheat sheets, and community Discords — all inside the game. :heart:

*Made by Xydroc — feedback and bug reports always welcome!*
