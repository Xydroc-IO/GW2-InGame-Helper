# Module layout — ≤500-line convention

**Revision:** 2.2.0.9 · **Audience:** contributors  
**Companions:** [`ARCHITECTURE.md`](ARCHITECTURE.md) §7, [`CONTRIBUTING.md`](../CONTRIBUTING.md), [`WHITEPAPER.md`](WHITEPAPER.md) §18

---

## 1. Rule

Prefer **≤500 lines** per `.cpp` translation unit. Split by **concern** (pad UI vs fetch vs parse vs state), not arbitrary line chops.

**Exempt:** generated or embedded blob headers (e.g. `BootJs.h`, `QuickAccessIcon.h`), third-party under `deps/`.

---

## 2. Shared / Internal pattern

| Artifact | Role |
|----------|------|
| `Foo.h` | Public API stable for other domains |
| `FooShared.h` | Cross-TU decls + `extern` state for the domain |
| `FooInternal.h` | Helpers visible only inside the domain’s TUs |
| Exactly **one** `.cpp` | Defines Shared globals |

Examples: `WikiBrowserShared.h`, `PathingIndex.h`, `LogManagerShared.h`, `LivePanelsBuildShared.h`, `CraftingShared.h`, `HelperInternal.h`, `JsonView.h`.

---

## 3. Domain ownership (summary)

| Domain | Directory | Notes |
|--------|-----------|-------|
| Kernel host | `src/browser/` | Restricted — [`KERNEL.md`](KERNEL.md) |
| Kernel helper | `src/helper/` | Restricted — ads/nav freeze awareness |
| Entry | `src/entry*.cpp` | Load/unload/WndProc/hotkeys |
| UI chrome | `src/ui/` | Side rail, Browse chrome, **SettingsPad**, Nexus Options stub |
| App shared | `src/app/` | Settings, paths, theme, pad dock, **`PadNav`** (wrap + scrollbar gutter), Mumble, AspectLayout (16:9/21:9/32:9) |
| API HTTP | `src/api/` | WinHTTP workers only; `JsonView.h` shared scrapers |
| Browse data | `src/browse/` | Sites, homepage, live panels, sheets |
| Account | `src/account/` | Official API pads |
| Pathing | `src/pathing/` | Packs + GPS — [`PATHING.md`](PATHING.md) |
| Logs | `src/logs/` | DPS Logs + EI |
| Events | `src/events/` | World Events |
| Notes | `src/notes/` | Notes + waypoints |

Includes are flat (`#include "Foo.h"`) via multiple `-Isrc/...` include paths in the Makefile/CMake.

---

## 4. When splitting a megafile

1. Identify natural seams (HTTP vs parse vs ImGui).
2. Move declarations to Shared/Internal; keep **one** defining TU.
3. Update Makefile / CMakeLists source lists.
4. Keep behavior identical; no drive-by refactors in kernel files.
5. Document new files in [`ARCHITECTURE.md`](ARCHITECTURE.md) if ownership changes.

---

## 5. What this is not

- Not a microservices mandate.
- Not a requirement to extract every 200-line file.
- Not permission to rewrite helper globals for style points ([`WHITEPAPER.md`](WHITEPAPER.md) §18).
