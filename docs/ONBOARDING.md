# Onboarding — first week

**Goal:** a new maintainer can build, validate, ship a pad-level change, and know what **not** to touch without a second opinion.

**Companions:** [`../CONTRIBUTING.md`](../CONTRIBUTING.md) · [`DOCUMENTATION.md`](DOCUMENTATION.md) · [`ARCHITECTURE.md`](ARCHITECTURE.md) · [`COMPLIANCE.md`](COMPLIANCE.md) · [`BUILD.md`](BUILD.md) · [`MODULES.md`](MODULES.md)

This guide assumes the **`master`** branch (shipping: `GW2-InGame-Helper.dll`, signature `HELP`). Experimental **`GW2-InGame-Helper-Beta`** may exist (`HELB`, separate data folder).

---

## Reading order (day 0)

| Order | Doc | Why |
|-------|-----|-----|
| 1 | [`COMPLIANCE.md`](COMPLIANCE.md) | Hard allow/deny |
| 2 | [`ARCHITECTURE.md`](ARCHITECTURE.md) §§1–6 | Process + IPC + present |
| 3 | [`MODULES.md`](MODULES.md) | Where code lives |
| 4 | [`KERNEL.md`](KERNEL.md) | What not to edit casually |
| 5 | [`WHITEPAPER.md`](WHITEPAPER.md) | Why the design is this way |
| 6 | Domain docs as needed | [`PATHING.md`](PATHING.md), [`ACCOUNT.md`](ACCOUNT.md), [`NAV_AND_ADS.md`](NAV_AND_ADS.md), [`DPS_LOGS.md`](DPS_LOGS.md) |

---

## Day 1 — Build and smoke

1. Clone with submodules; install MinGW ([`BUILD.md`](BUILD.md)).
2. `make ci` — must pass.
3. `make install` (shipping DLL → `addons/GW2-InGame-Helper.dll`).
4. Fully restart Guild Wars 2; open helper (`Ctrl+Shift+H` default).
5. Skim compliance Allowed/Forbidden again after seeing the UI.

**Done when:** DLL loads, Browse opens a wiki page, `make ci` is green.

---

## Day 2 — Catalog and pads

1. Read `data/sites.json` (schema **v2**: `browsePath`, `browseSections`) + `make validate-sites`.
2. Runtime catalog: `addons/GW2-InGame-Helper/sites.json`. Edit + full restart for no-rebuild tweaks; keep `data/sites.json` as release source of truth.
3. Trace one pad: e.g. `src/notes/NotesPad` or Account wallet → `src/ui/` side rail → `Settings` / `PadDock` / `G::PadGeom`.

**Done when:** you can add a Browse entry via JSON and find where a pad is toggled.

---

## Day 3 — Feature modules map

| Area | Start here |
|------|------------|
| Browse UI | `src/ui/UI_Browse*.cpp` |
| DPS Logs | `src/logs/LogManager*` + EI — [`DPS_LOGS.md`](DPS_LOGS.md) |
| Pathing | `src/pathing/` — [`PATHING.md`](PATHING.md) |
| World GPS | `WorldOverlay` / `WorldGps*` |
| Direction compass | `DirectionCompass.cpp` |
| Live digests | `LivePanels*` |
| Account | `src/account/` — [`ACCOUNT.md`](ACCOUNT.md) |
| Pad placement | `src/app/PadDock.h` |

Run parse fixture tests: `make test-parse`.

**Done when:** you know which TU owns parse vs UI for Logs and Pathing.

---

## Day 4 — Restricted kernel (read-only)

Read only; do not change yet without [`KERNEL.md`](KERNEL.md) + paired review:

- `WikiIpc.h` — `make test-ipc`
- `WikiBrowser*` — extract/launch, present, IPC rings
- `HelperNavPolicy*` / `HelperOsrRender*` / helper boot — [`NAV_AND_ADS.md`](NAV_AND_ADS.md)
- `entry*` — WndProc / input ownership

**Done when:** you can name which TU owns present vs nav policy vs launch, and why stamp bumps matter.

---

## Day 5 — Release hygiene

1. Version stamp checklist in [`DOCUMENTATION.md`](DOCUMENTATION.md) (**bump only when asked**).
2. Dry-run: list files for a patch release.
3. Confirm GitHub Actions `CI` matches `make ci`.
4. Remember: `docs/DISCORD*.md` and `RAIDCORE.md` are **gitignored** local drafts.

**Done when:** you can list the files that must stay aligned on a ship.

---

## Escalation

| Change type | Action |
|-------------|--------|
| Pad / Sites / docs | Normal PR; `make ci` |
| Parse formats (EI JSON, `.trl`) | Update golden fixtures in `tools/fixtures/` |
| IPC / helper / present / WndProc / ads | Stop — design note + paired review |
| World GPS device / Present-like hooks | Stop — compliance review ([`PATHING.md`](PATHING.md)) |

If something is unclear, prefer updating these docs in the same PR over tribal knowledge.
