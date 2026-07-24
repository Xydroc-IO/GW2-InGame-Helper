# Raidcore / Nexus listing notes

## Short description

```text
In-game browser for Guild Wars 2 — Wiki, Snowcrows, MetaBattle, and more.
```

## Signature

```text
0x48454C50
```

## Version

`2.0.0.0`

## Changelog highlights

- **2.0.0.0** — IPC v5 multi-client; graceful quit; non-blocking URL warm; deferred settings; dirty-rect present
- **1.7.8.53** — Browse display-scaled popup; first-open ~30% window size
- **1.7.8.52** — Browse credit footer: Created by Xydroc; IGN | Discord on one line
- **1.7.8.51** — Browse popup credit footer (Xydroc · IGN · Discord)
- **1.7.8.50** — Fix fast-typing dropped letters (ToUnicode synth + lParam scan codes + input ring v4)
- **1.7.8.49** — Chunked URL warm; about/file map; longest-path host match; Present Map DO_NOT_WAIT; how-to stamp 49
- **1.7.8.48** — Audit follow-up: warm URL indexes; host-map BestMatch; idle ~30 FPS present; StatusCStr strcmp; How to use homepage refresh
- **1.7.8.47** — Audit: keep CEF on collapse; URL-key cache; max-size OSR + UV crop; SET_BOUNDS throttle; StatusCStr; ImGuiIO hotkeys; ~30 Hz closed poll
- **1.7.8.46** — Browse scroll: cache category/section indices + clip large lists
- **1.7.8.45** — Wiki Utility by attribute; Minis section (all miniature pages by wiki subsections)
- **1.7.8.44** — Wiki Food / Ascended Food nested by attribute (wiki TOC); ascended feasts under Ascended Food only
- **1.7.8.43** — Gen 3 legendary variants; Wiki Utility; Upgrades (Superior Runes / Relics / Superior Sigils)
- **1.7.8.42** — Wiki Lifestyle / Crafting / Food / Ascended Feasts; Guides → Crafting (GW2 Crafts Normal/Fast/400-500/Special)
- **1.7.8.41** — Wiki: Cosmetic Infusions moved from Guides; Legendary Weapons (Gen 1–3) section
- **1.7.8.40** — Cosmetic Infusions: per-infusion GW2 Wiki pages
- **1.7.8.39** — Guides → Cosmetic Infusions (all wiki cosmetic infusions); Achievements Side Stories + HoT Verdant Brink / Auric Basin
- **1.7.8.38** — Guides Browse: Raids section nests Raid Wings + Raid Boss
- **1.7.8.37** — How to use: always rewrite helper-home + CEF cache-bust (Browse/Favorites pills)
- **1.7.8.36** — How to use: rewrite helper-home so Browse/Favorites pills stay gone
- **1.7.8.35** — Guides → Jumping Puzzles: Guildjen hub + all JP guides
- **1.7.8.34** — Guides → Mounts: Guildjen Siege Turtle unlock
- **1.7.8.33** — Guides: Guildjen fractals / achievements / beginners / Harvest Temple / W8; Progress + Rifts sections
- **1.7.8.32** — Home hero: remove decorative Browse / Favorites pills
- **1.7.8.31** — Fix stuck “Starting…” after audit: ready when helper accepts CREATE_TAB (not after first browser)
- **1.7.8.30** — Render/IPC/BootJs audit: ~60 FPS present+OSR, mouse wake throttle, tab-cmd retry, frame barriers, softer close, BootJs debounce/cache
- **1.7.8.29** — Fix Snow Crows guides vanishing: ad-strip no longer matches nitro-article placement ids
- **1.7.8.28** — Snow Crows guides: hydrate armory skill embeds + reveal cloaked TLDR / image diagrams
- **1.7.8.27** — Hydrate Guildjen Breeze lazy images so wing guides show content in CEF
- **1.7.8.26** — Stop YouTube embed popups from navigating the main tab; unlock Guildjen YouTube embeds
- **1.7.8.25** — Raid Wings: Guildjen W1–W8 + hub / beginner guide (replaces MetaBattle wing pages)
- **1.7.8.24** — Raid Boss: Snow Crows W1–W7 encounters (replaces Hardstuck); unblock Video.js CDN
- **1.7.8.23** — Browse sections start collapsed; remember expanded state in settings
- **1.7.8.22** — Raid Boss wings nested as collapsible subsections under Raid Boss
- **1.7.8.21** — Hardstuck 10-Player / Squad / Envoy Armor under Raid Boss; ArcDPS under Tools
- **1.7.8.20** — Guides: Hardstuck Raid Boss encounters W1–W8, grouped by wing
- **1.7.8.19** — Browse: collapsible sections within categories
- **1.7.8.18** — Google Search + DuckDuckGo CSS downlevel (display-p3, @property, dvh); skip ad-strip on DDG
- **1.7.8.17** — Gemini color-mix response filter (before paint); Chrome UA; dvh/nesting compat
- **1.7.8.16** — Firefox UA spoof for Google / Gemini login; tip banner + docs for Open Ext when Google blocks CEF
- **1.7.8.15** — Fix Google / Gemini readability (CSS downlevel, skip ad-strip on google.com, allow GTM)
- **1.7.8.14** — Google Gemini under Search → AI
- **1.7.8.13** — TLDR Fractals under Guides → TLDR (with other TLDR entries)
- **1.7.8.12** — GW2 TLDR Dungeons under Guides → TLDR
- **1.7.8.11** — MetaBattle PvP / WvW Guides hubs; Guides Browse sections for PvP and WvW
- **1.7.8.10** — Accessibility Wars under Builds → AccessiBuilds
- **1.7.8.9** — Snowcrows per-profession raid builds; MetaBattle Raid Wing 4
- **1.7.8.8** — Snowcrows build/guide hubs; MetaBattle Fractals, Raid Wings, Strikes sections; site registry validator
- **1.7.8.7** — IPC v3: fenced URL/title, double-buffered frames, safer cmd ring, idle wake event
- **1.7.8.6** — Tab sync/restart stability; install keeps settings.ini; multi-client helper kill fix
- **1.7.8.5** — Nexus updates via `UP_GitHub` + repo URL (direct DLL link for manual download only)
- **1.7.8.4** — Brief `UP_Direct` DLL link (superseded)
- **1.7.8.3** — Tab close is one control per tab so the last tab’s x cannot hit the previous tab
- **1.7.8.2** — Fix last-tab close killing / corrupting the previous tab (no double IPC CLOSE; sync only when helper active_tab matches)
- **1.7.8.1** — PvP/WvW builds under Builds; Uber axe/sickle/pick labeled separately; Wiki Easy Objectives clearer
- **1.7.8.0** — Fix closing the wrong tab; Browse sections everywhere; Vault Easy Objectives; Uber’s axe/sickle/pick labels; checklists use real checkboxes
- **1.7.7.0** — Fix multi-tab CEF slot assignment; interactive cheat-sheet checklists
- **1.7.6.0** — Browse section headers for Tools/Guides/Discord/Builds/Wiki/Official; fix Back→blank page
- **1.7.5.0** — Browse + chrome UI refresh; Cheat Sheets sections; ASCII-safe labels; six sheets from 1.7.4 bundled
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

Manual DLL download (players / mirrors):

```text
https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll
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
