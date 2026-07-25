# :rocket: GW2 In-Game Helper — **v2.0.0.8**

> Hotfix: window dragging works again. Click-through stays blocked.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop into `<Guild Wars 2>/addons/`. Restart GW2 after updating.

---

## What's new

- **Drag restored** — 2.0.0.7 ate `WM_MOUSEMOVE` before Nexus could update ImGui (addon WndProc runs first in the hook chain)
- **Click-through still fixed** — button-down / wheel over the overlay is fed to ImGui then blocked from the game; move/up pass through

`Ctrl+Shift+H` · *Xydroc*
