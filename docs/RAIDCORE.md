# Raidcore / Nexus listing notes

Use these when publishing on Raidcore or pasting into the Nexus addon library.

## Short description (AddonDef / one-liner)

```text
In-game browser for GW2 sites. One DLL. Ctrl+Shift+H toggle. No memory reads. No item-lookup macros.
```

## Signature

```text
0x48454C50
```

## Full HTML body

Paste or host [`description.html`](description.html) (cover + logo under `media/`).

## Release notes

See [`RELEASE_NOTES.md`](RELEASE_NOTES.md) for the current version (v1.3.0).

## Player artifact

Ship **only** `GW2-InGame-Helper.dll`.

## Hotkeys

| Bind ID | Default | Action |
|---------|---------|--------|
| `KB_HELPER_TOGGLE` | `CTRL+SHIFT+H` | Open / close helper |

**Removed:** `KB_HELPER_ITEM` (Ctrl+Shift+I / U item lookup). Deregistered on load so leftover binds stop firing.
