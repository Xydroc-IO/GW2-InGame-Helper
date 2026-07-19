# Raidcore / Nexus listing notes

Use these when publishing on Raidcore or pasting into the Nexus addon library.

## Short description (AddonDef / one-liner)

```text
In-game browser for GW2 sites. One DLL. Ctrl+Shift+H toggle; Ctrl+Shift+U wiki from clipboard [&…]. No memory reads.
```

## Signature

```text
0x48454C50
```

## Full HTML body

Paste or host [`description.html`](description.html) (cover + logo under `media/`).

## Release notes

See [`RELEASE_NOTES.md`](RELEASE_NOTES.md) for the current version (v1.2.5).

## Player artifact

Ship **only** `GW2-InGame-Helper.dll`.

## Hotkeys

| Bind ID | Default | Action |
|---------|---------|--------|
| `KB_HELPER_TOGGLE` | `CTRL+SHIFT+H` | Open / close helper |
| `KB_HELPER_ITEM` | `CTRL+SHIFT+U` | Wiki lookup from clipboard `[&…]` |

Wiki tip for players: Shift+Click item → Ctrl+C → Ctrl+Shift+U. No automatic click/chat macros.
