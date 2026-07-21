# GW2 In-Game Helper v1.4.1

<p align="center">
  <img src="media/cover.png" alt="GW2 In-Game Helper" width="100%">
</p>

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

**Install:** put `GW2-InGame-Helper.dll` in `<GW2>/addons/`. Nothing else.

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` (or `K`) · QuickAccess icon |

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Wine/Proton OK).

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

## What’s new in 1.4.1

- Fix Music Box URL: site only works over HTTP (`http://gw2mb.com/`); HTTPS has a bad certificate and returns 403

## What’s new in 1.4.0

- Added sites: GW2Timer Map, Accessibility Wars, Raidcore, GW2 Crafts, Guildjen, Music Box
- New **Discord** category: Fractal Training, Raidcore, Raid Academy

## Download (players)

**You only need one file:** `GW2-InGame-Helper.dll` → `<Guild Wars 2>/addons/`

## Included sites

| Category | Sites |
|----------|--------|
| Help | How to use |
| Official | Guild Wars 2, Raidcore |
| Wiki | GW2 Wiki |
| Builds | Snowcrows, MetaBattle, Accessibility Wars |
| Tools | gw2efficiency, GW2Timer Map, GW2 Crafts, Music Box |
| Guides | Guildjen |
| Farming | Fast Farming |
| Discord | Fractal Training, Raidcore, Raid Academy |

## Controls

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` or `K`, or QuickAccess icon |
| Rebind | Nexus `Ctrl+O` → `KB_HELPER_TOGGLE` |

## License

MIT — see [LICENSE](../LICENSE).

Build: `make -j$(nproc)` → `build/bin/GW2-InGame-Helper.dll`
