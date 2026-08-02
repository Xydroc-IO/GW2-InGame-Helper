# Building GW2 In-Game Helper

## Prerequisites

- Git
- Make
- MinGW-w64 C++ toolchain (`x86_64-w64-mingw32-g++`, `x86_64-w64-mingw32-gcc`)
- `curl`, `tar`, `zip`, `sha256sum` (for `make pack-cef`)

### Arch / Manjaro

```bash
sudo pacman -S --needed mingw-w64-gcc make git zip curl
```

### Debian / Ubuntu

```bash
sudo apt install -y mingw-w64 make git zip curl
```

## Build

```bash
cd GW2-InGame-Helper
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

`make install` refreshes helper/HTML stamps but **keeps** `settings.ini` and the
private `cef/` runtime tree.

### Pack private CEF runtime zip

```bash
make pack-cef
```

Writes `build/cef-runtime/cef-runtime-150-windows64.zip` + `SHA256SUMS`.
Upload the zip to a GitHub Release, then set `kDownloadUrl` / `kSha256Hex` in
[`src/CefRuntime.h`](../src/CefRuntime.h).

### Clean

```bash
make clean
```

## What gets compiled

| Target | Sources |
|--------|---------|
| `GW2HelperBrowser.exe` | `src/helper/*.cpp` against `deps/cef` **150** headers |
| Host DLL | `src/*.cpp` + Dear ImGui + miniz + embedded helper blob (+ `Sites.gen.cpp`) |

### Browse catalog (data → C++)

```bash
# edit data/sites.json, then:
make gen-sites
make validate-sites
make check-sites
```

Canonical catalog is `data/sites.json`. Do not hand-edit `src/Sites.gen.cpp`.

Player install layout:

```text
addons/GW2-InGame-Helper.dll   # only file players copy (shipping)
addons/GW2-InGame-Helper/      # runtime data + cef/ after first open
```

On the Beta branch, install names use `GW2-InGame-Helper-Beta` (see [`../CONTRIBUTING.md`](../CONTRIBUTING.md)).

Runtime CEF is **not** embedded in the DLL. First helper open downloads it into
the addon data folder. Do **not** use or write game `bin64/cef`.

Further reading: [`ARCHITECTURE.md`](ARCHITECTURE.md), [`WHITEPAPER.md`](WHITEPAPER.md).
