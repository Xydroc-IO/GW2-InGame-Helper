# CEF 150.0.14 headers for GW2-InGame-Helper-Beta

Matches Chromium 150.0.7871.129 (Stable).

Headers only — the private runtime is downloaded at first helper open into
`addons/GW2-InGame-Helper-Beta/cef/` (see `src/CefRuntime.h` and
`scripts/pack-cef-runtime.sh`). Do not use Guild Wars 2 `bin64/cef` with these headers.
