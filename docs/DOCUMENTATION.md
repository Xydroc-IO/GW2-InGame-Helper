# Documentation index

GW2 In-Game Helper — where to find what.

- **Shipping revision:** **2.3.0.0** (`GW2-InGame-Helper.dll`, private CEF 150) —
  see [`RELEASE_NOTES.md`](RELEASE_NOTES.md)
- **Contributor entry:** [`../CONTRIBUTING.md`](../CONTRIBUTING.md)

---

## Docs in this folder

| Doc | Audience | Contents |
|-----|----------|----------|
| [`../README.md`](../README.md) | Everyone | Install, features, site list, troubleshooting |
| [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Contributors | Build, stamps, PR habits |
| [`../SECURITY.md`](../SECURITY.md) | Everyone | Vulnerability reporting + key handling |
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Everyone | Full changelog + current **Stamps:** |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Contributors | Allowed / forbidden Nexus and TOS boundaries |
| [`WHITEPAPER.md`](WHITEPAPER.md) | Researchers / advanced contributors | Design rationale, trade-offs, Proton |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors | Process model, IPC, CEF, stamp table |
| [`PATHING.md`](PATHING.md) | Contributors | Packs, Features, compass, D3D world GPS |
| [`THEMES.md`](THEMES.md) | Players / contributors | Drop-in `config/themes/` color themes |
| [`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md) | Site operators | User-Agent allow/deny (`GW2-InGame-Helper`) |
| [`DPS_LOGS.md`](DPS_LOGS.md) | Players | Elite Insights, .NET 8, Protontricks, KillProof |
| [`API_KEY.md`](API_KEY.md) | Players | ArenaNet API key scopes |
| [`description.html`](description.html) | Listings | Nexus / web description HTML |
| [`CATALOG_RELEASE.md`](CATALOG_RELEASE.md) | Maintainers | Pre-release tag `gw2-helper-catalog` (`.igh` + CEF zip) |
| [`media/`](media/) | Build | Cover / homepage embed assets |

Everything under `docs/` is tracked in git (no draft allow-list).

---

## Catalog

| Path | Purpose |
|------|---------|
| `data/sites.json` | **Canonical** Browse registry (schema v2; embedded → runtime `addons/…/sites.json`) |

```bash
make validate-sites
# optional: re-derive browsePath from legacy rules
make enrich-sites
```

---

## Media

| Path | Use |
|------|------|
| `docs/media/cover.png` | README / store cover |
| `docs/media/logo.png` | Branding |
| `docs/media/home-logo.png` / `home-cover.jpg` | Embedded into homepage at build |

---

## Version stamp checklist (when shipping)

**Always keep stamps in lockstep with source.** Do not ship with drifted docs.
Run `make check-stamps` (also part of `make ci`) before tagging.

| When | What to bump |
|------|----------------|
| Cutting a release | `AddonVersion.h` + every doc revision / COMPLIANCE snapshot |
| Helper EXE / IPC / BootJs behavior changes | `kHelperStamp` |
| Homepage HTML / embedded home assets change | `kHomePageVersion` |
| `data/sites.json` catalog changes that must re-extract | `kSitesStamp` (`s####`) |
| Cheat sheet pack changes | CheatSheets `kPackStamp` (`c####`) |
| Live panel HTML / cache schema changes | `kPanelVer` |
| Raid food sheet changes | `kRaidFoodVersion` |
| `data/ui-chrome` pack changes | UiChrome `kPackStamp` (`uc##`) |

Keep these aligned every ship (or every stamp bump):

1. `src/app/AddonVersion.h` — `ADDON_VERSION_*`
2. `src/browser/WikiBrowserHelper.cpp` — `kHelperStamp`
3. `src/browse/HomePage.cpp` — `kHomePageVersion`
4. `src/browse/sites/SitesLoadParse.cpp` — `kSitesStamp`
5. `src/browse/CheatSheets.cpp` — `kPackStamp`
6. `src/browse/livepanels/LivePanelsInternal.h` — `kPanelVer`
7. `src/browse/RaidFood.cpp` — `kRaidFoodVersion`
8. `src/app/UiChrome.cpp` — `kPackStamp`
9. `README.md`, `docs/RELEASE_NOTES.md` (header + current **Stamps:** line), `docs/description.html`
10. `docs/ARCHITECTURE.md` stamp table · `docs/COMPLIANCE.md` policy snapshot · `docs/WHITEPAPER.md` appendix · domain doc **Revision:** lines when relevant
11. `make check-stamps && make && make install` (or release package)

**Rule:** if you change a stamped asset or bump `AddonVersion.h`, update the matching stamp **and** the doc tables in the same change. Never leave COMPLIANCE / WHITEPAPER / RELEASE_NOTES on an older revision.
