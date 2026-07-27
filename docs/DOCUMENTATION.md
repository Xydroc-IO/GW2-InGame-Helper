# Documentation index

GW2 In-Game Helper — where to find what. Current shipping revision: **2.0.0.21**.

---

## Start here

| Doc | Audience | Contents |
|-----|----------|----------|
| [`../README.md`](../README.md) | Everyone | Install, features, site list, troubleshooting, build pointers |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors | Process model, IPC, present/input, CEF, Browse, stamps |
| [`CODE_AUDIT.md`](CODE_AUDIT.md) | Contributors / reviewers | Findings, risks, regression checklist, source map |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Contributors | Allowed / forbidden Nexus and TOS boundaries |
| [`BUILD.md`](BUILD.md) | Contributors | MinGW cross-compile, `make install`, cheat-sheet extract |

---

## Releases and listings

| Doc | Purpose |
|-----|---------|
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Full changelog |
| [`RAIDCORE.md`](RAIDCORE.md) | Short description + Nexus listing changelog |
| [`description.html`](description.html) | HTML listing for web / Nexus description paste |
| [`DISCORD.md`](DISCORD.md) | Current player Discord announcement |
| [`DISCORD_2.0.0.N.md`](DISCORD_2.0.0.21.md) | Per-version Discord posts (keep latest + history) |

---

## Catalog

| Doc | Purpose |
|-----|---------|
| [`CATALOG.md`](CATALOG.md) | Browse outline by category / section (source of truth remains `src/Sites.cpp`) |

After editing sites or Browse section maps:

```bash
make validate-sites
```

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

1. `src/entry.cpp` — `G::AddonDef.Version.Revision`
2. `src/WikiBrowser.cpp` — `kHelperStamp`
3. `src/HomePage.cpp` — `kHomePageVersion`
4. `README.md`, `RELEASE_NOTES.md`, `RAIDCORE.md`, `description.html`, `DISCORD.md`, new `DISCORD_2.0.0.N.md`
5. Refresh `ARCHITECTURE.md` / `CODE_AUDIT.md` headers if behavior changed
6. `make && make install` (or release package)

---

## What Browse entries are

Browse rows are **labeled hyperlinks** (`SiteDef` in `Sites.cpp`): id + label + URL. The helper navigates CEF to that URL. There is no private site API integration for MetaBattle, Guildjen, or GW2.app — only deep links plus CEF polish (CSS downlevel, YouTube cards, login tips).
