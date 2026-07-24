# :rocket: GW2 In-Game Helper — **v2.0.0.4** is here!

> Full engine audit pass — worker-thread launch, safer quit/reopen, smoother Browse, cleaner API use.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`. That's it.
Needs **Raidcore Nexus**. Works on Windows + Wine/Proton. **No memory reads** — official APIs only.

---

## :sparkles: What's New (2.0.0.4)

- **Worker-thread launch** — CreateProcess no longer runs on the game render thread
- **Safer reopen** — closing/reopening mid-quit never hard-kills on the UI thread
- **Staging presents** — large frames copy to CPU staging, then chunked GPU upload
- **Browse caches** — Favorites, Raids, Achievements, Food, Minis, filter results
- **BootJs** — single-flight GW2 API armory queue with 429 backoff
- **Compliance docs** — hot-reload disabled on purpose; ArcDPS/ReShade dual-load notes

Prior 2.0.0.x: Legendary Armory Browse section, IPC v5, graceful quit, keep-warm fix.

---

## :video_game: Quick Reminders

`Ctrl+Shift+H` open/close · `Ctrl+T` new tab · `Ctrl+W` close tab · `Ctrl+Tab` cycle · `Ctrl+Shift+T` reopen · `Ctrl+F` find

*Made by Xydroc — feedback and bug reports always welcome!*
