# User themes

**Revision:** 2.2.3.10 · **Audience:** players and contributors  
**Companions:** [`ONBOARDING.md`](ONBOARDING.md), [`ARCHITECTURE.md`](ARCHITECTURE.md)

---

## 1. Purpose

Drop-in color themes for ImGui pads/overlays and built-in helper pages (Home, Live Panels, Cheat Sheets, Raid Food, Legendary Ledger). External browse sites and stamped `ui-chrome` PNGs are **not** themed.

---

## 2. Folder layout

Under the addon data directory:

```
addons/GW2-InGame-Helper/config/themes/
  README.txt
  example-high-contrast/
    theme.ini
  my-theme/
    theme.ini
```

Created automatically on first load (`UserTheme::EnsureSeed`). Pick a theme in **Settings → Theme**.

---

## 3. `theme.ini`

Keys are hex `#RRGGBB` or `#RRGGBBAA`. Omit a key to keep the built-in default.

| Key | Maps to |
|-----|---------|
| `gold`, `gold_bright`, `gold_dim`, `gold_muted` | Accent golds |
| `text` or `ink` | Primary text |
| `muted` | Secondary text |
| `bg`, `panel`, `child` | Surfaces |
| `border` | Borders |
| `tab_active`, `tab_idle`, `header` | Tabs / headers |
| `ok`, `warn` | Status colors |
| `name` | Optional label (ignored by loader) |

Example:

```ini
name=High contrast
gold=#ffe566
text=#ffffff
muted=#c8c8c8
bg=#000000
panel=#1a1a1a
border=#ffcc00
ok=#66ff66
warn=#ffaa44
```

---

## 4. Runtime

| Piece | Role |
|-------|------|
| `UserTheme` | Load / list / apply / CSS `:root` emit |
| `HelperTheme` | Mutable ImGui color tokens |
| `HelperThemeCss::AppendUserRoot` | Inject CSS after builtin vars |
| `settings.ini` → `ThemeId=` | Selected folder name (empty = default) |

**Reload themes** in Settings rescans folders and rewrites Home / Cheat Sheets CSS. Reopen other helper pages if they were already open.

---

## 5. Limitations

- No custom CSS files, font files, or ui-chrome texture swaps in this MVP.
- Third-party sites in the browser stay unthemed.
- Changing `theme.ini` on disk needs **Reload themes** (or restart) to pick up edits.
