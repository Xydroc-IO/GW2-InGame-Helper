# GW2 In-Game Helper v1.5.3.1

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
| New tab | **+** or Ctrl+click a site in Browse (max 8) |
| Close tab | **x** on the tab · middle-click |
| Favorite site | Star button on toolbar or next to Browse rows |
| Copy current URL | Toolbar **Copy URL** |
| Open in system browser | Toolbar **Open Ext** |

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.5.3.1

- **Search** category with [Google](https://www.google.com/)

---

## What’s new in 1.5.3.0

- **Tabs** — keep up to 8 sites open and switch between them
- Click a site in Browse to replace the current tab; **Ctrl+click** or **+** opens a new tab
- Each tab remembers its last URL; per-tab back/forward when multiple tabs are open
- Close with **x** or middle-click (cannot close the last tab)

---

## What’s new in 1.5.2.1

- **Fix:** Favorites star renders as a drawn icon (default ImGui font has no ★/☆ glyphs, which showed as `?`)

---

## What’s new in 1.5.2.0

- **Favorites** — star sites you use most
- **Favorites** category at the top of Browse
- Toolbar star toggles the current site
- Persisted as `FavoriteIds=` in `addons/GW2-InGame-Helper/settings.ini`

---

## What’s new in 1.5.1.0

- **Install layout** — runtime files live under `addons/GW2-InGame-Helper/`
- **DLL only in `addons/`**
- **Stale cleanup** for older root installs

---

## Features (current)

- In-game CEF browser using the game’s `bin64/cef`
- **Browse** panel — search and pick sites by category
- **Search** — Google and room for more engines
- **Tabs** — up to 8 open sites
- **Favorites** — pin frequently used sites
- Branded homepage + built-in Raid Food guide
- Toolbar: Browse · star · Back/Forward · Home · Reload · Copy URL · Open Ext
- Nexus QuickAccess · GitHub auto-update · no memory reads

---

## Notes

- Switching tabs reloads the saved URL in the single embedded browser (scroll/form state is not kept live — keeps memory Wine-friendly).
- Prefer **Open Ext** for Discord joins and site logins.
- Players only need the DLL.
