# Building GW2 In-Game Helper

**Revision:** 2.2.0.3 · **Companions:** [`ARCHITECTURE.md`](ARCHITECTURE.md), [`CEF_RUNTIME.md`](CEF_RUNTIME.md), [`MODULES.md`](MODULES.md), [`../CONTRIBUTING.md`](../CONTRIBUTING.md)

---

## Prerequisites

- Git (submodules as required by the repo)
- Make
- MinGW-w64 C++ toolchain (`x86_64-w64-mingw32-g++`, `x86_64-w64-mingw32-gcc`)
- `curl`, `tar`, `zip`, `sha256sum` (for `make pack-cef`)
- Python 3 (sites validate / enrich / some tests)

### Arch / Manjaro

```bash
sudo pacman -S --needed mingw-w64-gcc make git zip curl python
```

### Debian / Ubuntu

```bash
sudo apt install -y mingw-w64 make git zip curl python3
```

---

## Build

```bash
cd GW2-InGame-Helper
make -j"$(nproc)"
```

Output DLL:

```text
build/bin/GW2-InGame-Helper.dll
```

`GW2HelperBrowser.exe` is built and embedded into the DLL automatically. Hybrid `src/**/*.cpp` layout — see [`MODULES.md`](MODULES.md).

### Install into a local Guild Wars 2 tree

```bash
make install
# or:
make install GW2_ROOT="/path/to/Guild Wars 2"
```

`make install` refreshes helper/HTML stamps but **keeps** `settings.ini` and the private `cef/` runtime tree.

### Pack private CEF runtime zip

```bash
make pack-cef
```

Writes `build/cef-runtime/cef-runtime-150-windows64.zip` + `SHA256SUMS`.  
Upload, then sync `kDownloadUrl` / `kSha256Hex` in [`src/browser/CefRuntime.h`](../src/browser/CefRuntime.h). Details: [`CEF_RUNTIME.md`](CEF_RUNTIME.md).

### Clean

```bash
make clean
```

---

## Continuous integration

**GitHub Actions** runs `make ci` on push/PR to `master` and `GW2-InGame-Helper-Beta` (`.github/workflows/ci.yml`).

Locally:

```bash
make ci
```

Typical gates: sites validate/check, CSS tests, **parse golden fixtures**, IPC sizeof/magic smoke, MinGW smoke build.

Optional push gate (once per clone):

```bash
git config core.hooksPath .githooks
```

Useful singles:

| Target | Purpose |
|--------|---------|
| `make validate-sites` | Catalog integrity |
| `make enrich-sites` | Optional browsePath derivation |
| `make test-parse` | LogManager / EI JSON fixtures |
| `make test-ipc` | Packed IPC layout constants |

---

## What gets compiled

| Target | Sources |
|--------|---------|
| `GW2HelperBrowser.exe` | `src/helper/*.cpp` against `deps/cef` **150** headers |
| Host DLL | `src/**/*.cpp` (hybrid layers + domains) + Dear ImGui + miniz + embedded helper blob + `sites.json` + cheatsheets zip |

When adding a `.cpp`, update **both** Makefile and CMakeLists if both are maintained.

### Browse catalog (runtime JSON)

```bash
# edit data/sites.json (schema v2), then:
make validate-sites
make   # embeds catalog into the DLL
```

Canonical catalog: `data/sites.json`. Runtime extract: `addons/<addon-name>/sites.json` (edit + full GW2 restart for no-rebuild tweaks).

### Offline cheat sheets

```bash
# edit HTML/CSS under data/cheatsheets/, then:
make   # packs + embeds build/cheatsheets.zip
```

Runtime extract: `addons/<addon-name>/cheatsheets/`.

---

## Player install layout (shipping)

```text
addons/GW2-InGame-Helper.dll   # only file players copy
addons/GW2-InGame-Helper/      # runtime data + sites.json + cef/ after first open
```

Experimental `GW2-InGame-Helper-Beta` uses a parallel DLL/folder name; see [`../CONTRIBUTING.md`](../CONTRIBUTING.md).

Runtime CEF is **not** embedded in the DLL. Do **not** use or write game `bin64/cef`.

---

## Common failures

| Failure | Likely cause |
|---------|--------------|
| Missing `x86_64-w64-mingw32-g++` | Install mingw-w64 |
| Link errors after split | New `.cpp` not listed in Makefile/CMake |
| Sites CI red | `make validate-sites` locally |
| Helper not updating in-game | Stamp unchanged or GW2 not fully restarted |
| CMake vs Make drift | Prefer the path CI uses (`make ci`) |

Further reading: [`ARCHITECTURE.md`](ARCHITECTURE.md), [`WHITEPAPER.md`](WHITEPAPER.md), [`KERNEL.md`](KERNEL.md).
