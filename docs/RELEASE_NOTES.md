# GW2 In-Game Helper v1.5.4.0

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

```text
addons/GW2-InGame-Helper.dll          ← install this (only file you place)
addons/GW2-InGame-Helper/             ← created at runtime (helper, pages, settings)
```

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` (or `K`) · QuickAccess icon |
| Browse / Favorites / Tabs | Toolbar Browse · star · tab bar (`+` / Ctrl+click) |
| Search | Toolbar **Search…** + Enter / Go |
| Favorite | Star button |
| Copy / Open Ext | Toolbar |

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.5.4.0

- **GW2 theme** across the ImGui window (gold borders, bronze panels, themed tabs/scrollbars)
- **Raid Food** page palette aligned with the homepage
- Toolbar **Search** box (Wiki/Google; Google fallback elsewhere)
- New sites: DuckDuckGo, Forums, KillProof, Wingman, GW2BLTC, GW2 Treasures

---

## Recent

| Version | Highlights |
|---------|------------|
| **1.5.3.1** | Search category + Google |
| **1.5.3.0** | Multi-tab browsing |
| **1.5.2.x** | Favorites + star icon fix |
| **1.5.1.0** | Runtime under `addons/GW2-InGame-Helper/` |

---

## Notes

- Switching tabs reloads the saved URL (single browser — Wine-friendly).
- Prefer **Open Ext** for Discord joins and logins.
- Players only need the DLL.
