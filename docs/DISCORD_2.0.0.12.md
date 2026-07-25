# :rocket: GW2 In-Game Helper — **v2.0.0.12**

> Builds → Raids lists Snow Crows builds again. YouTube playback gets one more shot.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop into `<Guild Wars 2>/addons/`. Restart GW2 after updating.

---

## What's new

- **Fix: Builds → Raids empty** — the section said (10) but showed nothing because Guides' Raid Wings / Raid Boss nesting was applied to Builds by mistake
- **YouTube attempt** — helper now uses ANGLE SwiftShader (instead of hard `--disable-gpu`) and a CEF 103 user-agent so HTML5 video can paint in the overlay. If you still get "browser can't play this video", say so and we'll remove YouTube (use **Open Ext** meanwhile)

`Ctrl+Shift+H` · *Xydroc*
