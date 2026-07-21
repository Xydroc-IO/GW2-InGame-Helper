# GW2 In-Game Helper v1.4.1.3

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

## What’s new in 1.4.1.3

- **Homepage refresh** — branded hero with embedded logo and cover art, GW2 gold theme

## What’s new in 1.4.1.2

- **Built-in Raid Food guide** — Universal Seasoning Guide with power, condition, support, and hybrid/tank feast recipes (Guides → Raid Food)

## What’s new in 1.4.1.1

- **New sites**
  - Builds: [Gw2Skills Editor](https://en.gw2skills.net/editor/)
  - Tools: [Meta Timers](https://gw2tldr.com/metas), [Peu Research Center](https://peuresearchcenter.com/index.html)
  - Guides: [Mukluk Fractals](https://mukluklabs.com/gw2-fractal-guides), [GW2 TLDR](https://gw2tldr.com/)

## What’s new in 1.4.1

- **Fix:** Music Box now opens over HTTP (`http://gw2mb.com/`). The HTTPS endpoint uses an invalid certificate and returns 403.

## What’s new in 1.4.0

- **New sites**
  - Official: [Raidcore](https://raidcore.gg/gw2)
  - Builds: [Accessibility Wars](https://aw2.help/)
  - Tools: [GW2Timer Map](https://gw2timer.com/?page=Map), [GW2 Crafts](https://gw2crafts.net/), [Music Box](http://gw2mb.com/)
  - Guides: [Guildjen](https://guildjen.com/)
- **New Discord category**
  - [Fractal Training](https://discord.com/invite/zxeVeSqpuS)
  - [Raidcore](https://discord.com/invite/raidcore)
  - [Raid Academy](https://discord.com/invite/gw2ra)

---

## Included sites

| Category | Sites |
|----------|--------|
| Help | How to use (built-in) |
| Official | Guild Wars 2, Raidcore |
| Wiki | GW2 Wiki |
| Builds | Snowcrows, MetaBattle, Accessibility Wars, Gw2Skills Editor |
| Tools | gw2efficiency, GW2Timer Map, Meta Timers, GW2 Crafts, Music Box, Peu Research Center |
| Guides | Raid Food (built-in), Guildjen, Mukluk Fractals, GW2 TLDR |
| Farming | Fast Farming |
| Discord | Fractal Training, Raidcore, Raid Academy |

## Notes

- Discord invite pages open in the embedded browser; finishing a join may require the Discord app.
- Players only need the DLL. The browser helper is embedded and extracts on first use; Chromium comes from the game’s `bin64/cef`.
