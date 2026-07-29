# CEF Runtime 150 — GitHub Release notes

Use this body for the GitHub Release whose **tag** is exactly `cef-runtime-150`.

Upload asset: **`cef-runtime-150-windows64.zip`** only (the addon DLL ships on a separate release).

---

## CEF Runtime 150 (for GW2 In-Game Helper)

Private Chromium runtime used by **`GW2-InGame-Helper`** (v2.0.2.0+).

### Asset

| File | Notes |
|------|--------|
| `cef-runtime-150-windows64.zip` | ~167 MB · Windows x64 |

**SHA-256:** `d08859aa99266566f5ba51be4cacc7ec57265bcc4b84436151410553c7d82943`

### Versions

- **CEF:** 150.0.14
- **Chromium:** 150.0.7871.129
- **Stamp:** `cef.ver` → `150.0.14`

### How players get it

1. Install `GW2-InGame-Helper.dll` into `<GW2>/addons/`
2. Open the helper once
3. The DLL downloads this zip (if no local copy), verifies SHA-256, extracts to `addons/GW2-InGame-Helper/cef/`

**Manual / offline:** put `cef-runtime-150-windows64.zip` next to the DLL or under `addons/GW2-InGame-Helper/`, then open the helper.

### Important

- Do **not** unpack this into Guild Wars 2 `bin64/cef`
- Filename and tag should stay `cef-runtime-150` / `cef-runtime-150-windows64.zip` (download URL in `src/CefRuntime.h`)

### Download URL (temporary test host)

https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/1.0.0.0/cef-runtime-150-windows64.zip

When done testing, move the asset to tag `cef-runtime-150` and update `kDownloadUrl` in `src/CefRuntime.h` to match.

### Pack locally

```bash
make pack-cef
```

See also [`CEF_RUNTIME.md`](CEF_RUNTIME.md) and [`RELEASE_NOTES.md`](RELEASE_NOTES.md).
