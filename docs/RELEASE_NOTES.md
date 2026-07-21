# GW2 In-Game Helper v1.7.2.1

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 sites and community Discords.
One DLL for Nexus — no memory reads.

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper)

---

## What’s new in 1.7.2.1

- **Fix:** Game freeze from writing `settings.ini` every frame (tab title sync + window pos). Saves are debounced; titles no longer mark dirty every tick

## What’s new in 1.7.2.0

- **Tab hotkeys** — Ctrl+T new-tab picker · Ctrl+W close · Ctrl+Tab / Ctrl+Shift+Tab cycle
- **Fix:** Tab names update when the page changes (CEF title + URL→site match); titles are saved

## What’s new in 1.7.1.1

- **Fix:** Opening a site in a new tab no longer navigates the previous tab to the same page

## What’s new in 1.7.1.0

- **Cheat Sheets** category — built-in offline pages (Raid Food style):
  - **Raid Utilities** — oils, stones, crystals, enrichment by role
  - **Fractal Consumables** — mist potions, utility, agony basics, Mistlock
  - **Sigils & Runes** — common power / condi / support / tank picks
  - **Relics** — short picks by role and damage type
  - **Boon Checklist** — quick / alac / might / fury coverage
  - **CC / Defiance** — breakbar tools by profession
  - **Raid Wings Overview** — wing → bosses → typical KP
  - **Home Garden** — cultivated herbs for feast seasonings (pairs with Raid Food)

## What’s new in 1.7.0.0

- **Compact toolbar** — Browse · nav · Search · `...` (Find / Copy URL / Open Ext / New Tab / Pin / Reopen)
- **Pin tabs** — right-click or `...` · pinned tabs can’t be closed until unpinned
- **Reopen closed tab** — `...` or Ctrl+Shift+T (last 8)
- **Keep browser warm** — Options setting: hide without killing CEF (faster reopen, more RAM)
- **Status** — muted gold chip; hides Ready / closed noise; friendly error text
- **Theme** — warmer gold tabs matching the homepage

## What’s new in 1.6.0.3

- **Fix:** Home / Raid Food / restored tabs no longer open blank
- **Fix:** Opening restores tab URLs instead of always forcing the homepage

---

## Notes

- Closing the helper stops the browser process unless **Keep browser warm** is enabled in Options (tabs restore URLs on reopen).
- Prefer **Open Ext** for Discord joins and logins.
- Hardstuck / Discretize intentionally omitted.
- Settings writes are debounced to avoid hitching the game.
