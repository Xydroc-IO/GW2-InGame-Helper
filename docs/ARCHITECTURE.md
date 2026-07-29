# Architecture — GW2 In-Game Helper

**Current addon revision:** `2.0.2.2` · **IPC:** `HLI5` (v5) · **CEF:** private Stable **150** (`150.0.7871.129`)

Never loads game CEF and never writes into `bin64/cef`.

---

## 1. One-line summary

A **Raidcore Nexus** ImGui DLL opens an **out-of-process CEF off-screen (OSR)** browser that paints BGRA frames into **PID-scoped shared memory**. The DLL uploads those frames to a D3D11 texture (via Nexus `SwapChain`) and draws them with ImGui. Players ship **one DLL**; the helper EXE and homepage assets are embedded and extracted on first use. Chromium is a **private CEF 150** tree downloaded once into `addons/GW2-InGame-Helper/cef/`.

---

## 2. Process model

```text
Guild Wars 2.exe
 └─ Nexus loads GW2-InGame-Helper.dll
      ├─ RT_Render → UI_Render → WikiBrowser::PresentFrame / Tick
      ├─ WndProc → keyboard/mouse routing (CEF vs ImGui vs game)
      └─ CreateProcess → GW2HelperBrowser.exe
           └─ LoadLibrary(addons/.../cef/libcef.dll) — windowless OSR
```

| Component | Role |
|-----------|------|
| `GW2-InGame-Helper.dll` | Nexus addon: ImGui UI, IPC host, D3D11 present, CEF download, settings |
| `GW2HelperBrowser.exe` | CEF client: navigate, paint, find-in-page, BootJs |
| `addons/GW2-InGame-Helper/cef/` | Private CEF 150 runtime (first-run download) |

### Embedding and extract

1. **Build:** helper is compiled, copied to `build/helper_blob.exe`, linked into the DLL as a binary blob.
2. **Runtime:** `ExtractHelper()` writes `addons/GW2-InGame-Helper/GW2HelperBrowser.exe` plus `.ver`.
3. **CEF:** `CefRuntime::EnsureInstalled()` (on the launch worker) downloads `cef-runtime-150-windows64.zip`, verifies SHA-256, extracts to `cef/`, writes `cef.ver`.
4. **Launch:** `--cef-dir=<addon>/cef` and `--host-pid=<GW2 PID>`. No `bin64/cef` fallback.

### Packaging CEF

```bash
make pack-cef   # or: bash scripts/pack-cef-runtime.sh
```

Upload the zip to a GitHub Release and keep `src/CefRuntime.h` URL + SHA256 in sync.

### Job Object and host watch

Same Job Object `KILL_ON_JOB_CLOSE`, host `SYNCHRONIZE`, `AF_DisableHotloading`.

### Multi-client

IPC names remain PID-scoped (`WikiIpcFormatNames`).

Cache: `%TEMP%\GW2-InGame-Helper-cef` (never under `addons/` or `bin64/cef`).

---

## 3. Layout

```text
<GW2>/addons/GW2-InGame-Helper.dll     # only file players copy
<GW2>/addons/GW2-InGame-Helper/        # runtime data
  GW2HelperBrowser.exe
  cef/                     # private CEF 150 (downloaded)
    libcef.dll
    locales/
    *.pak, icudtl.dat, …
  settings.ini
  helper-home.html …
```

See also [`BUILD.md`](BUILD.md) and [`COMPLIANCE.md`](COMPLIANCE.md).
