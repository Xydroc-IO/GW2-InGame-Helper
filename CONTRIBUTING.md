# Contributing — GW2 In-Game Helper

Thanks for helping. This addon is a **Raidcore Nexus** ImGui DLL plus an
out-of-process **private CEF 150** helper. Normative allow/deny rules live in
[`docs/COMPLIANCE.md`](docs/COMPLIANCE.md). Report security issues via
[`SECURITY.md`](SECURITY.md) — do not open public issues for exploitable bugs.

## Published docs in git

Tracked under `docs/` (see `.gitignore` allow-list):

| Doc | Role |
|-----|------|
| [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md) | Changelog + current **Stamps:** line |
| [`docs/COMPLIANCE.md`](docs/COMPLIANCE.md) | Allowed / forbidden boundaries |
| [`docs/WHITEPAPER.md`](docs/WHITEPAPER.md) | Design rationale |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Systems map / stamp table |
| [`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md) | Doc index + version stamp checklist |
| [`docs/PATHING.md`](docs/PATHING.md) | Pathing packs / GPS |
| [`docs/THEMES.md`](docs/THEMES.md) | Drop-in `config/themes/` color themes |
| [`docs/PUBLISHER_ACCESS.md`](docs/PUBLISHER_ACCESS.md) | User-Agent allow/deny for site operators |
| [`docs/DPS_LOGS.md`](docs/DPS_LOGS.md) | Elite Insights / Proton setup |
| [`docs/API_KEY.md`](docs/API_KEY.md) | ArenaNet API scopes |
| [`docs/description.html`](docs/description.html) | Nexus / web listing HTML |
| [`docs/media/`](docs/media/) | Cover / homepage embed assets |

Local drafts (`RAIDCORE.md`, `DISCORD*.md`, `CODE_AUDIT.md`, …) stay gitignored.
Do not force-add them.

Player-facing overview: [`README.md`](README.md).

## Prerequisites

- Linux or similar with `x86_64-w64-mingw32-g++`, `make`, `python3`, `git`
- Arch/Manjaro example: `sudo pacman -S --needed mingw-w64-gcc make git`
- Guild Wars 2 + Nexus for in-game smoke tests (`make install`)

## Build

```bash
make -j"$(nproc)"          # DLL + embedded helper / assets
make validate-sites        # data/sites.json schema
make check-stamps          # version + extract stamps vs docs
make ci                    # full local gate (stamps, tests, MinGW build)
```

Install into a GW2 tree (default Steam path; override with `GW2_ROOT=`):

```bash
make install
# make install GW2_ROOT="/path/to/Guild Wars 2"
```

Packaging helpers:

- `python3 tools/pack_ui_chrome.py` — `data/ui-chrome/` → `build/ui_chrome.zip`
- `python3 tools/pack_cheatsheets.py` — offline sheets zip
- `make pack-cef` — CEF runtime zip (maintainers)

## Before you open a PR

1. Run `make ci` and fix failures.
2. Stay inside [`docs/COMPLIANCE.md`](docs/COMPLIANCE.md): Nexus APIs, private CEF
   under the addon data folder, read-only official APIs / MumbleLink. **No** game
   memory R/W, Present hooks, or writes into `bin64/cef`.
3. Prefer **≤500 lines** per new/edited `.cpp` (split modules rather than grow
   large TUs).
4. Match surrounding style; keep diffs focused on the change.
5. Never commit secrets (`settings.ini`, API keys), `cef/`, pathing `.taco` packs,
   or addon runtime trees.

### Version & extract stamps

Shipping version and extract stamps are SSOT in code (`src/app/AddonVersion.h`
and the `k*Stamp` / `k*Version` constants). If you bump `ADDON_VERSION_*` **or**
change a stamped asset (helper blob, homepage, sites, cheatsheets, live panels,
raid food, ui-chrome), update the matching stamp **in the same change**, then
sync the current **Stamps:** line in [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md)
(and other stamp tables when those docs are present). Run `make check-stamps`.

Stamp constants (see also release notes):

| Stamp | Source |
|-------|--------|
| Helper | `src/browser/WikiBrowserHelper.cpp` → `kHelperStamp` |
| Homepage | `src/browse/HomePage.cpp` → `kHomePageVersion` |
| Sites | `src/browse/sites/SitesLoadParse.cpp` → `kSitesStamp` |
| CheatSheets | `src/browse/CheatSheets.cpp` → `kPackStamp` |
| Live panels | `src/browse/livepanels/LivePanelsInternal.h` → `kPanelVer` |
| Raid food | `src/browse/RaidFood.cpp` → `kRaidFoodVersion` |
| UI chrome | `src/app/UiChrome.cpp` → `kPackStamp` |

## Pull requests

- Prefer small, reviewable PRs with a clear why.
- Describe user-visible behavior and any stamp / compliance impact.
- Do not push secrets or large binary dumps; ui-chrome PNGs belong only under
  `data/ui-chrome/` via the existing pack pipeline.

## Code map (short)

| Area | Path |
|------|------|
| Nexus entry / hotkeys | `src/entry*.cpp` |
| ImGui chrome / Browse | `src/ui/` |
| CEF host + IPC | `src/browser/` |
| CEF helper process | `src/helper/` |
| Official API pads | `src/account/`, `src/economy/`, … |
| Pathing / GPS | `src/pathing/` |
| Curated sites / sheets | `data/sites.json`, `data/cheatsheets/` |
| Immersive textures | `data/ui-chrome/` (ArenaNet assets — not MIT) |

## Themes

Drop-in color themes live under the addon data folder
`config/themes/<name>/theme.ini` (seeded at runtime). Settings → Theme.
Stamped `ui-chrome` PNGs and external browse sites are not recolored by themes.

## License

Contributions are accepted under the project [MIT License](LICENSE). Guild Wars 2
and related trademarks belong to ArenaNet / NCSoft; curated UI textures in
`data/ui-chrome/` remain ArenaNet-owned and are **not** relicensed under MIT.
