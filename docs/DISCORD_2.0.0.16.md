# :rocket: GW2 In-Game Helper — **v2.0.0.16**

> Embedded YouTube on guide pages no longer refreshes the whole tab when you hit Play.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop into `<Guild Wars 2>/addons/`. Restart GW2 after updating.

---

## What's new

- **Fix:** Play on Guildjen (and similar) embeds was promoting `youtube.com/watch` / googlevideo / accounts into the main frame after a few seconds of playback — that looked like a refresh
- Main-frame CDN / YouTube navigations stay cancelled while you’re on a normal site; embeds prefer youtube-nocookie + playsinline
- Playback in CEF is still best-effort — use **Open Ext** if the player stalls. The guide should stay put either way

`Ctrl+Shift+H` · *Xydroc*
