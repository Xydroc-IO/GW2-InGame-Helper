# GW2 In-Game Helper v1.6.0.0

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.6.0.0

- **Live tabs** — each open tab keeps its own CEF page (scroll/forms/login stay while the helper is open); switching no longer reloads
- **+ new-tab picker** — full Browse panel (categories + filter + Favorites)
- **Favorites drag-reorder** — drag rows in Favorites to reorder (saved)
- **New Tab** — duplicate the current page into another tab
- **Find in page** — Find button / Ctrl+F (Next / Prev / match case)

---

## Notes

- Closing the helper still stops the browser process (tabs restore URLs on reopen; live state is while open).
- Prefer **Open Ext** for Discord joins and logins.
- Hardstuck / Discretize intentionally omitted.
