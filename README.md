# GW2 In-Game Helper

A Raidcore Nexus addon that opens useful Guild Wars 2 websites inside the game.
One DLL — pick Wiki, Snowcrows, MetaBattle, and more from an in-game browser.
No memory reads; uses Nexus APIs and the game’s built-in CEF.

**Install:** copy `GW2-InGame-Helper.dll` into `<GW2>/addons/`. That’s it.

| Site | Category |
|------|----------|
| How to use (built-in) | Help |
| [Guild Wars 2](https://www.guildwars2.com/) | Official |
| [Guild Wars 2 Wiki](https://wiki.guildwars2.com/) | Wiki |
| [Snowcrows](https://snowcrows.com/) | Builds |
| [MetaBattle](https://metabattle.com/wiki/MetaBattle_Wiki) | Builds |
| [gw2efficiency](https://gw2efficiency.com/) | Tools |
| [Fast Farming Community](https://fast.farming-community.eu/) | Farming |

Add more sites in `src/Sites.cpp`. Replaces the older Wiki / Snowcrow browser addons.
Works on Windows and on Linux via Wine/Proton.

> **Players only need the DLL.** The browser helper is embedded and extracts on first use; Chromium comes from the game’s `bin64/cef`.

## Features

- In-game CEF browser with site dropdown (grouped by category)
- Nexus **QuickAccess** icon at the top of the screen
- Hotkeys for toggle and hovered-item Wiki lookup
- Home / Back / Forward / Reload toolbar
- Built-in how-to homepage on first open
- Single DLL — browser helper is embedded and extracted on first use
- **No Guild Wars 2 memory reads** — official Nexus APIs only

## Requirements

- Guild Wars 2 (64-bit Windows client)
- [Raidcore Nexus](https://raidcore.gg/gw2/nexus) installed and working
- **Only** the release file `GW2-InGame-Helper.dll` (nothing else)

## Install (players)

Players need **one file**. No separate helper `.exe`, CEF package, or WebView2.

1. Close Guild Wars 2.
2. Copy `GW2-InGame-Helper.dll` into your game’s `addons` folder:

   ```text
   <Guild Wars 2>/addons/GW2-InGame-Helper.dll
   ```

3. Start the game, open Nexus with `Ctrl+O`, and enable **GW2-InGame-Helper** if needed.
4. Restart if Nexus asks you to.

The DLL embeds its browser helper. On first use it extracts `GW2HelperBrowser.exe`
into the addon’s Nexus directory and loads CEF from the game’s existing
`bin64/cef` folder. Do **not** download or ship a separate CEF runtime or helper
exe — players only install the DLL.

### Common install paths

**Windows (Steam)**

```text
C:\Program Files (x86)\Steam\steamapps\common\Guild Wars 2
```

**Linux (Steam)**

```text
~/.local/share/Steam/steamapps/common/Guild Wars 2
```

## How to use

| Action | Default |
|--------|---------|
| Open / close helper | `Ctrl+Shift+H` or QuickAccess icon |
| Wiki lookup (hovered item) | `Ctrl+Shift+I` |
| Nexus options / rebinds | `Ctrl+O` → `KB_HELPER_TOGGLE` / `KB_HELPER_ITEM` |

1. Open the helper — it starts on the how-to **Home** page.
2. Pick a site from the toolbar dropdown.
3. Click inside the page to interact.
4. Use **Back**, **Forward**, **Home**, and **Reload** as needed.
5. Click outside the window (on the game) to return movement/skills to Guild Wars 2.

### Wiki item lookup

1. Hover an item in inventory, bank, trading post, or crafting.
2. Press `Ctrl+Shift+I`.
3. The helper switches to the Wiki and opens that item’s page.

If a `[&...]` chat link is already on the clipboard, the same hotkey uses it without capturing again.

Opacity, font scale, and related options live in the addon’s Nexus options panel. Window size and position are saved automatically.

## Updating

1. Close Guild Wars 2.
2. Replace `addons/GW2-InGame-Helper.dll` with the new build.
3. Start the game again.

## Build from source

### Dependencies (submodules)

```bash
git clone --recurse-submodules <this-repo-url>
cd GW2-InGame-Helper
# or, if already cloned:
git submodule update --init --recursive
```

| Submodule | Source |
|-----------|--------|
| `deps/nexus` | [Raidcore Nexus API](https://github.com/RaidcoreGG/RCGG-lib-nexus-api) |
| `deps/imgui` | [Raidcore imgui fork](https://github.com/RaidcoreGG/imgui) |
| `deps/cef` | CEF headers only (runtime comes from the game) |

### Linux (MinGW cross-compile)

```bash
# Arch / Manjaro
sudo pacman -S --needed mingw-w64-gcc make git

make -j"$(nproc)"
```

Output:

```text
build/bin/GW2-InGame-Helper.dll
```

Install into a local GW2 tree (default Steam path on Linux):

```bash
make install
# or:
make install GW2_ROOT="/path/to/Guild Wars 2"
```

Clean:

```bash
make clean
```

### CMake (optional)

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
cmake --build build -j"$(nproc)"
```

## Adding another site

Edit `src/Sites.cpp` and append a `SiteDef`:

```cpp
{
    "hardstuck",                         // unique id (saved in settings)
    "Builds",                            // category (group header in picker)
    "Hardstuck",                         // dropdown label
    "Hardstuck",                         // window/title hint
    "https://hardstuck.gg/",             // Home URL
    nullptr,                             // optional search URL prefix
    nullptr,                             // optional search URL suffix
    false,                               // itemLookup (Wiki-only)
},
```

Keep sites with the same category contiguous so the picker groups them. Rebuild and reinstall.

For search bars, set `searchUrlPrefix` / `searchUrlSuffix` so a query becomes `prefix + urlencode(query) + suffix`.

## Troubleshooting

**Addon does not appear**

- Confirm Nexus opens with `Ctrl+O`.
- Filename must be exactly `GW2-InGame-Helper.dll` under `<GW2>/addons`.
- Enable the addon in Nexus and restart.

**Window does not open**

- Try `Ctrl+Shift+H` or the top QuickAccess icon.
- Check Nexus for a conflicting `KB_HELPER_TOGGLE` bind.
- In addon options, enable **Show helper window**.

**Page stuck loading**

- Confirm `bin64/cef/libcef.dll` exists in the game install.
- Allow `GW2HelperBrowser.exe` if antivirus blocks it.
- Fully quit and restart the game.

**Typing / clicking feels wrong**

- Click inside the rendered page first.
- Click the game outside the window to release keyboard focus.
- Restart after replacing the DLL (hotloading is disabled for this addon).

## Policy & compliance

Intended to stay within ArenaNet’s
[Third-Party Programs](https://help.guildwars2.com/hc/en-us/articles/360013625034-Policy-Third-Party-Programs)
policy and [Raidcore’s Addon Policy](https://raidcore.gg/gw2/addon-policy).

ArenaNet does not endorse third-party software. Use at your own risk. Not affiliated with ArenaNet, NCSoft, Guild Wars 2, or Snow Crows.

### Does **not**

- Read or write Guild Wars 2 process memory
- Use MinHook / Detours / IAT hooks or patch game code
- Use Nexus `MinHook_*`, `DataLink_*`, or MumbleLink
- Automate combat, inventory, trading, or economy
- Bot, macro unattended play, or spoof packets
- Modify `Gw2-64.exe` or ArenaNet game DLLs

### Does

- Use official Nexus APIs (ImGui, render callbacks, keybinds, WndProc, paths, logging, D3D11 texture)
- Open public websites in a separate helper process
- Load the game’s CEF runtime **read-only** into that helper
- Share pixels/input via local shared-memory IPC
- Block keyboard from the game only while the page has focus
- Optional hovered-item Wiki lookup via clipboard chat links

## How it works

1. `GW2-InGame-Helper.dll` — Nexus UI, site picker, QuickAccess, D3D11 texture.
2. Embedded `GW2HelperBrowser.exe` — loads `<GW2>/bin64/cef/libcef.dll` (read-only).
3. CEF renders off-screen into shared memory.
4. CSS is downleveled for Chromium 103 where needed; common ad/consent hosts are blocked.
5. HTTP cache lives under `%TEMP%` (not under `addons`).
6. Site list lives in `src/Sites.cpp`.

## License

This project is licensed under the [MIT License](LICENSE).

### Third-party

| Component | License | Notes |
|-----------|---------|--------|
| [Raidcore Nexus API](https://github.com/RaidcoreGG/RCGG-lib-nexus-api) (`deps/nexus`) | MIT | Headers only |
| [Dear ImGui](https://github.com/RaidcoreGG/imgui) (`deps/imgui`) | MIT | Raidcore fork |
| CEF headers (`deps/cef`) | BSD-style (Chromium Embedded Framework) | Headers only; runtime `libcef.dll` is shipped by Guild Wars 2 |

Guild Wars 2, its CEF runtime, and related trademarks belong to ArenaNet / NCSoft.
This project is not affiliated with them.
