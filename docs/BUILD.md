# Building GW2 In-Game Helper

## Prerequisites

- Git (with submodule support)
- Make
- MinGW-w64 C++ toolchain (`x86_64-w64-mingw32-g++`)

### Arch / Manjaro

```bash
sudo pacman -S --needed mingw-w64-gcc make git
```

### Debian / Ubuntu

```bash
sudo apt install -y mingw-w64 make git
```

## Clone

```bash
git clone --recurse-submodules <this-repo-url>
cd GW2-InGame-Helper
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

## Build

```bash
make -j"$(nproc)"
```

Output DLL:

```text
build/bin/GW2-InGame-Helper.dll
```

`GW2HelperBrowser.exe` is built and embedded into the DLL automatically.

### Install into a local Guild Wars 2 tree

```bash
make install
# or:
make install GW2_ROOT="/path/to/Guild Wars 2"
```

### Clean

```bash
make clean
```

## CMake (optional)

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
cmake --build build -j"$(nproc)"
```

## What gets compiled

| Target | Sources |
|--------|---------|
| `GW2HelperBrowser.exe` | `src/helper/*.cpp` |
| `GW2-InGame-Helper.dll` | `src/*.cpp` + Dear ImGui + embedded helper blob + homepage logo/cover |

The homepage (`HomePage.cpp`) embeds `docs/media/home-logo.png` and `docs/media/home-cover.jpg`
into the DLL; they are written next to `helper-home.html` under `addons/GW2-InGame-Helper/` on first open.

Built-in **Cheat Sheets** are compiled in and written on first open:

| Sheet | Module | Files under addon data |
|-------|--------|------------------------|
| Raid Food | `src/RaidFood.cpp` | `raid-food.html` / `.ver` |
| Hubs (Raid Prep, Squad Utility, Encounters, Fractals) + Uber's All-In-One, Raid Utilities, Fractal Consumables, Fractal CM / T4, Sigils & Runes, Relics, Boon Checklist, Squad Template, Stability / Cleanse, CC / Defiance, Raid Wings, Strike Missions, Material Conversions, Legendary Paths, Mount Unlock, Home Garden | `src/CheatSheets.cpp` | matching `*.html` / `*.ver` (hubs write child sheets first) |

Uber's All-In-One waypoint list is curated by **uberduber.1249**.

Runtime CEF libraries are **not** shipped — the helper loads them from the game install at `bin64/cef/`.

Player install layout:

```text
addons/GW2-InGame-Helper.dll     # only file players copy
addons/GW2-InGame-Helper/        # runtime data directory
```

## Dependencies

| Path | Role |
|------|------|
| `deps/nexus` | Raidcore Nexus API headers (submodule) |
| `deps/imgui` | Dear ImGui (submodule) |
| `deps/cef` | CEF C++ headers only (vendored; runtime from game) |

See the root [README](../README.md) for install, usage, and policy notes.
