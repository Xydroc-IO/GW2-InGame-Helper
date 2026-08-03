# Private CEF runtime — DLL-owned setup

Players only install **`GW2-InGame-Helper.dll`**. On first helper open the DLL
sets up everything under `addons/GW2-InGame-Helper/`:

1. Creates the addon data folder (if needed)
2. Installs private CEF into `cef/` (see below)
3. Writes `cef/cef.ver` (`150.0.14`)
4. Extracts embedded `GW2HelperBrowser.exe`
5. Launches the helper with `--cef-dir=.../cef`

Never writes into game `bin64/cef`.

## How CEF is obtained (in order)

1. **Already installed** — `cef/libcef.dll` + resources + matching `cef.ver` → skip
2. **Complete tree, missing stamp** — write `cef.ver` and continue
3. **Local zip** (verified SHA-256, then extract):
   - `addons/GW2-InGame-Helper/cef-runtime-150-windows64.zip`
   - `addons/cef-runtime-150-windows64.zip` (next to the DLL)
   - `addons/.../cef/cef-runtime-150-windows64.zip` (cleaned out of `cef/` after)
4. **HTTPS download** from `CefRuntime.h` → `kDownloadUrl`
   (currently temporary: release tag `1.0.0.0` / `cef-runtime-150-windows64.zip`)

## Pack / publish the zip

```bash
make pack-cef
```

Upload `build/cef-runtime/cef-runtime-150-windows64.zip` to a GitHub Release and keep
`kDownloadUrl` / `kSha256Hex` in `src/browser/CefRuntime.h` in sync.

## Local offline test (no GitHub)

```bash
cp build/cef-runtime/cef-runtime-150-windows64.zip \
  "<GW2>/addons/cef-runtime-150-windows64.zip"
# or into addons/GW2-InGame-Helper/
```

Open the helper once — the DLL verifies, extracts to `cef/`, stamps, and launches.
