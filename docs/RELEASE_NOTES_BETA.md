# GW2 In-Game Helper Beta

**Product:** `GW2-InGame-Helper-Beta` · **Base:** v2.0.1.1 · **Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

Private-CEF channel of the Raidcore Nexus in-game browser. Same Browse UI and
catalog direction as stable, but Chromium comes from **CEF Stable 150** under
the addon folder — not Guild Wars 2 `bin64/cef` (CEF 103).

Docs: [`CEF_RUNTIME.md`](CEF_RUNTIME.md) · [`ARCHITECTURE.md`](ARCHITECTURE.md) ·
[`SNOWCROWS.md`](SNOWCROWS.md) · [`COMPLIANCE.md`](COMPLIANCE.md)

---

## Install

1. Copy **`GW2-InGame-Helper-Beta.dll`** into `<Guild Wars 2>/addons/`.
2. Keep the **stable** `GW2-InGame-Helper.dll` if you still want it — they are
   separate addons and separate data folders.
3. Open the Beta helper once. On first launch it installs private CEF into
   `addons/GW2-InGame-Helper-Beta/cef/` (~170MB zip, one-time).

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2
(Windows / Wine / Proton).

**Do not** drop Beta’s CEF into `bin64/cef`. Beta never writes there.

**Offline / pre-seed CEF:** place `cef-runtime-150-windows64.zip` next to the DLL
or under `addons/GW2-InGame-Helper-Beta/` (see [`CEF_RUNTIME.md`](CEF_RUNTIME.md)).

---

## What’s in this Beta

### Private CEF Stable 150

- Runtime: **CEF 150.0.14** / Chromium **150.0.7871.129**
- Path: `addons/GW2-InGame-Helper-Beta/cef/` + `cef.ver` stamp
- First open: local zip (SHA-256) → else HTTPS download → extract → launch helper
- No fallback to game `bin64/cef` (headers / ABI won’t match)

### Why Beta exists

Modern sites (especially **Snow Crows**) use CSS/JS that CEF 103 downlevels
awkwardly. Private CEF 150 paints modern CSS natively and reduces that jank.
It does **not** magically fix every Cloudflare challenge, OAuth edge case, or
OSR limit.

### Input & focus

- CEF 150 key events set `cef_key_event_t.size` (typing works again)
- Same Nexus input routing as stable: page typing does not drive skills / WASD

### Snow Crows

- Optional Browse test entries (builds hub / login) for CEF testing — not a full
  catalog return; removable on request
- Login: Discord OAuth can hand off to the Discord app (**Continue to Discord**)
  when localhost RPC is allowed; tip **Open Ext** if it fails
- Account / settings: native `<select>` lists (e.g. region) use an in-page picker
  because off-screen CEF cannot show OS combobox popups reliably
- Header chrome kept above NitroPay so Profile / Inbox stay clickable
- No CEF-103 CSS rewrite / response buffering on Snow Crows (pass-through)

### Reliability under Proton

- Crash-loop brake and process caps so a bad helper launch does not lock the host
- Discord `discord://` deep links open via the game process (Wine helper
  ShellExecute is unreliable)
- Local Network Access checks relaxed so Discord’s desktop-app probe
  (`127.0.0.1:6463`) can unlock **Continue to Discord**

### Docs & tooling

- [`CEF_RUNTIME.md`](CEF_RUNTIME.md) — install / zip / SHA flow
- `scripts/pack-cef-runtime.sh` / `make pack-cef` — pack the runtime zip
- Compliance / architecture / Snow Crows notes updated for the Beta channel

---

## Known limitations (honest)

| Issue | Guidance |
|-------|----------|
| MetaBattle / other **Cloudflare** interstitials | Often fail in embedded OSR CEF — use **Open Ext** |
| Google / Gemini sign-in | Frequently blocked in CEF — **Open Ext** |
| Discord OAuth | Prefer **Continue to Discord** / app; else **Open Ext** (sessions are separate) |
| Native `<select>` without polyfill hosts | OSR combobox popups are incomplete; Snow Crows is polyfilled |
| Ad viewability / NitroPay “desktop” guarantees | Not claimed for OSR |
| First CEF download | Needs network once (~170MB) unless you pre-seed the zip |
| Stable vs Beta data | Separate folders; cookies / login are **not** shared |

We do **not** bypass Cloudflare, framing, or OAuth protections.

---

## Coexistence with stable

| | Stable | Beta |
|---|--------|------|
| DLL | `GW2-InGame-Helper.dll` | `GW2-InGame-Helper-Beta.dll` |
| Data | `addons/GW2-InGame-Helper/` | `addons/GW2-InGame-Helper-Beta/` |
| CEF | Game `bin64/cef` (~103) | Private `…/cef/` (150) |
| Branch / tree | `master` / main project | `GW2-InGame-Helper-Beta` |

You can load both in Nexus; use Beta for modern-site testing and stable for the
usual day-to-day path.

---

## After updating

Fully quit Guild Wars 2 (and any stuck `GW2HelperBrowser.exe`), then relaunch so
the new DLL re-extracts the helper when the stamp changes.

---

## Feedback

Report what still fails in-game (site + what you clicked + Proton vs Windows).
Highest-value reports: Snow Crows settings/login, MetaBattle Cloudflare,
and any full lockups after refresh.
