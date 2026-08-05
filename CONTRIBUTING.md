# Contributing to GW2 In-Game Helper

**Audience:** engineers extending or maintaining the addon.  
**Normative companions:** [`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md) (index), [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), [`docs/WHITEPAPER.md`](docs/WHITEPAPER.md), [`docs/COMPLIANCE.md`](docs/COMPLIANCE.md), [`docs/KERNEL.md`](docs/KERNEL.md), [`docs/NAV_AND_ADS.md`](docs/NAV_AND_ADS.md), [`docs/MODULES.md`](docs/MODULES.md), [`docs/BUILD.md`](docs/BUILD.md).

This guide states how a second maintainer can change the codebase **without** silently breaking the CEF/IPC kernel, Browse catalog integrity, or Nexus input ownership.

---

## 1. Scope and channels

| Channel | Branch | `ADDON_NAME` / install |
|---------|--------|-------------------------|
| Shipping | `master` | `GW2-InGame-Helper` → `addons/GW2-InGame-Helper.dll` |
| Experimental | `GW2-InGame-Helper-Beta` | `GW2-InGame-Helper-Beta` → Beta DLL + data folder |

Do **not** enable both DLLs under Nexus at once unless you intentionally want
side-by-side testing (Beta uses signature `HELB`, shipping uses `HELP`). Prefer
feature work on Beta; merge to `master` only after in-game verification.

---

## 2. Ownership zones

| Zone | Path | Casual edits | Extra rules |
|------|------|--------------|-------------|
| Pads / feature data | `src/account/`, `src/pathing/`, `src/logs/`, `src/events/`, `src/notes/` | Yes | Prefer in-window chips/radios over ImGui popup combos; keep new `.cpp` TUs **≤500 lines** (split by concern) |
| Catalog | `data/sites.json` + `src/browse/` | Yes | `make validate-sites` required; schema v2 + `browsePath` |
| Cheat sheets | `data/cheatsheets/` | Yes | Edit HTML/CSS + `manifest.json`; bump pack stamp `c2210` in `src/browse/CheatSheets.cpp` when shipping extract changes |
| LivePanels HTML builders | `src/browse/LivePanels*` | Careful | No secrets in HTML; bump panel version when caching |
| Browser kernel | `src/browser/`, `src/helper/` | **Restricted** | Follow [`docs/KERNEL.md`](docs/KERNEL.md); stamp bump + coordinated DLL+helper; pair review |
| Nexus entry / WndProc | `src/entry.cpp` | **Restricted** | Autorun / focus regressions — test in GW2 |
| Shared chrome / app | `src/ui/`, `src/app/`, `src/api/` | Careful | Side-rail / settings / HTTP touch many pads |
| Pathing / Logs parsers | `src/pathing/PathingParse*`, `src/logs/LogManagerParse*` | Careful | Prefer golden fixtures when changing formats |

---

## 3. Build and validation

```bash
git submodule update --init --recursive
make ci                   # sites + CSS + parse fixtures + MinGW smoke
```

**First week:** follow [`docs/ONBOARDING.md`](docs/ONBOARDING.md).  
**Security reports:** [`SECURITY.md`](SECURITY.md).

This project uses **GitHub Actions** on public PRs/pushes (`CI` workflow) plus an
identical local gate:

```bash
make ci
# optional — also block local pushes that fail CI:
git config core.hooksPath .githooks
```

Prefer a full Guild Wars 2 **restart** after DLL updates. Nexus can Disable/unload the
addon (`AF_None`); unload shuts down the CEF helper first — still restart if a stale
helper remains after a botched replace.

Catalog workflow (runtime JSON):

1. Edit `data/sites.json` (schema v2; contiguous categories; unique ids).
2. Set optional `browsePath` (e.g. `["Raids", "Raid Boss", "W9 …"]`) and keep `browseSections` ordered lists in sync.
3. `make validate-sites`.
4. Commit `data/sites.json`. Rebuild embeds it; installs extract to `addons/GW2-InGame-Helper/sites.json`.
5. For a local no-rebuild tweak: edit the extracted file under the addon data folder, then fully restart GW2.

Parse fixtures (update when changing EI/dps.report JSON or `.trl` layout):

```bash
make test-parse
# fixtures: tools/fixtures/*.json · tools/test_trl_parse.py
```

---

## 4. Pull-request checklist

- [ ] Ran `make ci` (or at least `validate-sites` for catalog edits).
- [ ] Branch is intentional (`master` vs Beta).
- [ ] Touched Sites? Ran `make validate-sites`.
- [ ] Touched helper or IPC? Bumped `kHelperStamp` (and home stamp if needed); both sides of `WikiIpc.h` agree.
- [ ] Touched input / WndProc / pad hover capture? Tested in-client (keys, mouse, autorun).
- [ ] No game memory, Present hooks, or writes into `bin64/cef` ([`COMPLIANCE.md`](docs/COMPLIANCE.md)).
- [ ] Version bump **only** when releasing (entry + stamps + RELEASE_NOTES).

---

## 5. Documentation standards

Public technical docs aim for **report quality**: dated revision, explicit assumptions, reproducible commands, and a clear separation of *normative* policy ([`COMPLIANCE.md`](docs/COMPLIANCE.md)) from *descriptive* architecture ([`ARCHITECTURE.md`](docs/ARCHITECTURE.md)) and *argumentative* design rationale ([`WHITEPAPER.md`](docs/WHITEPAPER.md)).

When changing behavior in a documented subsystem, update the corresponding document in the same commit series.

---

## 6. What still requires a human in GW2

Automated gates cannot fully replace in-client checks for: CEF major upgrades, OSR paint races, Wine/Proton-only failures, focus/mouse ownership, and third-party site redesigns that break BootJs.

---

## 7. License

Contributions are accepted under the project MIT license (`LICENSE`).
