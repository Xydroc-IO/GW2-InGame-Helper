# :rocket: GW2 In-Game Helper — **v2.0.0.11**

> Browse fix: expanding Food / Minis / Armory (and similar) no longer shows a blank list.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop into `<Guild Wars 2>/addons/`. Restart GW2 after updating.

---

## What's new

- **Fix: empty Browse sections** — nested headers were handing `ImGuiListClipper` a zero row height, so the body drew nothing and didn't even grow a scrollbar. Clipper now uses a fixed row height; small lists skip clipping entirely

`Ctrl+Shift+H` · *Xydroc*
