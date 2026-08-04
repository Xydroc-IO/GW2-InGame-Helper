# Private CEF runtime — DLL-owned setup

**Revision:** 2.2.0.3 · CEF Stable **150.0.14** / Chromium **150.0.7871.129**  
**Companions:** [`ARCHITECTURE.md`](ARCHITECTURE.md), [`BUILD.md`](BUILD.md), [`WHITEPAPER.md`](WHITEPAPER.md), [`CefRuntime.h`](../src/browser/CefRuntime.h)

Players only install **`GW2-InGame-Helper.dll`**. On first helper open the DLL sets up everything under `addons/GW2-InGame-Helper/`. **Never** writes into game `bin64/cef`.

---

## 1. First-open sequence

1. Create the addon data folder (if needed).
2. Install private CEF into `cef/` (see acquisition order below).
3. Write `cef/cef.ver` (`150.0.14`).
4. Extract embedded `GW2HelperBrowser.exe` (+ helper `.ver` stamp).
5. Launch the helper with `--cef-dir=.../cef` and `--host-pid=<GW2 PID>`.

Chromium profile/cache: `%LOCALAPPDATA%/GW2-InGame-Helper/cef-cache/` (not under `addons/`).

---

## 2. How CEF is obtained (in order)

1. **Already installed** — `cef/libcef.dll` + resources + matching `cef.ver` → skip.
2. **Complete tree, missing stamp** — write `cef.ver` and continue.
3. **Local zip** (SHA-256 verify, then extract), searched as:
   - `addons/GW2-InGame-Helper/cef-runtime-150-windows64.zip`
   - `addons/cef-runtime-150-windows64.zip` (next to the DLL)
   - `addons/.../cef/cef-runtime-150-windows64.zip` (cleaned out of `cef/` after)
4. **HTTPS download** from `CefRuntime.h` → `kDownloadUrl` (must match `kSha256Hex`).

On failure, Browse cannot start; user-visible status should indicate install/verify problems.

---

## 3. Pack / publish the zip

```bash
make pack-cef
```

`scripts/pack-cef-runtime.sh` downloads the official minimal Windows x64 package, flattens `Release/` + `Resources/`, omits unused bootstrap binaries, and writes:

```text
build/cef-runtime/cef-runtime-150-windows64.zip
build/cef-runtime/SHA256SUMS
```

Upload the zip to a GitHub Release (see also [`RELEASE_NOTES_CEF_RUNTIME.md`](RELEASE_NOTES_CEF_RUNTIME.md) for tag `cef-runtime-150`). Keep `kDownloadUrl` / `kSha256Hex` in [`src/browser/CefRuntime.h`](../src/browser/CefRuntime.h) in sync.

---

## 4. Local offline test (no GitHub)

```bash
cp build/cef-runtime/cef-runtime-150-windows64.zip \
  "<GW2>/addons/cef-runtime-150-windows64.zip"
# or into addons/GW2-InGame-Helper/
```

Open the helper once — the DLL verifies, extracts to `cef/`, stamps, and launches.

---

## 5. Troubleshooting

| Symptom | Check |
|---------|-------|
| Re-downloads every launch | `cef.ver` mismatch or incomplete tree |
| SHA failure | Corrupt zip; URL/hash drift in `CefRuntime.h` |
| Helper exits immediately | Missing `libcef.dll` / locales; Wine file lock on old EXE |
| AV quarantine | Unsigned tree under `addons/` — restore + exclude carefully |
| Proton instability | Expected software OSR flags; do not “enable GPU” casually |

---

## 6. Policy

- Stock CEF only — no forked Chromium in shipping builds.
- No proprietary codec redistribution via stock Spotify builds (video → Open Ext).
- Beta uses the same CEF mechanism under `GW2-InGame-Helper-Beta/` when that channel is installed.
