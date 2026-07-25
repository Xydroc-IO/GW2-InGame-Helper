# :rocket: GW2 In-Game Helper — **v2.0.0.15**

> Typing in Browse / Search / Find no longer leaks keys to the game (no more stuck autorun from `R`).

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop into `<Guild Wars 2>/addons/`. Restart GW2 after updating.

---

## What's new

- **Fix:** Keys typed into ImGui fields (Browse filter, toolbar Search, Find) were only blocked when the web page had focus — typing `R` toggled GW2 autorun and left the character running
- Keyboard capture now follows `WantTextInput` as well as the CEF page, and key-up stays paired with the same sink

`Ctrl+Shift+H` · *Xydroc*
