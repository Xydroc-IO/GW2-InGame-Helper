# :rocket: GW2 In-Game Helper — **v2.0.0.5** hotfix

> Fixed the black browser panel stuck on **“Waiting for first paint…”**.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`. That's it.
Needs **Raidcore Nexus**. Restart GW2 after updating (hot-reload is disabled).

---

## :wrench: Fix

- First GPU upload now uses `WRITE_DISCARD` so the D3D11 dynamic texture initializes correctly
- Chunked staging uploads only run **after** the first successful paint

Also includes everything from **2.0.0.4** (worker-thread launch, safer quit/reopen, Browse caches, BootJs single-flight API, compliance docs).

---

`Ctrl+Shift+H` open/close · feedback welcome — *Xydroc*
