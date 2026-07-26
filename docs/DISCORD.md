# :rocket: GW2 In-Game Helper — **v2.0.0.19**

> Wiki, builds, guides, cheat sheets, tools, and community Discords — **inside Guild Wars 2**. One DLL. No memory reads.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll)
Drop `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`. Restart GW2 after updating.

Needs **[Raidcore Nexus](https://raidcore.gg/gw2/nexus)**. Works on **Windows** and **Wine / Proton**. Official Nexus APIs only.

**Repo:** https://github.com/Xydroc-IO/GW2-InGame-Helper

---

## :books: What’s inside

**2,674** Browse entries you can search and open in-game:

- **Wiki** — thousands of GW2 Wiki pages (legendaries, armory, food, minis, upgrades, lifestyle, crafting, and more)
- **Guides** — Guildjen, Snow Crows, MetaBattle, TLDR, raids / strikes / fractals / mounts / achievements
- **Builds** — Snow Crows, MetaBattle, AccessiBuilds, Gw2Skills, Accessibility Wars
- **Cheat Sheets** — 23 offline sheets (raid food, utilities, boons, wings, dailies, and more)
- **Tools** — gw2efficiency, timers, Wingman, KillProof, BLTC, and more
- **Search** — Google, DuckDuckGo, Gemini
- **Discord** — community / training / farming invites (use **Open Ext** to join)
- **Official** — gw2.com, forums, Raidcore

Favorites, up to **8 tabs**, pin, find-in-page, and a GW2-themed chrome.

---

## :video_game: How to use

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` (or `K`) or QuickAccess |
| New tab / Browse | `Ctrl+T` or **+** / **Browse** |
| Close tab | `Ctrl+W` · **x** · middle-click |
| Cycle tabs | `Ctrl+Tab` / `Ctrl+Shift+Tab` |
| Reopen closed | `Ctrl+Shift+T` |
| Find in page | `Ctrl+F` |
| Return skills to game | Click outside the helper |

1. Open the helper — starts on the how-to **Home** page
2. Click **Browse** — search or pick a category, then a site
3. Click the page to interact; drag the title bar to move
4. Use **Open Ext** for Discord invites / Google login / anything that needs your system browser
5. YouTube on guides shows a **Watch on YouTube** card (opens your system browser — in-page play refreshes the tab)

---

## :sparkles: Recent highlights

- **v2.0.0.19** — Fix Windows “Waiting for first paint…” stall; Defender `Wacatac.B!ml` is a false positive on this unsigned build
- **v2.0.0.18** — Guildjen YouTube iframes rewritten to Watch cards + embed loads blocked (no play→refresh)
- **v2.0.0.17** — YouTube embeds → system-browser cards
- **v2.0.0.15** — Typing in Browse / Search / Find no longer leaks keys to GW2 (no more stuck autorun from `R`)
- **v2.0.0.14** — Removed the YouTube Browse site (CEF 103 OSR can’t play it reliably)
- **v2.0.0.11–12** — Browse section fixes (blank lists / Builds → Raids empty)
- **v2.0.0.10** — No more freeze on exit; helper exits with GW2
- **v2.0.0.7–8** — Click-through + window drag fixed

---

## :wrench: Install notes

- Copy **only** the DLL into `addons/` — helper + homepage assets extract on first use
- Runtime folder: `addons/GW2-InGame-Helper/`
- Keep browser warm / collapse the title bar if you want CEF to stay alive between opens
- Google / Gemini login often needs **Open Ext** (“This browser may not be secure”)
- After updating, **fully restart GW2** so the helper exe re-extracts

---

## :shield: Windows Defender (`Wacatac.B!ml`)

Defender may ML-flag the DLL as `Trojan:Win32/Wacatac.B!ml`. That is a **false positive** common on unsigned MinGW builds — the addon is open source and does not contain malware.

- **Players:** Windows Security → Protection history → Allow / restore the file (or exclude the GW2 `addons` folder)
- **Devs:** Submit the release DLL at https://www.microsoft.com/en-us/wdsi/filesubmission → Software developer → incorrectly detected

---

`Ctrl+Shift+H` · feedback welcome · *Xydroc*
