# Note for Snow Crows — GW2 In-Game Helper (Beta)

**To:** Snow Crows staff / site owners  
**From:** maintainer of [GW2 In-Game Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper) (Raidcore Nexus addon)  
**Purpose:** Clear description of what the **Beta** addon is, what it does with snowcrows.com, and why it is not a risk to your site, brand, or revenue.

---

## Short version

GW2 In-Game Helper Beta is an **in-game web browser** for Guild Wars 2 players. When someone opens your site in it, they are loading **your live pages from your servers**, in a Chromium-based browser — the same way they would in Chrome or Firefox, just painted inside the game window.

We do **not**:

- scrape, mirror, or republish your guides
- call a private Snow Crows API
- strip or block your ads / NitroPay / analytics / consent UI
- steal cookies or bridge login sessions into other tools
- automate accounts, combat, or site actions
- read or write Guild Wars 2 game memory

We **do** treat your prior request seriously: full Snow Crows Browse catalog entries were **removed at your request** (addon v2.0.0.20). Any presence since then has been minimal and intentional (see below).

If you prefer **zero** in-addon links to snowcrows.com, say so and they will be removed.

---

## What the addon is

| Item | Detail |
|------|--------|
| Product | Raidcore **Nexus** ImGui addon (`GW2-InGame-Helper-Beta.dll`) |
| Job | Let players open useful GW2 community sites **while playing** |
| Browser | Out-of-process **CEF Stable 150** (private runtime under `addons/GW2-InGame-Helper-Beta/cef/`) |
| Integration | Official Nexus APIs only (render, input, paths, swap chain) |
| Source | Open / inspectable — MIT license |
| Version context | Beta line ~**2.0.1.x** (sibling of stable game-CEF build) |

Players install one DLL. The helper EXE extracts locally under the addon folder. Chromium is downloaded once into the addon `cef/` folder — we do **not** write into game `bin64/cef`. Live HTTPS to your origin is unchanged.

---

## How your site is used (if a player opens it)

1. The player picks a Browse link 
2. CEF navigates to that URL over normal HTTPS.
3. Your HTML, CSS, JS, ads, and analytics load from **your** origin (and your CDNs), like any other browser.
4. The page is painted off-screen and shown in an ImGui window.

There is **no** Snow Crows-specific backend, scraper, or content cache inside the addon. Offline “cheat sheets” in the addon are our own static pages — not copies of your guides.

### Ads and analytics

Since v2.0.0.20 we **allow** site ads, consent banners, and analytics hosts (including NitroPay-related loads). We do **not** run an ad blocker against your pages.

**Honest caveat:** the in-game browser is CEF **off-screen rendering** (Chromium ~150 on Beta). That environment can still differ from a normal desktop Chrome session (Cloudflare challenges, some OAuth flows, and ad-network “viewability / guaranteed impression” rules). We do **not** claim we can meet NitroPay desktop guarantees. We also do **not** strip your tags to “fake” compliance — if a creative cannot complete in CEF, the honest path is the player using **Open Ext** (system browser).

### Login / Discord OAuth / Cloudflare

Embedded CEF often cannot finish Cloudflare bot checks or Discord OAuth the way a system browser can. The addon tells players to use **Open Ext** when that happens. System-browser sessions are **separate** from in-game tabs — we do **not** copy cookies between them.

### Display polish only

On some modern sites (including yours), we may rewrite CSS **in the client** so older Chromium can paint layouts that use newer color functions (e.g. `oklch`). That is a display compatibility fix in the player’s browser process. It is not content scraping and does not change what your server sends as the source of truth for other visitors.

We do **not** remove or rewrite your article bodies for ads. (An older ad-strip bug once matched placement ids and briefly blanked guides; that path is gone, and ads are allowed.)

---

## Respect for your earlier request

You asked us to stop featuring Snow Crows site-wide in the addon catalog. We complied:

- Removed Browse deep links to builds / raid guides / Discord invite (v2.0.0.20).
- Players were directed to other community sources (e.g. MetaBattle, Accessibility Wars) for those Browse rows.
- Project compliance notes still say: **do not re-add a full Snow Crows catalog without an explicit product decision with you.**

**Current limited presence (for your awareness):** for CEF / Cloudflare / login testing only, there may be one or two optional test Browse entries (e.g. builds hub and/or login). These are **not** a return of the full catalog. If you want them gone, they will be removed immediately — no argument.

---

## Why this is low risk for Snow Crows

| Concern | Our position |
|---------|----------------|
| Content theft / mirror | No. Live navigation only; no republished guides. |
| Competing “SC app” | No. Generic multi-site helper; you are one possible URL among many. |
| Ad revenue harm | We stopped blocking ads. We do not promise desktop NitroPay metrics in CEF OSR. |
| Brand / deep linking | Catalog featuring is under **your** consent. Previously removed on request; remains removable. |
| Account / cookie abuse | No session bridging; OAuth failures → Open Ext. |
| Botting / ToS (ArenaNet) | No game memory, no input injection into GW2, no combat automation. |
| Security of your servers | Same attack surface as any browser hitting HTTPS; we are not a privileged client. |
| X-Frame / embedding abuse | We do not strip `X-Frame-Options` / CSP `frame-ancestors`. Your site loads as a **top-level** tab in CEF, not as a framed widget we force into another origin. |

---

## What we will never do without talking to you first

- Re-add a large Browse catalog of Snow Crows guides / builds / Discord as a default feature set
- Ship a scraper, offline mirror, or “SC mode” that copies your pages into the addon
- Claim official partnership or endorsement you have not given
- Bypass Cloudflare / framing / OAuth protections on your behalf

---

## What we would appreciate from you (optional)

Nothing is required. If you want a clear relationship:

1. **No links** — we remove any remaining snowcrows.com Browse entries.  
2. **Test-only links OK** — for testing purposes
3. **Catalog OK again** — only if you explicitly want deep links back (we will not assume).

A short written preference (even one sentence) is enough for us to follow.

---

## Technical one-pager (for whoever checks the code)

```text
Guild Wars 2.exe
 └─ Nexus loads GW2-InGame-Helper-Beta.dll
      └─ CreateProcess → GW2HelperBrowser.exe
           └─ LoadLibrary(addons/.../cef/libcef.dll) — windowless OSR
                └─ HTTPS navigation to whatever URL the player opened
```

- **IPC:** shared memory between DLL and helper only (frame pixels + input/commands). Scoped by GW2 process id.
- **Browse rows:** static labeled hyperlinks in source (`Sites.cpp`) — not a live site API.
- **Official GW2 API:** used only where pages need public armory-style data, with credentials omitted and rate-limit backoff — not a Snow Crows private API.
- **Hot-reload:** disabled; players restart the game after updates.
- **Job Object:** helper dies with the game so we do not leave orphan browser processes.

Deeper maintainer docs (architecture / compliance) live in the repo under `docs/`.

---

## Closing

You asked us to step back once; we did. The addon is a player browser chrome around the normal web — not a scraper, not a mirror, and not a way around your ads or auth. We want zero surprise and zero conflict with Snow Crows.

If anything above is unclear, or you want a change to how snowcrows.com appears (or does not appear) in the addon, tell us and we will match your preference.
