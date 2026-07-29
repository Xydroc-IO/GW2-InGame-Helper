# Documentation index

GW2 In-Game Helper — where to find what.

- **Shipping revision:** **2.0.2.0** (`GW2-InGame-Helper.dll`, private CEF 150) —
  see [`RELEASE_NOTES.md`](RELEASE_NOTES.md)

---

## Start here

| Doc | Audience | Contents |
|-----|----------|----------|
| [`../README.md`](../README.md) | Everyone | Install, features, site list, troubleshooting, build pointers |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors | Process model, IPC, present/input, CEF, Browse, stamps |
| `CODE_AUDIT.md` *(gitignored)* | Local audit notes — findings, risks, regression checklist |
| [`COMPLIANCE.md`](COMPLIANCE.md) | Contributors | Allowed / forbidden Nexus and TOS boundaries |
| [`SNOWCROWS.md`](SNOWCROWS.md) | External (Snow Crows) | Brief for site owners — what the addon is / is not |
| [`CEF_RUNTIME.md`](CEF_RUNTIME.md) | Contributors | Private CEF 150 first-run download, zip, SHA |
| [`BUILD.md`](BUILD.md) | Contributors | MinGW cross-compile, `make install`, cheat-sheet extract |

---

## Releases and listings

| Doc | Purpose |
|-----|---------|
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Full changelog (current: **2.0.2.0**) |
| [`RELEASE_NOTES_CEF_RUNTIME.md`](RELEASE_NOTES_CEF_RUNTIME.md) | GitHub body for tag `cef-runtime-150` (zip asset) |
| [`description.html`](description.html) | HTML listing for web / Nexus description paste |
| `RAIDCORE.md` *(gitignored)* | Local Nexus listing draft — short description + changelog |
| `DISCORD.md` / `DISCORD_*.md` *(gitignored)* | Local Discord announcement drafts |
| [`RELEASE_NOTES_BETA.md`](RELEASE_NOTES_BETA.md) | Superseded — Beta channel folded into **2.0.2.0** |

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
