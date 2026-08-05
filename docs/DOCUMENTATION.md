# Documentation index

GW2 In-Game Helper — where to find what.

- **Shipping revision:** **2.2.0.11** (`GW2-InGame-Helper.dll`, private CEF 150) —
  see [`RELEASE_NOTES.md`](RELEASE_NOTES.md)
- **Contributor entry:** [`../CONTRIBUTING.md`](../CONTRIBUTING.md)

---

## Reading paths

### Players

1. [`../README.md`](../README.md) — install, features, troubleshooting  
2. [`DPS_LOGS.md`](DPS_LOGS.md) / [`API_KEY.md`](API_KEY.md) as needed  
3. [`../pathing/README.md`](../pathing/README.md) — packs & GPS player notes  

### Contributors (implementation)

1. [`COMPLIANCE.md`](COMPLIANCE.md) — normative allow/deny  
2. [`MODULES.md`](MODULES.md) — where code lives  
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — systems map  
4. [`KERNEL.md`](KERNEL.md) — CEF/IPC/present playbooks  
5. [`NAV_AND_ADS.md`](NAV_AND_ADS.md) — ads non-regression  
6. Domain: [`PATHING.md`](PATHING.md), [`ACCOUNT.md`](ACCOUNT.md), [`DPS_LOGS.md`](DPS_LOGS.md)  
7. [`BUILD.md`](BUILD.md) / [`ONBOARDING.md`](ONBOARDING.md)  

### Researchers / design rationale

1. [`WHITEPAPER.md`](WHITEPAPER.md) — academic technical report  
2. [`ARCHITECTURE.md`](ARCHITECTURE.md) — operational companion  
3. [`reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md`](reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md) — ecosystem survey (historical)

---

## Start here

| Doc | Audience | Contents |
|-----|----------|----------|
| [`../README.md`](../README.md) | Everyone | Install, features, site list, troubleshooting, build pointers |
| [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Contributors | Ownership zones, build/validate, PR checklist |
| [`ONBOARDING.md`](ONBOARDING.md) | New maintainers | First-week takeover checklist |
| [`MODULES.md`](MODULES.md) | Contributors | ≤500-line layout, Shared/Internal pattern |
| [`KERNEL.md`](KERNEL.md) | Kernel editors | WikiBrowser / helper ownership, stamps, playbooks |
| [`../SECURITY.md`](../SECURITY.md) | Everyone | Vulnerability reporting + key handling |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors | Process model, IPC, present/input, CEF, source map |
| [`WHITEPAPER.md`](WHITEPAPER.md) | Researchers / advanced contributors | Design rationale, trade-offs, security, Proton, application layer |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Contributors | Allowed / forbidden Nexus and TOS boundaries |
| [`NAV_AND_ADS.md`](NAV_AND_ADS.md) | Kernel contributors | Open Ext, ads policy, non-regression |
| [`PATHING.md`](PATHING.md) | Contributors | Packs, Features, compass, D3D world GPS |
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
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Full changelog (current: **2.2.0.11**) |
| [`RELEASE_NOTES_CEF_RUNTIME.md`](RELEASE_NOTES_CEF_RUNTIME.md) | GitHub body for tag `cef-runtime-150` (zip asset) |
| [`description.html`](description.html) | HTML listing for web / Nexus description paste |
| [`reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md`](reports/GW2_Addon_Ecosystem_Academic_Report_2026-08.md) | Academic ecosystem survey (Aug 2026) |
| `RAIDCORE.md` *(gitignored)* | Local Nexus listing draft |
| `DISCORD.md` / `DISCORD_*.md` *(gitignored)* | Local Discord announcement drafts |

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

Bump **only when asked**. Keep these aligned:

1. `src/entry.cpp` — `G::AddonDef.Version` (Major / Minor / Build / Revision)
2. `src/browser/WikiBrowserHelper.cpp` — `kHelperStamp`
3. `src/browse/HomePage.cpp` — `kHomePageVersion`
4. Sites / cheatsheets stamps (`s####` / `c####`) when extract caches must invalidate
5. `README.md`, `RELEASE_NOTES.md`, `description.html`; local (gitignored) `RAIDCORE.md`, `DISCORD.md`
6. Refresh `ARCHITECTURE.md` / `WHITEPAPER.md` appendices if constants changed
7. `make && make install` (or release package)

---

## What Browse entries are

Browse rows are **labeled hyperlinks** (`SiteDef`): id + label + URL. Canonical data is `data/sites.json`. The helper navigates CEF to that URL. There is no private site API integration for MetaBattle, Guildjen, or GW2.app — only deep links plus CEF polish (CSS downlevel, YouTube cards, login tips).

---

## Documentation quality bar

Public engineering docs should remain **report-grade**: explicit revision, reproducible commands, normative vs descriptive separation (compliance vs architecture vs whitepaper), and update-in-lockstep with behavioral changes.

[`WHITEPAPER.md`](WHITEPAPER.md) aims at **academic technical-report quality** (structured abstract, contributions, related work, evaluation criteria, threats to validity, numbered references, appendices). It is still a project document—not a peer-reviewed journal article. [`ARCHITECTURE.md`](ARCHITECTURE.md) remains the operational systems map; [`COMPLIANCE.md`](COMPLIANCE.md) remains normative policy; [`NAV_AND_ADS.md`](NAV_AND_ADS.md) / [`KERNEL.md`](KERNEL.md) remain actionable contributor playbooks.
