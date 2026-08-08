# Documentation index

GW2 In-Game Helper — where to find what.

- **Shipping revision:** **2.2.3.10** (`GW2-InGame-Helper.dll`, private CEF 150) —
  see [`RELEASE_NOTES.md`](RELEASE_NOTES.md)
- **Contributor entry:** [`ONBOARDING.md`](ONBOARDING.md)

---

## Reading paths

### Players

1. [`../README.md`](../README.md) — install, features, troubleshooting  
2. [`DPS_LOGS.md`](DPS_LOGS.md) / [`API_KEY.md`](API_KEY.md) as needed  
3. [`../pathing/README.md`](../pathing/README.md) — packs & GPS player notes  

### Contributors (implementation)

1. [`COMPLIANCE.md`](COMPLIANCE.md) — normative allow/deny  
2. [`ARCHITECTURE.md`](ARCHITECTURE.md) — systems map / module layout  
3. [`KERNEL.md`](KERNEL.md) — CEF/IPC/present playbooks  
4. [`NAV_AND_ADS.md`](NAV_AND_ADS.md) — ads non-regression  
5. Domain: [`PATHING.md`](PATHING.md), [`ACCOUNT.md`](ACCOUNT.md), [`COMPLETION.md`](COMPLETION.md), [`FARMING.md`](FARMING.md), [`DPS_LOGS.md`](DPS_LOGS.md), [`THEMES.md`](THEMES.md)  
6. [`BUILD.md`](BUILD.md) / [`ONBOARDING.md`](ONBOARDING.md)  

### Researchers / design rationale

1. [`WHITEPAPER.md`](WHITEPAPER.md) — academic technical report  
2. [`ARCHITECTURE.md`](ARCHITECTURE.md) — operational companion  
3. [`reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md`](reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md) — ecosystem survey (historical)

---

## Start here

