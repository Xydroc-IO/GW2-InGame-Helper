# Raidcore / Nexus listing notes

## Short description

```text
In-game browser for GW2 sites and Discords. One DLL. Ctrl+Shift+H toggle. No memory reads.
```

## Signature

```text
0x48454C50
```

## Version

`1.5.2.0`

## Changelog highlights

- **1.5.2.0** — Favorites: star sites in Browse / toolbar; persisted in settings
- **1.5.1.0** — Runtime data under `addons/GW2-InGame-Helper/`; DLL only in `addons/`
- **1.4.1.8** — Full-page themed homepage redesign
- **1.4.1.7** — Removed Lucky Noobs
- **1.4.1.6** — Browse panel (search + categories)
- **1.4.1.5** — Gap-fill sites + Copy URL / Open Ext
- **1.4.1.0–1.4.1.4** — Community sites, Raid Food, Discords (no Hardstuck/Discretize)

See [`RELEASE_NOTES.md`](RELEASE_NOTES.md) for full notes.

## Updates

```text
Provider   = UP_GitHub
UpdateLink = https://github.com/Xydroc-IO/GW2-InGame-Helper
```

## Full HTML

[`description.html`](description.html) · Release notes: [`RELEASE_NOTES.md`](RELEASE_NOTES.md)

## Hotkeys

| Bind ID | Default | Action |
|---------|---------|--------|
| `KB_HELPER_TOGGLE` | `CTRL+SHIFT+H` | Open / close helper |

## Player artifact

Ship **only** `GW2-InGame-Helper.dll` into `<GW2>/addons/`.
Runtime extracts into `<GW2>/addons/GW2-InGame-Helper/`.
