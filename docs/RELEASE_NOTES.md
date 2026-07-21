# GW2 In-Game Helper v1.5.1.0

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

```text
addons/GW2-InGame-Helper.dll          ← install this (only file you place)
addons/GW2-InGame-Helper/             ← created at runtime (helper, pages, settings)
```

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` (or `K`) · QuickAccess icon |
| Rebind | Nexus `Ctrl+O` → `KB_HELPER_TOGGLE` |

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.5.1.0

- **Install layout** — runtime files (helper, homepage, Raid Food, settings) live under `addons/GW2-InGame-Helper/`; only the DLL sits in `addons/`
- Cleans stale files left in `addons/` root from older builds

## What’s new in 1.4.1.x

- Full-page themed homepage, Browse panel, Open Ext / Copy URL, expanded sites (no Hardstuck / Discretize / Lucky Noobs)

---

## Notes

- Use **Browse** to pick sites; **Open Ext** for Discord joins.
- Players only need the DLL.