| Doc | Audience | Contents |
|-----|----------|----------|
| [`../README.md`](../README.md) | Everyone | Install, features, site list, troubleshooting, build pointers |
| [`ONBOARDING.md`](ONBOARDING.md) | New maintainers | First-week takeover checklist; build/validate + PR habits |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors | Process model, IPC, present/input, CEF, source map, ≤500-line layout |
| [`KERNEL.md`](KERNEL.md) | Kernel editors | WikiBrowser / helper ownership, stamps, playbooks |
| [`../SECURITY.md`](../SECURITY.md) | Everyone | Vulnerability reporting + key handling |
| [`WHITEPAPER.md`](WHITEPAPER.md) | Researchers / advanced contributors | Design rationale, trade-offs, security, Proton, application layer |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Contributors | Allowed / forbidden Nexus and TOS boundaries |
| [`NAV_AND_ADS.md`](NAV_AND_ADS.md) | Kernel contributors | Open Ext, ads policy, non-regression |
| [`PATHING.md`](PATHING.md) | Contributors | Packs, Features, compass, D3D world GPS |
| [`COMPLETION.md`](COMPLETION.md) | Contributors / players | Checklist / Atlas / routes + GPS handoff |
| [`FARMING.md`](FARMING.md) | Contributors / players | Farm-run checklists + fishing log |
| [`THEMES.md`](THEMES.md) | Players / contributors | Drop-in `config/themes/` color themes |
| [`ACCOUNT.md`](ACCOUNT.md) | Contributors / players | Official API pads + key scopes pointer |
| [`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md) | Site operators | Allow/deny via User-Agent (`GW2-InGame-Helper`) |
| [`CEF_RUNTIME.md`](CEF_RUNTIME.md) | Contributors | Private CEF 150 first-run download, zip, SHA |
| [`DPS_LOGS.md`](DPS_LOGS.md) | Players | Elite Insights, .NET 8, Protontricks, KillProof |
| [`API_KEY.md`](API_KEY.md) | Players | ArenaNet API key scopes + Nexus Options paste |
| [`BUILD.md`](BUILD.md) | Contributors | MinGW cross-compile, install, sites, **CI** |
| `CODE_AUDIT.md` *(gitignored)* | Local | Working audit notes |
| `SNOWCROWS.md` *(gitignored)* | Local | Partner brief (not published) |

---

## Releases and listings

| Doc | Purpose |
|-----|---------|
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Full changelog (current: **2.2.3.10**) |
| [`RELEASE_NOTES_CEF_RUNTIME.md`](RELEASE_NOTES_CEF_RUNTIME.md) | GitHub body for tag `cef-runtime-150` (zip asset) |
| [`description.html`](description.html) | HTML listing for web / Nexus description paste |
| [`reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md`](reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md) | Academic ecosystem survey (Aug 2026) |
| `RAIDCORE.md` *(gitignored)* | Local Nexus listing draft (short/long paste) |
| `DISCORD.md` *(gitignored)* | Local Discord release announce (paste-ready) |
| `DISCORD_FEATURES.md` *(gitignored)* | Local Discord feature-spotlight / follow-up thread |

---

## Catalog

| Doc | Purpose |
|-----|---------|
| [`CATALOG.md`](CATALOG.md) | Browse outline by category / section |
| `data/sites.json` | **Canonical** registry (schema v2; embedded → runtime `addons/…/sites.json`) |

After editing the catalog:

```bash
make validate-sites
# optional: re-derive browsePath from legacy rules
make enrich-sites
```

Browse hierarchy is data-driven (`browsePath` / `browseSections`). Runtime file: `addons/<addon>/sites.json`.

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

1. `src/app/AddonVersion.h` — `ADDON_VERSION_*` (feeds `entry.cpp` / shipping version)
2. `src/browser/WikiBrowserHelper.cpp` — `kHelperStamp`
3. `src/browse/HomePage.cpp` — `kHomePageVersion`
4. `src/browse/sites/SitesLoadParse.cpp` — `kSitesStamp`
5. `src/browse/CheatSheets.cpp` — `kPackStamp`
6. `src/browse/livepanels/LivePanelsInternal.h` — `kPanelVer`
7. `src/browse/RaidFood.cpp` — `kRaidFoodVersion`
8. `src/app/UiChrome.cpp` — `kPackStamp`
9. `README.md`, `docs/RELEASE_NOTES.md` (header + current **Stamps:** line), `docs/description.html`
10. `docs/ARCHITECTURE.md` stamp table · `docs/COMPLIANCE.md` policy snapshot · `docs/WHITEPAPER.md` appendix · domain docs (`PATHING`, `ACCOUNT`, `COMPLETION`, `FARMING`, …) **Revision:** lines
11. Local drafts if you use them: `RAIDCORE.md`, `DISCORD.md`
12. `make check-stamps && make && make install` (or release package)

**Rule for agents / contributors:** if you change a stamped asset or bump `AddonVersion.h`, update the matching stamp **and** the doc tables in the same change. Never leave COMPLIANCE / WHITEPAPER / RELEASE_NOTES on an older revision.

---

## What Browse entries are

Browse rows are **labeled hyperlinks** (`SiteDef`): id + label + URL. Canonical data is `data/sites.json`. The helper navigates CEF to that URL. There is no private site API integration for MetaBattle, Guildjen, or GW2.app — only deep links plus CEF polish (CSS downlevel, YouTube cards, login tips).

---

## Documentation quality bar

Public engineering docs should remain **report-grade**: explicit revision, reproducible commands, normative vs descriptive separation (compliance vs architecture vs whitepaper), and update-in-lockstep with behavioral changes.

[`WHITEPAPER.md`](WHITEPAPER.md) aims at **academic technical-report quality** (structured abstract, contributions, related work, evaluation criteria, threats to validity, numbered references, appendices). It is still a project document—not a peer-reviewed journal article. [`ARCHITECTURE.md`](ARCHITECTURE.md) remains the operational systems map; [`COMPLIANCE.md`](COMPLIANCE.md) remains normative policy; [`NAV_AND_ADS.md`](NAV_AND_ADS.md) / [`KERNEL.md`](KERNEL.md) remain actionable contributor playbooks.
