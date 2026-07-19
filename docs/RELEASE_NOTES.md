# GW2 In-Game Helper v1.2.5

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
| Wiki item from clipboard | `Ctrl+Shift+U` |

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Wine/Proton OK).
Replaces the older Wiki / Snowcrow browser addons.

## Download (players)

**You only need one file to install:**

| File | Where it goes |
|------|----------------|
| `GW2-InGame-Helper.dll` | `<Guild Wars 2>/addons/GW2-InGame-Helper.dll` |

Do **not** download or copy `GW2HelperBrowser.exe`, CEF, WebView2, or anything else.

### What happens at runtime (automatic)

1. On first open, the DLL **extracts** the embedded `GW2HelperBrowser.exe` into the addon’s Nexus data folder.
2. That helper loads Chromium from the game’s existing `bin64/cef` (read-only).
3. Pages render off-screen and show in the overlay.

### Install steps

1. Close Guild Wars 2.
2. Copy **only** `GW2-InGame-Helper.dll` into your `addons` folder.
3. Start the game. Enable **GW2-InGame-Helper** in Nexus (`Ctrl+O`) if needed.
4. Optional: disable old Wiki / Snowcrow browser addons if you still have them.

### Common paths

```text
Windows: C:\Program Files (x86)\Steam\steamapps\common\Guild Wars 2\addons\
Linux:   ~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/
```

## Highlights

- One DLL — no companion files for players
- Categorized site picker + Nexus QuickAccess top-bar icon
- Built-in how-to homepage on open
- Wiki item lookup from a clipboard `[&…]` chat link (no fake clicks / chat macros)
- No Guild Wars 2 memory reads (Nexus APIs only)

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
| Wiki item lookup | `Ctrl+Shift+U` (clipboard `[&…]`) |
| Rebind | Nexus `Ctrl+O` → `KB_HELPER_TOGGLE` / `KB_HELPER_ITEM` |

### Wiki item lookup

1. In game, **Shift+Click** an item so a `[&…]` link appears in chat (or copy one from chat).
2. Copy it with `Ctrl+C`.
3. Press `Ctrl+Shift+U`.
4. The helper opens the Wiki for that item.

The addon does **not** simulate Shift+Click, open chat, or press Enter for you.

## Updating

Close the game, replace the same DLL path with the new `GW2-InGame-Helper.dll`, restart.
If an old bind remains, reset `KB_HELPER_ITEM` to `Ctrl+Shift+U` in Nexus.

## Known notes

- Restart the game after replacing the DLL (hotloading is disabled).
- Some modern sites may look imperfect under the game’s older CEF (Chromium 103).

## Compliance

Does **not** read/write game memory, hook game code, or automate combat/trading.
Does **not** inject mouse/keyboard macros into the game for item capture.
Uses Nexus APIs and a separate CEF helper process only.

## License

MIT — see [LICENSE](../LICENSE). Third-party deps (Nexus API, ImGui, CEF headers) retain their own licenses.

---

### For developers / packagers

Player release = **`GW2-InGame-Helper.dll` only**.  
Do not zip `GW2HelperBrowser.exe`, CEF binaries, or `build/` for players — the helper is already linked into the DLL as an embedded blob and extracted on first use.

Raidcore / web listing HTML: [`description.html`](description.html)

Build: `make -j$(nproc)` → `build/bin/GW2-InGame-Helper.dll`
