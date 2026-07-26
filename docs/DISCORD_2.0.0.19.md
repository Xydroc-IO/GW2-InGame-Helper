# :rocket: GW2 In-Game Helper — **v2.0.0.19**

> Fixed Windows stuck on **Waiting for first paint…** (Ready but black panel). Also: Defender `Wacatac.B!ml` is a known false positive on this unsigned MinGW build.

**Grab it:** [Latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll) — drop into `<Guild Wars 2>/addons/`. **Fully restart GW2** after updating.

---

## What's new

- **Fix:** Native Windows could stay on “Waiting for first paint…” while status said Ready — first GPU upload now blocks instead of skipping forever when the device is busy
- Helper also kicks a resize after load if CEF hasn’t painted yet
- **Defender note:** `Trojan:Win32/Wacatac.B!ml` is an ML false positive on unsigned MinGW DLLs. Allow/restore the file. Devs can submit at https://www.microsoft.com/en-us/wdsi/filesubmission (Software developer → incorrectly detected). Source: https://github.com/Xydroc-IO/GW2-InGame-Helper

`Ctrl+Shift+H` · *Xydroc*
