# Documentation index

GW2 In-Game Helper — where to find what.

- **Shipping revision:** **2.1.0.2** (`GW2-InGame-Helper.dll`, private CEF 150) —
  see [`RELEASE_NOTES.md`](RELEASE_NOTES.md)
- **Contributor entry:** [`../CONTRIBUTING.md`](../CONTRIBUTING.md)

---

## Start here

| Doc | Audience | Contents |
|-----|----------|----------|
| [`../README.md`](../README.md) | Everyone | Install, features, site list, troubleshooting, build pointers |
| [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Contributors | Ownership zones, build/validate, PR checklist |
| [`ONBOARDING.md`](ONBOARDING.md) | New maintainers | First-week takeover checklist (Beta) |
| [`../SECURITY.md`](../SECURITY.md) | Everyone | Vulnerability reporting + key handling |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors | Process model, IPC, present/input, CEF, source map |
| [`WHITEPAPER.md`](WHITEPAPER.md) | Researchers / advanced contributors | Design rationale, trade-offs, security, Proton, limitations |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Contributors | Allowed / forbidden Nexus and TOS boundaries |
| [`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md) | Site operators | How to allow/deny the addon via User-Agent (`GW2-InGame-Helper`) |
| [`CEF_RUNTIME.md`](CEF_RUNTIME.md) | Contributors | Private CEF 150 first-run download, zip, SHA |
| [`DPS_LOGS.md`](DPS_LOGS.md) | Players | DPS Logs pad — Elite Insights, .NET 8, Protontricks, KillProof tab, group-by (also Browse → Help) |
| [`API_KEY.md`](API_KEY.md) | Players | ArenaNet API key scopes + Nexus Options paste (also Browse → Help) |
| [`BUILD.md`](BUILD.md) | Contributors | MinGW cross-compile, `make install`, sites codegen, **CI** (`make ci` / GitHub Actions) |
| `CODE_AUDIT.md` *(gitignored)* | Local | Working audit notes — findings, regression checklist |
| `SNOWCROWS.md` *(gitignored)* | Local | Brief for Snow Crows (not published) |

---

## Releases and listings

| Doc | Purpose |
|-----|---------|
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Full changelog (current: **2.1.0.2**) |
| [`RELEASE_NOTES_CEF_RUNTIME.md`](RELEASE_NOTES_CEF_RUNTIME.md) | GitHub body for tag `cef-runtime-150` (zip asset) |
| [`description.html`](description.html) | HTML listing for web / Nexus description paste |
| `RAIDCORE.md` *(gitignored)* | Local Nexus listing draft |
| `DISCORD.md` / `DISCORD_*.md` *(gitignored)* | Local Discord announcement drafts |

---

## Catalog

| Doc | Purpose |
|-----|---------|
| [`CATALOG.md`](CATALOG.md) | Browse outline by category / section |
| `data/sites.json` | **Canonical** registry (codegen → `src/Sites.gen.cpp`) |

After editing sites or Browse section maps:

```bash
make gen-sites
make validate-sites
make check-sites
```

Browse subsection headers live in `src/UI_Browse.cpp` (`BrowseSection` / `BrowseSectionsForCategory`).

---

## Media

| Path | Use |
|------|-----|
| `docs/media/cover.png` | README / store cover |
| `docs/media/logo.png` | Branding |
| `docs/media/home-logo.png` / `home-cover.jpg` | Embedded into homepage at build |

---

## Version stamp checklist (when shipping)

Bump **only when asked**. Keep these aligned:

1. `src/entry.cpp` — `G::AddonDef.Version` (Major / Minor / Build / Revision)
2. `src/WikiBrowser.cpp` — `kHelperStamp`
3. `src/HomePage.cpp` — `kHomePageVersion`
4. `README.md`, `RELEASE_NOTES.md`, `description.html`; local (gitignored) `RAIDCORE.md`, `DISCORD.md`
5. Refresh `ARCHITECTURE.md` / `WHITEPAPER.md` appendices if constants changed
6. `make && make install` (or release package)

---

## What Browse entries are

Browse rows are **labeled hyperlinks** (`SiteDef`): id + label + URL. Canonical data is `data/sites.json`. The helper navigates CEF to that URL. There is no private site API integration for MetaBattle, Guildjen, or GW2.app — only deep links plus CEF polish (CSS downlevel, YouTube cards, login tips).

---

## Documentation quality bar

Public engineering docs should remain **report-grade**: explicit revision, reproducible commands, normative vs descriptive separation (compliance vs architecture vs whitepaper), and update-in-lockstep with behavioral changes. They are not peer-reviewed academic journal articles; they are project technical reports suitable for onboarding and external review.
