# Contributing to GW2 In-Game Helper

**Audience:** engineers extending or maintaining the addon.  
**Normative companions:** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), [`docs/WHITEPAPER.md`](docs/WHITEPAPER.md), [`docs/COMPLIANCE.md`](docs/COMPLIANCE.md), [`docs/BUILD.md`](docs/BUILD.md).

This guide states how a second maintainer can change the codebase **without** silently breaking the CEF/IPC kernel, Browse catalog integrity, or Nexus input ownership.

---

## 1. Scope and channels

| Channel | Branch | `ADDON_NAME` / install |
|---------|--------|-------------------------|
| Shipping | `master` | `GW2-InGame-Helper` → `addons/GW2-InGame-Helper.dll` |
| Experimental | `GW2-InGame-Helper-Beta` | `GW2-InGame-Helper-Beta` → Beta DLL + data folder |

Do **not** enable both DLLs under Nexus at once (identical signature `HELP`). Prefer feature work on Beta; merge to `master` only after in-game verification.

---

## 2. Ownership zones

| Zone | Casual edits | Extra rules |
|------|--------------|-------------|
| Pads (`*Pad.cpp`, `*Data.cpp`) | Yes | Prefer in-window chips/radios over ImGui popup combos |
| Catalog (`data/sites.json`) | Yes | `make gen-sites validate-sites` required |
| LivePanels / CheatSheets HTML builders | Careful | No secrets in HTML; bump panel/sheet versions when caching |
| `WikiBrowser` / `WikiIpc` / `helper/*` | **Restricted** | Stamp bump + coordinated DLL+helper; pair review |
| `entry.cpp` WndProc / mouse capture | **Restricted** | Autorun / focus regressions — test in GW2 |
| Tekkit parsers / LogManager parse | Careful | Prefer golden fixtures when changing formats |

---

## 3. Build and validation

```bash
git submodule update --init --recursive
make -j"$(nproc)"
make validate-sites
make check-sites          # gen output must match JSON
make test-css             # CssCompat color-mix downlevel
make ci                   # full local gate (sites + CSS + MinGW smoke build)
```

This project does **not** use GitHub Actions. Quality gates are **local**:

```bash
make ci
# optional — block pushes that fail CI:
git config core.hooksPath .githooks
```

Full Guild Wars 2 **restart** is required after DLL updates (`AF_DisableHotloading`).

Catalog workflow:

1. Edit `data/sites.json` (contiguous categories; unique ids).
2. `make gen-sites validate-sites`.
3. Commit **both** `data/sites.json` and `src/Sites.gen.cpp`.
4. Map Browse subsection headers in `src/UI_Browse.cpp` when adding sectioned categories.

---

## 4. Pull-request checklist

- [ ] Ran `make ci` (or at least `validate-sites` / `check-sites` for catalog edits).
- [ ] Branch is intentional (`master` vs Beta).
- [ ] Touched Sites? Ran `make validate-sites` / `check-sites`.
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
