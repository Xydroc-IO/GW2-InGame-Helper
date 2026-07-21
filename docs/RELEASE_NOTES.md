# GW2 In-Game Helper v1.5.5.0

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.5.5.0

- **Persisted tabs** — open tabs (site + last URL + active index) save to `settings.ini` and restore on next launch
- **Default landing site** — Options “Choose default site…” now sets a real default used when no tabs are saved (no longer forced to Home on load)

---

## Recent

| Version | Highlights |
|---------|------------|
| **1.5.4.1** | Escape no longer closes the helper |
| **1.5.4.0** | GW2 ImGui theme; toolbar Search; KillProof/Wingman/BLTC |
| **1.5.3.x** | Tabs + Search/Google |
| **1.5.2.x** | Favorites |
| **1.5.1.0** | Runtime under `addons/GW2-InGame-Helper/` |

---

## Notes

- Switching tabs still reloads the saved URL in the single embedded browser.
- Prefer **Open Ext** for Discord joins and logins.
- Players only need the DLL.
