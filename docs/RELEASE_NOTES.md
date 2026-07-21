# GW2 In-Game Helper v1.5.2.0

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

```text
addons/GW2-InGame-Helper.dll          ← install this (only file you place)
addons/GW2-InGame-Helper/             ← created at runtime (helper, pages, settings)
```

Do **not** put the DLL inside `addons/GW2-InGame-Helper/`. That folder is for runtime data only.

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` (or `K`) · QuickAccess icon |
| Rebind | Nexus `Ctrl+O` → `KB_HELPER_TOGGLE` |
| Browse sites | Toolbar **Browse** (search + categories + Favorites) |
| Favorite site | ☆ / ★ on toolbar or next to Browse rows |
| Copy current URL | Toolbar **Copy URL** |
| Open in system browser | Toolbar **Open Ext** |

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.5.2.0

- **Favorites** — star sites you use most (☆ add / ★ remove)
- **Favorites** category at the top of Browse
- Toolbar star toggles the current site
- Persisted as `FavoriteIds=` in `addons/GW2-InGame-Helper/settings.ini`

---

## What’s new in 1.5.1.0

- **Install layout** — runtime files (helper exe, homepage HTML/assets, Raid Food page, `settings.ini`) live under `addons/GW2-InGame-Helper/`
- **DLL only in `addons/`** — players still place a single DLL; nothing else ships beside it
- **Stale cleanup** — older builds that left helper/HTML/settings next to the DLL in `addons/` are cleaned up on load

---

## Features (current)

- In-game CEF browser using the game’s `bin64/cef` (no separate Chromium download)
- **Browse** panel — search and pick sites by category
- **Favorites** — pin frequently used sites
- Branded **How to use** homepage (logo + cover art)
- Built-in **Raid Food** seasoning / feast guide
- Toolbar: Home · Back · Forward · Reload · Browse · ★ · Copy URL · Open Ext
- Nexus QuickAccess icon
- Auto-update via GitHub Releases (`UP_GitHub`)
- No Guild Wars 2 memory reads — Nexus APIs only
- Windows and Linux (Wine / Proton)

---

## What’s new since 1.4.0 (summary)

| Version | Highlights |
|---------|------------|
| **1.5.2.0** | Favorites (star sites; persisted) |
| **1.5.1.0** | Runtime data under `addons/GW2-InGame-Helper/`; DLL only in `addons/` |
| **1.4.1.8** | Full-page themed homepage redesign |
| **1.4.1.7** | Removed Lucky Noobs (site + Discord) |
| **1.4.1.6** | Browse panel (search + categories) |
| **1.4.1.5** | Gap-fill sites; **Copy URL** / **Open Ext** |
| **1.4.1.4** | Expanded sites & Discords (no Hardstuck / Discretize) |
| **1.4.1.3** | Branded homepage refresh |
| **1.4.1.2** | Built-in Raid Food guide |
| **1.4.1.1** | Gw2Skills, Meta Timers, Peu, Mukluk Fractals, GW2 TLDR |
| **1.4.1** | Music Box fixed to HTTP (`http://gw2mb.com/`) |
| **1.4.0** | Community sites + Discord category |

Hardstuck and Discretize remain intentionally omitted (outdated).

---

## Changelog detail

### 1.5.2.0

- Add / remove favorites from the toolbar star or Browse row stars
- Virtual **Favorites** category at the top of Browse
- Persist favorite site ids in settings (`FavoriteIds`)

### 1.5.1.0

- Extract helper, homepage assets, and settings into Nexus addon data directory `addons/GW2-InGame-Helper/`
- `make install` places only the DLL in `addons/` and creates the data folder
- Remove leftover helper/HTML/settings from `addons/` root left by older installs

### 1.4.1.8

- Full-page GW2-themed homepage (logo, cover, stronger visual hierarchy)

### 1.4.1.7

- Removed Lucky Noobs from sites and Discord list

### 1.4.1.6

- Replaced site dropdown with **Browse** panel (search + category filters)

### 1.4.1.5

- Additional community / guide / Discord links to close content gaps
- **Copy URL** and **Open Ext** toolbar actions (useful for Discord joins and logins)

### 1.4.1.4

- Large site and Discord expansion (Official, Wiki, Builds, PvP, WvW, Tools, Guides, Farming, Discord)
- Hardstuck / Discretize excluded

### 1.4.1.3

- Homepage branding refresh (embedded logo / cover)

### 1.4.1.2

- Built-in **Raid Food** page (`about:raid-food`)

### 1.4.1.1

- Builds: [Gw2Skills Editor](https://en.gw2skills.net/editor/)
- Tools: [Meta Timers](https://gw2tldr.com/metas), [Peu Research Center](https://peuresearchcenter.com/index.html)
- Guides: [Mukluk Fractals](https://mukluklabs.com/gw2-fractal-guides), [GW2 TLDR](https://gw2tldr.com/)

### 1.4.1

- **Fix:** Music Box opens over HTTP (`http://gw2mb.com/`). HTTPS uses an invalid certificate and returns 403.

### 1.4.0

- Official: [Raidcore](https://raidcore.gg/gw2)
- Builds: [Accessibility Wars](https://aw2.help/)
- Tools: GW2Timer Map, GW2 Crafts, Music Box
- Guides: [Guildjen](https://guildjen.com/)
- Discord: Fractal Training, Raidcore, Raid Academy

---

## Included sites

| Category | Sites |
|----------|--------|
| Favorites | Your starred sites (user-defined) |
| Help | How to use (built-in) |
| Official | Guild Wars 2, GW2 News, Raidcore |
| Wiki | GW2 Wiki, Game Updates, Legendaries, Mounts |
| Builds | Snowcrows, MetaBattle, MetaBattle OW, Accessibility Wars, Gw2Skills Editor |
| PvP | MetaBattle PvP |
| WvW | MetaBattle WvW |
| Tools | gw2efficiency, Legendary Tracker, Blish HUD, GW2Timer Events, GW2Timer Map, Meta Timers, GW2 Crafts, Music Box, Peu Research Center |
| Guides | Raid Food (built-in), Guildjen, Living World, Leveling, Earn Gold, Griffon Unlock, Skyscale Unlock, Mukluk Fractals, GW2 TLDR, TLDR Raids, TLDR Fractals |
| Farming | Fast Farming Community |
| Discord | Official, Community, Snowcrows, MetaBattle, Guildjen, Mukluk, Accessibility Wars, Skein Gang, Fractal Training, Raid Academy, GW2 University, Crossroads Inn, Raid Training EU, Welcome to PvP, WvW NA/EU Alliance, Fast Farming, Raidcore, Overflow Trading, GW2 Central Hub |

---

## Notes

- Use **Browse** to pick sites. Star frequent ones into **Favorites**. Prefer **Open Ext** when joining Discord or completing site logins.
- Discord invite pages can open in the embedded browser; finishing a join may require the Discord app.
- Players only need the DLL. The browser helper and homepage assets are embedded and extract on first use; Chromium comes from the game’s `bin64/cef`.
- HTTP cache uses `%TEMP%` (not under `addons`).
- Upgrading from pre-1.5.1.0: replace the DLL; stale files in `addons/` root (if any) are removed automatically on next load.
