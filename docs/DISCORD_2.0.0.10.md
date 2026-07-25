# :rocket: GW2 In-Game Helper — **v2.0.0.10**

> Stability fix: the game no longer freezes when you close it out.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop into `<Guild Wars 2>/addons/`. Restart GW2 after updating.

---

## What's new

- **Fix: freeze on exit** — the addon was unloading while its browser-launch worker thread was still running, so Guild Wars 2 stalled on shutdown. Unload now waits for that thread before releasing anything
- **Fix: orphaned helper** — `GW2HelperBrowser.exe` now exits together with GW2. Previously a hard crash could leave it running and block the next launch

Nothing to reconfigure — favorites, tabs, and window position carry over.

`Ctrl+Shift+H` · *Xydroc*
