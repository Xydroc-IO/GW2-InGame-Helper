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

### Continuous integration

**GitHub Actions** (public repo — free on standard runners) runs `make ci` on
push/PR to `master` and `GW2-InGame-Helper-Beta` (`.github/workflows/ci.yml`).

Locally:

```bash
make ci
```

Runs sites validate/check, CSS tests, **parse golden fixtures**, and a MinGW smoke build.
Optional push gate (once per clone):

```bash
git config core.hooksPath .githooks
```

## What gets compiled

| Target | Sources |
|--------|---------|
| `GW2HelperBrowser.exe` | `src/helper/*.cpp` against `deps/cef` **150** headers |
| Host DLL | `src/*.cpp` + Dear ImGui + miniz + embedded helper blob + `sites.json` + cheatsheets zip |

### Browse catalog (runtime JSON)

```bash
# edit data/sites.json (schema v2), then:
make validate-sites
make   # embeds catalog into the DLL
```

Canonical catalog is `data/sites.json`. At runtime it is extracted to
`addons/<addon-name>/sites.json` (edit there for no-rebuild tweaks; restart GW2).

### Offline cheat sheets

```bash
# edit HTML/CSS under data/cheatsheets/, then:
make   # packs + embeds build/cheatsheets.zip
```

Runtime extract: `addons/<addon-name>/cheatsheets/` (`manifest.json`, `shared.css`, `*.html`).

Player install layout (shipping):

```text
addons/GW2-InGame-Helper.dll   # only file players copy
addons/GW2-InGame-Helper/      # runtime data + sites.json + cef/ after first open
```

The experimental `GW2-InGame-Helper-Beta` branch uses a parallel DLL/folder name; see [`../CONTRIBUTING.md`](../CONTRIBUTING.md).

Runtime CEF is **not** embedded in the DLL. First helper open downloads it into
the addon data folder. Do **not** use or write game `bin64/cef`.

Further reading: [`ARCHITECTURE.md`](ARCHITECTURE.md), [`WHITEPAPER.md`](WHITEPAPER.md).
