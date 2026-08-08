# Publisher guide — allow or block GW2 In-Game Helper

This note is for site operators (Snow Crows, Guildjen, MetaBattle, etc.) who want to
**allow**, **deny**, or **treat differently** traffic from the Guild Wars 2 addon
[GW2 In-Game Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper).

---

## What the addon is

- A **Raidcore Nexus** overlay that embeds **stock Chromium (CEF 150)** as a separate
  process and paints pages into an ImGui panel inside Guild Wars 2.
- It is **not** a bot farm, scraper, or headless CI runner. It is a real player’s browser
  with a small viewport.
- It **does** load site ads, consent, and analytics by policy. Ad clicks are handed to the
  player’s system browser when OSR cannot host the popup.
- It **does not** read Guild Wars 2 game memory for automation, inject into your backend,
  or spoof your cookies from another product.

Players who are blocked in-game can still open your site with **Open Ext** (their normal
desktop browser). Blocking the addon only affects the in-game panel.

---

## Recommended: match the User-Agent product token

Starting with helper builds that identify themselves, the Chromium **User-Agent** ends with
a stable product token:

```text
… Chrome/150.0.7871.129 Safari/537.36 GW2-InGame-Helper
```

Match substring (case-sensitive):

```text
GW2-InGame-Helper
```

Do **not** key only on `Chrome/150` — that also matches other Chromium 150 clients.

### Cloudflare (WAF / custom rule)

**Expression (block HTML document requests):**

```text
(http.user_agent contains "GW2-InGame-Helper")
```

Action: *Block*, or *Managed Challenge*, or *JS Challenge* — your choice.

To block only certain paths (example: keep marketing homepage, block `/builds`):

```text
(http.user_agent contains "GW2-InGame-Helper" and starts_with(http.request.uri.path, "/builds"))
```

### nginx

```nginx
# Inside server { } or a location
if ($http_user_agent ~* "GW2-InGame-Helper") {
    return 403;
}
```

Softer option — serve a short explain page:

```nginx
if ($http_user_agent ~* "GW2-InGame-Helper") {
    return 403 '{"error":"GW2 In-Game Helper is not permitted on this site. Open this page in a normal browser."}\n';
}
```

(Adjust `Content-Type` via a dedicated `location` + `error_page` if you need proper JSON/HTML.)

### Apache

```apache
RewriteEngine On
RewriteCond %{HTTP_USER_AGENT} GW2-InGame-Helper [NC]
RewriteRule ^ - [F,L]
```

### Node / Express (middleware sketch)

```js
app.use((req, res, next) => {
  const ua = req.get('user-agent') || '';
  if (ua.includes('GW2-InGame-Helper')) {
    return res.status(403).type('html').send(
      '<!doctype html><title>Not supported</title>' +
      '<p>GW2 In-Game Helper is not allowed here. ' +
      'Please open this site in Chrome, Firefox, or Edge.</p>'
    );
  }
  next();
});
```

### CDN / edge (generic)

Any rule that inspects `User-Agent` / `http.user_agent` can use the same substring
`GW2-InGame-Helper`. Prefer an explicit deny list over guessing “embedded CEF” from
screen size or missing WebGL — those heuristics false-positive real users.

---

## Optional: allow only (allowlist)

If you want to **permit** the addon but still know who it is (logging, feature flags,
ad viewability experiments):

```text
User-Agent contains GW2-InGame-Helper  →  treat as "in-game helper"
```

Examples:

- Log a custom analytics dimension `client=gw2-in-game-helper`
- Serve a lighter layout for small viewports
- Keep ads enabled (the addon does not strip NitroPay / AdSense by policy)

---

## What not to rely on alone

These are **weak** signals for this client and will misclassify normal browsers or miss the addon:

| Signal | Why it’s weak here |
|--------|--------------------|
| Tiny `window.screen` | Helper reports real desktop metrics for `screen.*`; viewport is the ImGui panel |
| Missing GPU / WebGL | Software OSR is common under Wine/Proton; also appears on locked-down PCs |
| `navigator.webdriver` | Not set like Selenium |
| Generic “Chrome 150” | Many legitimate Chromium embeds share the major |
| IP / ASN alone | Same player IP as their desktop browser |

Use the **`GW2-InGame-Helper` UA token** as the contract.

---

## Client-side (page JS) — last resort

Edge rules are better. If you only control the page:

```js
if (/\bGW2-InGame-Helper\b/.test(navigator.userAgent)) {
  document.documentElement.innerHTML =
    '<p style="font:16px system-ui;padding:2rem">This site is not available inside ' +
    'GW2 In-Game Helper. Use <b>Open Ext</b> in the addon toolbar, or open us in a ' +
    'normal browser.</p>';
  // Optional: stop further app boot
  throw new Error('GW2-InGame-Helper blocked by publisher');
}
```

Or hide ads only:

```js
if (/\bGW2-InGame-Helper\b/.test(navigator.userAgent)) {
  document.documentElement.classList.add('no-helper-ads');
}
```

```css
html.no-helper-ads .nitropay,
html.no-helper-ads [data-nitropay],
html.no-helper-ads iframe[src*="googlesyndication"] {
  display: none !important;
}
```

---

## Ads vs full site block

| Goal | Suggestion |
|------|------------|
| No in-game browsing at all | Edge/WAF 403 on `GW2-InGame-Helper` |
| Allow reading, disable ads | Client class + CSS/JS, or ad-server key/value targeting on UA |
| Prefer desktop for login/OAuth | You do not need a special rule — Google/Discord often fail in embeds anyway; the addon already routes many of those via Open Ext |

---

## Verify

1. Install the addon, open your site in the in-game panel.
2. In your access logs / Cloudflare events, confirm `User-Agent` contains `GW2-InGame-Helper`.
3. Enable the block rule and reload — you should get your 403 / interstitial.
4. Click **Open Ext** in the addon toolbar — the same URL in the system browser should still work (normal Chrome UA, no token).

If logs show Chrome 150 **without** the token, the player is on an older helper build. Ask them to update the DLL from
[GitHub Releases](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases), or contact the maintainer below.

---
