# Documentation index

GW2 In-Game Helper — where to find what.

- **Stable shipping revision:** **2.0.1.1** (`GW2-InGame-Helper.dll`, game `bin64/cef`)
- **Beta channel:** **2.0.1.1-beta** (`GW2-InGame-Helper-Beta.dll`, private CEF 150) —
  see [`RELEASE_NOTES_BETA.md`](RELEASE_NOTES_BETA.md)

---

## Start here

| Doc | Audience | Contents |
|-----|----------|----------|
| [`../README.md`](../README.md) | Everyone | Install, features, site list, troubleshooting, build pointers |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors | Process model, IPC, present/input, CEF, Browse, stamps |
| `CODE_AUDIT.md` *(gitignored)* | Local audit notes — findings, risks, regression checklist |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Contributors | Allowed / forbidden Nexus and TOS boundaries |
| [`SNOWCROWS.md`](SNOWCROWS.md) | External (Snow Crows) | Brief for site owners — what the addon is / is not |
| [`CEF_RUNTIME.md`](CEF_RUNTIME.md) | Contributors / Beta | Private CEF 150 first-run download, zip, SHA |
| [`BUILD.md`](BUILD.md) | Contributors | MinGW cross-compile, `make install`, cheat-sheet extract |

---

## Releases and listings

| Doc | Purpose |
|-----|---------|
| [`RELEASE_NOTES_BETA.md`](RELEASE_NOTES_BETA.md) | **Beta** private-CEF channel notes |
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Stable full changelog |
| [`description.html`](description.html) | HTML listing for web / Nexus description paste |
| `RAIDCORE.md` *(gitignored)* | Local Nexus listing draft — short description + changelog |
| `DISCORD.md` / `DISCORD_*.md` *(gitignored)* | Local Discord announcement drafts |

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

1. `src/entry.cpp` — `G::AddonDef.Version` (Major / Minor / Build / Revision)
2. `src/WikiBrowser.cpp` — `kHelperStamp`
3. `src/HomePage.cpp` — `kHomePageVersion`
4. `README.md`, `RELEASE_NOTES.md`, `description.html`; local (gitignored) `RAIDCORE.md`, `DISCORD.md`, new `DISCORD_*.md`
5. Refresh `ARCHITECTURE.md` headers if behavior changed; update local `CODE_AUDIT.md` if you keep one
6. `make && make install` (or release package)

---

## What Browse entries are

Browse rows are **labeled hyperlinks** (`SiteDef` in `Sites.cpp`): id + label + URL. The helper navigates CEF to that URL. There is no private site API integration for MetaBattle, Guildjen, or GW2.app — only deep links plus CEF polish (CSS downlevel, YouTube cards, login tips).
