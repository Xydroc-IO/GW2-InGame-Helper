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

`1.7.5.0`

## Changelog highlights

- **1.7.5.0** — Browse + chrome UI refresh; Cheat Sheets sections; ASCII-safe labels (no `?` glyphs); six sheets from 1.7.4 bundled
- **1.7.4.0** — Six cheat sheets (dailies, currency sinks, ascended, portals, homestead, WvW); Home→default site; AR table fixes
- **1.7.3.0** — Remove tabbed cheat sheet hubs; sheets stay as individual Browse entries
- **1.7.2.2** — Uber’s All-In-One + seven cheat sheets (strikes, fractal CMs, squad, stab/cleanse, mats, legendaries, mounts); credit uberduber.1249
- **1.7.2.1** — Fix freeze from per-frame settings.ini writes (debounce + title dirty)
- **1.7.2.0** — Tab hotkeys (Ctrl+T/W/Tab); live tab titles from page/URL
- **1.7.1.1** — Fix new-tab navigating the previous tab
- **1.7.1.0** — Cheat Sheets category (Raid Food + offline sheets)
- **1.7.0.0** — Compact toolbar; pin / reopen tabs; keep-warm option; gold status polish
- **1.6.0.3** — Fix blank Home/Raid Food/restored tabs; ready-resync; no forced homepage on open
- **1.6.0.2** — Fix Home / Raid Food white page (activate+navigate + helper `about:` fallback)
- **1.6.0.1** — Fix Raid Food / helper home blank page under live tabs (`about:` → `file://` on CreateTab)
- **1.6.0.0** — Live multi-tab CEF; Browse-style + picker; fav reorder; New Tab; Find in page
- **1.5.5.0** — Persist open tabs; fix Options default landing site
- **1.5.4.1** — Escape no longer closes the helper
- **1.5.4.0** — Full GW2 ImGui theme; Raid Food match; toolbar Search; KillProof/Wingman/BLTC/Forums/DDG
- **1.5.3.1** — Search category with Google
- **1.5.3.0** — Multi-tab browsing (up to 8); Ctrl+click / + opens a new tab
- **1.5.2.1** — Favorites star drawn with ImGui (fixes ? glyph from missing font chars)
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

| Bind / key | Default | Action |
|------------|---------|--------|
| `KB_HELPER_TOGGLE` | `Ctrl+Shift+H` (or `K`) | Open / close helper |
| (in helper) | `Ctrl+T` | New-tab site picker |
| (in helper) | `Ctrl+W` | Close tab (unless pinned) |
| (in helper) | `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab |
| (in helper) | `Ctrl+Shift+T` | Reopen closed tab |
| (in helper) | `Ctrl+F` | Find in page |

## Player artifact

Ship **only** `GW2-InGame-Helper.dll` into `<GW2>/addons/`.
Runtime extracts into `<GW2>/addons/GW2-InGame-Helper/`.
