# GW2 In-Game Helper v1.4.1.4

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

## Install

Copy `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`. Nothing else.

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` (or `K`) · QuickAccess icon |
| Rebind | Nexus `Ctrl+O` → `KB_HELPER_TOGGLE` |

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.4.1.4

- **Expanded sites** — GW2 News, MetaBattle Open World / PvP / WvW, GW2Timer Events, Lucky Noobs
- **New categories** — PvP, WvW
- **Expanded Discords** — Official, Community, Snowcrows, MetaBattle, Guildjen, Mukluk, Accessibility Wars, Skein Gang, Lucky Noobs, GW2 University, Crossroads Inn, Raid Training EU, Welcome to PvP, WvW NA/EU Alliance, Fast Farming
- Hardstuck and Discretize intentionally omitted (outdated)

## What’s new in 1.4.1.3

- **Homepage refresh** — branded hero with embedded logo and cover art, GW2 gold theme

## What’s new in 1.4.1.2

- **Built-in Raid Food guide** — Universal Seasoning Guide (Guides → Raid Food)

## What’s new in 1.4.1.1

- Gw2Skills Editor, Meta Timers, Peu Research Center, Mukluk Fractals, GW2 TLDR

## What’s new in 1.4.1

- **Fix:** Music Box HTTP (`http://gw2mb.com/`)

## What’s new in 1.4.0

- Community sites + Discord category (Fractal Training, Raidcore, Raid Academy)

---

## Included sites

| Category | Sites |
|----------|--------|
| Help | How to use (built-in) |
| Official | Guild Wars 2, GW2 News, Raidcore |
| Wiki | GW2 Wiki |
| Builds | Snowcrows, MetaBattle, MetaBattle OW, Accessibility Wars, Gw2Skills Editor |
| PvP | MetaBattle PvP |
| WvW | MetaBattle WvW |
| Tools | gw2efficiency, GW2Timer Events, GW2Timer Map, Meta Timers, GW2 Crafts, Music Box, Peu Research Center |
| Guides | Raid Food, Lucky Noobs, Guildjen, Mukluk Fractals, GW2 TLDR |
| Farming | Fast Farming |
| Discord | Official, Community, Snowcrows, MetaBattle, Guildjen, Mukluk, Accessibility Wars, Skein Gang, Lucky Noobs, Fractal Training, Raid Academy, GW2 University, Crossroads Inn, Raid Training EU, Welcome to PvP, WvW NA/EU Alliance, Fast Farming, Raidcore |

## Notes

- Discord invite pages open in the embedded browser; finishing a join may require the Discord app.
- Players only need the DLL. The browser helper is embedded and extracts on first use; Chromium comes from the game’s `bin64/cef`.
