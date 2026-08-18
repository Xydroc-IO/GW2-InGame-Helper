# GW2 Helper Catalog + CEF

**Tag:** `gw2-helper-catalog` · **Title:** GW2 Helper Catalog · **Pre-release** · **Not a shipping DLL**

One GitHub pre-release for first-run extras: IGH1 catalog/icon packs and the CEF runtime zip. The addon fetches these on its own. Players do **not** install this release.

Do not attach these files to shipping DLL tags (`2.3.0.x`). Add new public packs as extra files on **this same tag**.

## Assets (upload all)

| File | Purpose |
|------|---------|
| `gw2-helper-catalog.ver` | One line: ArenaNet `/v2/build` id. The addon checks this first. |
| `gw2-helper-catalog.igh` | **IGH1** (custom, not zip). Gzip members: `catalog.ver`, `names-en.tsv`, `recipes.tsv`. Name kinds: `i` item, `c` currency, `s` skin, `n` mini, `m` material category, `d` dye, `f` finisher, `o` outfit, `g` glider, `u` mail carrier, `v` novelty, `t` title, `a` achievement, `y` legendary armory (`max_count`). |
| `gw2-helper-icons.ver` | SHA-256 of packed icon keys. Cheap freshness check for the texture pack. |
| `gw2-helper-icons.igh` | **IGH1** unique `render.guildwars2.com` PNGs (~22k, keyed `signature/file_id.png`). Built with `scripts/build-gw2-icons.py`. Stash/wallet/crafting use these first; missing ids still hit the ArenaNet CDN. |
| `cef-runtime-150-windows64.zip` | Private CEF 150 Windows x64 (~170MB). SHA-256 in `src/browser/CefRuntime.h`. Upload by hand (`make pack-cef`). Keep as `.zip`. |

Direct URLs the DLL uses:

- https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/gw2-helper-catalog/gw2-helper-catalog.ver
- https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/gw2-helper-catalog/gw2-helper-catalog.igh
- https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/gw2-helper-catalog/gw2-helper-icons.ver
- https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/gw2-helper-catalog/gw2-helper-icons.igh
- https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/gw2-helper-catalog/cef-runtime-150-windows64.zip

The **GW2 Helper Catalog** Action only clobbers the two **catalog** files. It must **not** delete or replace the icons pack or the CEF zip. Never `gh release delete gw2-helper-catalog`.

Offline: drop `gw2-helper-catalog.igh` and/or `gw2-helper-icons.igh` next to the DLL or in `addons/GW2-InGame-Helper/`.

## What this is / is not

- **Is:** public names/recipes + unique render icons rebuilt when the API build (or icon set) changes, plus the CEF runtime zip. ArenaNet retains ownership of those textures; they are not MIT-relicensed.
- **Is not:** the addon DLL, player API keys, live TP prices, rotating dailies / Wizard’s Vault, or continent floor JSON.

## Rebuild (maintainers)

Catalog (ArenaNet crawl, ~20–25 min):

```bash
python3 scripts/build-gw2-catalog.py -o dist
gh release upload gw2-helper-catalog dist/gw2-helper-catalog.ver dist/gw2-helper-catalog.igh --clobber
```

Icon pack (CDN fetch of unique PNGs, cache in `dist/icon-cache/`; not in the daily Action):

```bash
python3 scripts/build-gw2-icons.py -o dist
gh release upload gw2-helper-catalog dist/gw2-helper-icons.ver dist/gw2-helper-icons.igh --clobber
```

CEF zip (after `make pack-cef`):

```bash
gh release upload gw2-helper-catalog build/cef-runtime/cef-runtime-150-windows64.zip --clobber
```

Keep `CefRuntime.h` `kSha256Hex` in sync if the zip contents change.
