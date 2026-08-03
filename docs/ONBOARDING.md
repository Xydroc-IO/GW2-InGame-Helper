# Onboarding — first week

**Goal:** a new maintainer can build, validate, ship a pad-level change, and know what **not** to touch without a second opinion.

Companion: [`../CONTRIBUTING.md`](../CONTRIBUTING.md) · [`ARCHITECTURE.md`](ARCHITECTURE.md) · [`COMPLIANCE.md`](COMPLIANCE.md) · [`BUILD.md`](BUILD.md).

This guide assumes the **`master`** branch (shipping install: `GW2-InGame-Helper.dll`). An experimental **`GW2-InGame-Helper-Beta`** branch may exist with a separate `ADDON_NAME` / data folder and Nexus signature `HELB` (shipping is `HELP`).

---

## Day 1 — Build and smoke

1. Clone with submodules; install MinGW (`docs/BUILD.md`).
2. `make ci` — must pass (sites, CSS, parse fixtures, MinGW smoke).
3. `make install` (shipping DLL → `addons/GW2-InGame-Helper.dll`).
4. Fully restart Guild Wars 2; open helper (`Ctrl+Shift+H` default).
5. Skim [`ARCHITECTURE.md`](ARCHITECTURE.md) §§1–5 and [`COMPLIANCE.md`](COMPLIANCE.md) Allowed/Forbidden.

**Done when:** DLL loads, Browse opens a wiki page, `make ci` is green.

---

## Day 2 — Catalog and pads

1. Read `data/sites.json` (schema **v2**: `browsePath`, `browseSections`) + `make validate-sites`.
2. Runtime catalog lives in `addons/GW2-InGame-Helper/sites.json` (extracted from the DLL). Edit that file and fully restart GW2 to change Browse without rebuilding; keep `data/sites.json` in git as the source of truth for releases.
3. Trace one pad: e.g. `src/notes/NotesPad` or `src/account/WalletPad` → `src/ui/UI.cpp` helper side rail → settings flag in `src/app/Globals.h` / `Settings.cpp`. Pad positions persist via `PadDock` / `G::PadGeom`.

**Done when:** you can add a Browse entry via JSON and find where a pad is toggled.

---

## Day 3 — Feature modules map

| Area | Start here |
|------|------------|
| Browse UI | `src/ui/UI_Browse.cpp` |
| DPS Logs | `src/logs/LogManagerPad.cpp` (defs) + Shared → Cache / KillProof / Scan / Stats / Ui / Parse / Upload / Ei |
| Pathing | `src/pathing/PathingTrails.cpp` + `PathingIndex.h` → Load / Gps / Presets / Ui / Parse / Index / PathingPacks |
| Direction compass | `src/pathing/DirectionCompass.cpp` (side-rail **Compass** pad) |
| Live digests | `src/browse/LivePanels.cpp` → BuildCommon / Dailies / News / Fashion / Progress |
| Account API pads | `src/account/AccountPad.cpp`, `ProgressData.cpp`, `CraftingData.cpp` (+ Api/Wiki/Plan/Dailies); TP/Wallet → Data/Fetch TUs |
| Pad placement | `src/app/PadDock.h` (`G::PadGeom` → `settings.ini`) |

Run parse fixture tests: `make test-parse`.

**Done when:** you know which TU owns parse vs UI for Logs and Pathing.

---

## Day 4 — Restricted kernel (read-only)

Read only; do not change yet without [`KERNEL.md`](KERNEL.md):

- `src/browser/WikiIpc.h` — packed IPC contract (`HLI5`); `make test-ipc`
- `src/browser/WikiBrowser*.cpp` — extract/launch (`Helper`), present (`Present`), IPC rings (`Ipc`)
- `src/helper/HelperNavPolicy.cpp` / `HelperOsrRender.cpp` / `main.cpp` — policy, OSR, boot
- `src/entry.cpp` — WndProc / input ownership

**Done when:** you can name which TU owns present vs nav policy vs launch, and why stamp bumps matter.

---

## Day 5 — Release hygiene

1. Version stamp checklist in [`DOCUMENTATION.md`](DOCUMENTATION.md).
2. Practice a dry-run: what files change for a patch release (no bump unless asked).
3. Confirm GitHub Actions `CI` workflow matches `make ci`.

**Done when:** you can list the files that must stay aligned on a ship.

---

## Escalation

| Change type | Action |
|-------------|--------|
| Pad / Sites / docs | Normal PR; `make ci` |
| Parse formats (EI JSON, `.trl`) | Update golden fixtures in `tools/fixtures/` |
| IPC / helper / present / WndProc | Stop — design note + paired review |

If something is unclear, prefer updating these docs in the same PR over tribal knowledge.
