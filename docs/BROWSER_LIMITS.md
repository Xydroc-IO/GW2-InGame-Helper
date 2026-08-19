# In-game browser limitations

**Revision:** 2.3.0.4 · **Audience:** players and contributors

The Helper embeds a real Chromium engine (CEF) inside Guild Wars 2. It is still
**not** a full desktop browser. Some sites will fail, look broken, or refuse to
sign you in. That is expected.

## What this browser is

- An **out-of-process**, **windowless** (OSR) Chromium helper painted into the
  Nexus overlay
- Tuned to **coexist** with Guild Wars 2, ArcDPS, ReShade, and Wine/Proton
- Distributed as **stock CEF** binaries (no custom Chromium fork)

It is built for wikis, guides, build sites, and account tools — not for every
site on the internet.

## What often does not work

| Category | Examples | What to do |
|----------|----------|------------|
| Sign-in / OAuth | Google, Discord account linking, many “Continue with…” flows | **Open Ext** (system browser) |
| Bot checks | Cloudflare “Just a moment…”, reCAPTCHA | **Open Ext** |
| Video / streaming | YouTube embeds, Twitch, TikTok, many MP4 players | **Open Ext** (or the site’s Watch card) |
| DRM media | Services that require Widevine / protected playback | Use a normal browser or app |
| Heavy WebGL / hardware video | Players that assume a normal Chrome GPU path | **Open Ext** |

Sessions in **Open Ext** are separate from in-game tabs (cookies and logins do
not carry over).

## Why we cannot “just fix” this

These limits are structural, not a missing checkbox:

1. **Stock CEF has no proprietary codecs** (H.264 / AAC). Many video sites need
   them. Enabling them means rebuilding Chromium and securing codec licenses —
   a product and legal decision, not a config flag.
2. **Windowless OSR + software rendering** keeps the game and other overlays
   stable (especially under Proton). That path is a poor fit for sites that
   expect a normal Chrome window, GPU decode, and real popups.
3. **Large sites fingerprint embedded browsers.** Google, Cloudflare, TikTok,
   and similar services often block or degrade non-standard clients even when
   the page partially loads.
4. **Widevine / DRM** is a separate licensing stack. It would not fix codec or
   OSR issues, and it is not practical to ship inside this addon.

Even a large engineering effort would still leave many sites unreliable, while
risking game stability, distribution size, and compliance. Prefer **Open Ext**
when the in-game view is not enough.

## Practical guidance

- Use the in-game browser for reading and browsing GW2 community sites.
- Use **Open Ext** for login, Discord invites, bot checks, and video.
- If a page is blank, stuck, or “unsupported,” assume the limitation is the
  embedded browser — not a corrupt install — unless other Helper pages also fail.

See also: [`COMPLIANCE.md`](COMPLIANCE.md) (normative codec / OAuth notes),
[`WHITEPAPER.md`](WHITEPAPER.md) (design trade-offs).
