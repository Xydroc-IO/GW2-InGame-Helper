# GW2 In-Game Helper v1.3.0

<p align="center">
  <img src="media/cover.png" alt="GW2 In-Game Helper" width="100%">
</p>

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites (Wiki, Snowcrows, MetaBattle, and more).
One DLL for Nexus — no memory reads.

**Install:** put `GW2-InGame-Helper.dll` in `<GW2>/addons/`. Nothing else.

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` (or `K`) · QuickAccess icon |

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Wine/Proton OK).
Replaces the older Wiki / Snowcrow browser addons.

## What’s new in 1.3.0

- **Removed item-lookup keybind** (`KB_HELPER_ITEM` / Ctrl+Shift+I / Ctrl+Shift+U).
- That feature used game input and caused mount / party-chat side effects under Wine/Proton.
- Legacy `KB_HELPER_ITEM` is deregistered on load so old binds stop firing.
- Browse sites normally via the in-game picker and toolbar.

## Download (players)

**You only need one file to install:**

| File | Where it goes |
|------|----------------|
| `GW2-InGame-Helper.dll` | `<Guild Wars 2>/addons/GW2-InGame-Helper.dll` |

Do **not** download or copy `GW2HelperBrowser.exe`, CEF, WebView2, or anything else.

### Install steps

1. Close Guild Wars 2.
2. Copy **only** `GW2-InGame-Helper.dll` into your `addons` folder.
3. Start the game. Enable **GW2-InGame-Helper** in Nexus (`Ctrl+O`) if needed.
4. Optional: in Nexus keybinds, delete any leftover `KB_HELPER_ITEM` entry.
5. Optional: disable old Wiki / Snowcrow browser addons if you still have them.

## Highlights

- One DLL — no companion files for players
- Categorized site picker + Nexus QuickAccess top-bar icon
- Built-in how-to homepage on open
- No Guild Wars 2 memory reads (Nexus APIs only)
- No item-capture macros or item-lookup hotkeys

## Included sites

| Category | Site |
|----------|------|
| Help | How to use (default landing) |
| Official | Guild Wars 2 |
| Wiki | GW2 Wiki |
| Builds | Snowcrows, MetaBattle |
| Tools | gw2efficiency |
| Farming | Fast Farming Community |

## Controls

| Action | Default |
|--------|---------|
| Open / close | `Ctrl+Shift+H` or `K`, or QuickAccess icon |
| Rebind | Nexus `Ctrl+O` → `KB_HELPER_TOGGLE` |

## Updating

Close the game, replace the DLL, restart. Remove any leftover `KB_HELPER_ITEM` bind in Nexus if it still appears.

## Compliance

Does **not** read/write game memory, hook game code, or automate combat/trading.
Does **not** inject mouse/keyboard macros.
Uses Nexus APIs and a separate CEF helper process only.

## License

MIT — see [LICENSE](../LICENSE).

---

### For developers / packagers

Player release = **`GW2-InGame-Helper.dll` only**.

Raidcore / web listing: [`description.html`](description.html) · [`RAIDCORE.md`](RAIDCORE.md)

Build: `make -j$(nproc)` → `build/bin/GW2-InGame-Helper.dll`
