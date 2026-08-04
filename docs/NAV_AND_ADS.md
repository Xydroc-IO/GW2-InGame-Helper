# Navigation and advertisement policy (operational)

**Revision:** 2.2.0.5 · **Audience:** kernel contributors  
**Companions:** [`WHITEPAPER.md`](WHITEPAPER.md) §8, [`KERNEL.md`](KERNEL.md) playbook D, [`COMPLIANCE.md`](COMPLIANCE.md), [`HelperNavPolicy.cpp`](../src/helper/HelperNavPolicy.cpp)

This document is the **do-not-break** checklist for ads, Open Ext, and related navigation. Analytical rationale lives in the whitepaper.

---

## 1. Why this exists

OSR cannot host real popup windows. `OnBeforePopup` always cancels. Without replacement policy, `target=_blank` ad clicks become silent no-ops and publishers lose CPC. Partner sites also need consent/analytics hosts to load.

---

## 2. Open Ext pipeline

1. Helper classifies a navigation as externalisable.
2. Writes **full** URL into `open_ext_url[8192]` and bumps `open_ext_seq`.
3. If too long → **refuse** (never truncate).
4. DLL drains on Tick → `ShellExecuteA` in the **game** process (Proton-correct).

Related: `open_tab_*` opens an in-addon tab instead of the system browser.

---

## 3. Externalise when (summary)

User-gesture navigations matching any of:

1. Click-tracker URL shapes (`pagead/aclk`, DoubleClick `/pcs/click`, …).
2. Click-id query params (`gclid`, `gad_source`, `msclkid`, …).
3. Main-frame hits on ad-network hosts.
4. Referrer is an ad frame (SafeFrame / DoubleClick) even if destination looks “clean.”
5. Subframe-originated promotable URLs from known ad iframes.
6. Cross-site `target=_blank` (same-site / `file://` may stay in-tab).

Also prefer external for YouTube / `discord://` flows OSR cannot complete.

---

## 4. Non-regression (merge blockers)

A PR **must not** merge if it:

1. Truncates Open Ext URLs or shrinks the 8 KiB buffer without a complete replacement design.
2. Blocks ad / consent / analytics hosts by default.
3. Applies `pointer-events: none` (or equivalent) such that ad creatives are not clickable.
4. Re-enables native OSR popups without an externalisation path.
5. Relies solely on helper-process `ShellExecute` with no DLL drain (breaks Proton).
6. Sets `GetScreenInfo` / `window.screen` to the tiny panel size (hurts viewability fingerprint hygiene).

---

## 5. Maintainer tracing

Create `navlog.on` beside the helper to log browse/popup decisions. **Never ship** that marker enabled.

---

## 6. BootJs / CssCompat rules of thumb

- Open Ext tips for Google / Cloudflare / some logins are OK.
- Do not “fix” pages by stripping ads.
- YouTube embeds → Watch cards / Open Ext (stock CEF lacks proprietary codecs).
- `<select>` uses in-page polyfill — do not restore PET_POPUP casually (Windows helper crashes).

---

## 7. Where to edit

| Concern | File |
|---------|------|
| Policy decisions | `src/helper/HelperNavPolicy.cpp` (+ Handlers) |
| OSR geometry | `src/helper/HelperOsrRender.cpp` |
| Injected tips / polyfills | `src/helper/BootJs.h` |
| CSS filters | `src/helper/CssCompat*` |
| DLL Open Ext drain | `src/browser/WikiBrowserHelper*` |

Follow [`KERNEL.md`](KERNEL.md) paired-review expectations.
