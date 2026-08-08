# GW2 In-Game Helper v2.2.3.13

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 — Wiki, Snow Crows, MetaBattle, Guildjen, and more.
One DLL for Nexus — no memory reads. Chromium is **private CEF Stable 150**
(first-run download into `addons/GW2-InGame-Helper/cef/`).

Docs: [`CONTRIBUTING.md`](../CONTRIBUTING.md) · CEF / design [`WHITEPAPER.md`](WHITEPAPER.md) ·
DPS Logs [`DPS_LOGS.md`](DPS_LOGS.md) · API key [`API_KEY.md`](API_KEY.md) ·
Compliance [`COMPLIANCE.md`](COMPLIANCE.md)

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

On first helper open the DLL downloads the CEF runtime (~170MB) once unless you
pre-seed `cef-runtime-150-windows64.zip` (see whitepaper / CEF notes).

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper) ·
[latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll)

### Upgrading from the Beta DLL

1. Disable/remove `GW2-InGame-Helper-Beta.dll` from `addons/` (Beta uses signature
   `HELB`; shipping uses `HELP` — do not run both if you only want one data folder).
2. Install `GW2-InGame-Helper.dll`.
3. Data/CEF live under `addons/GW2-InGame-Helper/` — optionally move
   `addons/GW2-InGame-Helper-Beta/cef/` there to skip re-download.

---

## What’s new in 2.2.3.13

- **Watch control pad:** **Watch** / **About** tabs on the Start/Stop pad (how-to + limits).
- **Watch rail / keybind:** Toggles the control pad only — Mirror keeps running until Stop
  or Mirror close (no more hard `CloseAll` when reopening controls).
- **Stamps:** Helper `2242` · homepage `2230` · sites `s2214` · cheatsheets `c2228`
  · live panel `62` · raid food `9` · ui-chrome `uc31` · watchd `w10`

## What’s new in 2.2.3.12

- **Watch pad:** Side-rail companion above Settings — slim Start/Stop control; video
  opens in a separate **Watch Mirror** window. Look-only (playback stays in the
  system app). Unique viewfinder rail icon (`WatchView`).
- **Wine / Proton:** embedded `gw2igh-watchd` (stamp `w10`) — xdg-desktop-portal
  ScreenCast + PipeWire → `/dev/shm` (~60 FPS, up to **1280×720**). Daemon spawn/chmod is async with a
  space-safe `/tmp` binary so Start does not hitch the game thread.
- **Windows:** async GDI window capture (`PrintWindow` / `BitBlt`) with in-pad picker.
- **Watch stability / clarity:** Closing Mirror no longer frees the D3D SRV mid-frame
  (deferred GPU release). Capture max raised from 640×360 → 1280×720 so the mirror is not mush.
- **Stamps:** Helper `2242` · homepage `2230` · sites `s2214` · cheatsheets `c2228`
  · live panel `62` · raid food `9` · ui-chrome `uc31` · watchd `w10`

## What’s new in 2.2.3.11

- **Short CEF pages:** CEF OSR `100vh` does not give layout free space — pin
  `--app-h` from `window.innerHeight` and center content inside min-height wraps
  (API Check, hubs, Home, sheets, Raid Food, Legendary) so empty landscape shows
  above and below.
- **UI chrome:** Drop stretched `btn-frame` / `card-fill` on rail + HTML tiles (top-aligned
  strips); gold CSS/ImGui borders instead. Avoid no-op Browse hub reloads that Wine CEF
  treats as `STATUS_BREAKPOINT` (exit `2147483651`).
- **Stamps:** Helper `2242` · homepage `2230` · sites `s2214` · cheatsheets `c2228`
  · live panel `62` · raid food `9` · ui-chrome `uc31`

## What’s new in 2.2.3.10

- **User themes:** Drop-in `config/themes/<name>/theme.ini` color packs; Settings picker,
  Open folder / Reload; ImGui tokens + CEF `:root` inject on Home / Live / sheets
  (hardened against multi-click). See [`CONTRIBUTING.md`](../CONTRIBUTING.md).
- **UI chrome:** Richer curated plaque pack (btn frame/plate, card-fill, hero plate, …);
  side-rail hover/selected uses gold frame — stamp `uc31`.
- **Browse catalog:** Dropped redundant **Browse** row under Help — sites `s2214`.
- **Browse / Cheat Sheets tiles:** Centered, larger labels on the flat grey of card-fill
  (Categories, Cheat Sheets hub, Help / Wiki / favorites / every category page) —
  live panel `59`, cheatsheets `c2226`.
- **Browse Back / missing live pages:** Stamp-only live-panel invalidate (never delete
  `.html` CEF history still points at); helper recovers `ERR_FILE_NOT_FOUND` via `about:` —
  Helper `2242`.
- **Side rail:** No stretch gaps / gold divider strips between buttons; leftover height
  expands each row so the stack reaches the bottom (HELPER / TOOLS labels kept).
- **Stamps:** Helper `2242` · homepage `2228` · sites `s2214` · cheatsheets `c2226`
  · live panel `59` · raid food `7` · ui-chrome `uc31`

## What’s new in 2.2.3.9

- **Event alerts:** On-screen toasts when events are soon/live; default = all catalog events;
  **Alerts tracked** / **Alerts map** filters; persisted settings; **Place alert** drag-to-position.
- **Events polish:** Shared UTC timing helpers, next/then clocks, claim badges, wiki/MetaBattle links.
- **Instances sync:** Fractal level + daily fractal board; CM achievement overlays on open/Sync.
  Soft refresh while open skips the large achievements download; raids/dailies/FR fetch in parallel
  and paint first. Story stays local (quest crawl removed from the hot path). Strikes stay local.
- **API load control:** Process-wide `ApiBudget` (max concurrent GW2 HTTP) + `BgFetch` so Crafting
  details / Wallet / Vault / Instances do not stampede when switching Economy tabs.
- **Crafting:** Leveling-path discipline chips (Nexus-safe, no popup combo); Known recipes start
  collapsed; character cycle arrows; Plan / Known / Browse / Cart sub-tabs.
- **Map assist:** Opt-in Pathing world-map steer (`MapAssist`, default off) + optional waypoint tap
  (never auto-confirms teleport). Uses `GameLive` Mumble freshness; compliance exception documented.
- **Economy charts:** Per-item sample history (up to 120/id) fully persisted; ~90s poll of pinned items.
- **Completion:** Pack-sourced hearts/HP/AP; Explorer/Been There API overlay (throttled reopen);
  floors/pack structure split.
- **Farming:** Catalog/schedule modules; Events-linked UTC hints for HoT metas.
- **Stamps:** Helper `2241` · homepage `2227` · sites `s2213` · cheatsheets `c2224`
  · live panel `54` · raid food `7` · ui-chrome `uc28`

## What’s new in 2.2.3.8

- **Economy hub:** Stash, Trading (delivery + watchlist), Item lookup, and Crafting moved under
  Economy; Account keeps Overview / Progress / Unlocks / History. Lazy tab soft-kick on open.
- **Vault pad:** Wizard’s Vault is its own side-rail button (under Compass) with gold star icon
  (`561441`); still `Ctrl+Shift+V`.
- **Side rail:** COMPANIONS section removed — tools live under one TOOLS list; reordered
  (Compass → Vault → Events → Instances → Economy → Farming → Pathing…).
- **Rail icons:** Content UV fit so padded PNGs match dense ones; Pathing / Trail Tools /
  Vault icons packed in ui-chrome `uc27`.
- **Crafting known recipes:** Parallel bulk detail workers + disk cache; 429 backoff / stubs so
  “Loading details N / M” can finish instead of rate-limit spinning.
- **Instances / Completion:** Raids weekly clears from `/v2/account/raids`; Atlas strikes +
  festival scopes (carried with this ship).
- **Browse hub:** Transparent sticky category chips (no full-width dark TOC bar); cropped
  transparent browse-hero figure bottom-left.
- **Stamps:** Helper `2241` · homepage `2227` · sites `s2213` · cheatsheets `c2224`
  · live panel `53` · raid food `7` · ui-chrome `uc27`

## What’s new in 2.2.3.7

- **GPS pathfinding:** A* over pack-trail corridors + official waypoints when building the
  orange search guide (falls back to densified direct). Hardened graph size / rebuild hysteresis.
- **World GPS visuals:** denser smoothed ribbon, thicker guide, procedural chevrons.
- **Farming live nodes:** nearest Pathing pack markers on your map + GPS handoff.
- **Instances raids:** weekly clears sync from `/v2/account/raids` (W1–W8); story/fractals/strikes local.
- **Completion:** strikes + festival/(Public) clones in Atlas (scope chips); eager waypoint index.
- **Sources of truth:** Vault pad = Wizard’s Vault account progress; `about:live-dailies` =
  Today board (shared Vault cache + crafting + bosses); Daily/Weekly sheet = offline reference only.
- **Legendary discovery:** side-rail Ledger is primary; `about:live-progress` redirects to Ledger;
  Account → Progress keeps ImGui armory + Open Ledger.
- **Craft carts:** Crafting cart owns projects; Economy cart is TP shopping — “Send to Crafting plan”.
- **Stamps:** Helper `2241` · homepage `2227` · sites `s2213` · cheatsheets `c2224`
  · live panel `51` · raid food `7` · ui-chrome `uc23`

## What’s new in 2.2.3.6

- **Hero chrome:** Dual rim (`panel-edge` + `ink-edge`), translucent wash, amber gem crest
  centered on the side rail with **Game Helper** title to its right.
- **Title fade:** Title strip fades L→R onto the game (wash starts below the band) with a soft
  grey pocket behind − / X — Hero-style, not an opaque plate edge-to-edge.
- **Browse hub art:** Armored figure (`browse-hero`) on hub and every category page.
- **Side rail:** Icons auto-shrink to fit helper height (no nav scroll); matching top/bottom pad.
- **Browse stability:** Stop deleting open `live-browse-*.html` under CEF (fixes intermittent
  Chromium “Can't find the page”); stamp-only invalidate + Reload when already on hub.
- **No CEF context menu:** Right-click clears the Chromium menu (Back / Forward / Print / View
  source); OSR quick menu cancelled.
- **Crafting:** Known-recipes rail, multi-item craft cart, buy-vs-craft plan split into focused
  TUs; Economy cart **Plan** / **Plan first** handoff.
- **Stamps:** Helper `2239` · homepage `2227` · sites `s2213` · cheatsheets `c2223`
  · live panel `50` · raid food `5` · ui-chrome `uc23`

## What’s new in 2.2.3.5

- **Home while Home:** Pressing Home on the Browse hub is a no-op (no hub HTML rewrite +
  CEF reload). Same-URL navigates are skipped in the helper. Fixes Wine crash-loop
  (`exit=2147483651` / `STATUS_BREAKPOINT`) that disabled the browser helper.
- **Browse hub write:** Skip rewriting `live-browse-hub.html` when content is unchanged.
- **Stamps:** Helper `2237` · homepage `2227` · sites `s2213` · cheatsheets `c2223`
  · live panel `49` · raid food `5` · ui-chrome `uc21`

## What’s new in 2.2.3.4

- **Compact pad defaults:** Shared Compact (`440×480`) / Workbench (`560×640`) /
  Pathing (`640×700`) FirstUseEver sizes via `PadDock`; DPS Logs stay display-sized.
- **Icon side dock:** External compact rail (labels optional); curated ui-chrome icons
  (`uc21`).
- **Uniform fonts:** Global FontScale only (no per-window size multiplier); auto scale
  capped ~1.15×.
- **Wallet & Stash:** Parallel inventory+equipment per toon; progressive character
  publish; no dual InventoryData crawl on open.
- **DAT scrollbars:** Paint native scroll chrome after `End()` on each window’s draw list
  (not the global foreground) so other pads no longer slide under a gutter; compose
  top (`154969`) / mid (`154970`) / grip (`154971`) / cap (`154973`) / chevron (`155031`)
  from `UI Textures` (`Gw2UiPadChrome` / `Title` / `Scroll` split).
- **Helper gold CEF bars:** Intentional gold OSR theme — pads (ImGui DAT) and Helper
  (CEF CSS) stay on different stacks by design.
- **Minimize restore:** Pad size constraints allow title-strip height when collapsed.
- **Stamps:** Helper `2236` · homepage `2227` · sites `s2213` · cheatsheets `c2218`
  · live panel `46` · raid food `5` · ui-chrome `uc21`

## What’s new in 2.2.3.3

- **Title bar strip:** DAT `156046` metal plate flush with the window top (opaque pack
  fringe); larger medallion crest + − / X hit targets.
- **Hero-style rim:** Soft `ink-edge` (155967 brush fringe) on bottom + sides — no thick
  black matte frame; title bar left unchanged.
- **CEF fixed backgrounds:** Tall Browse/hub pages keep wash pinned while content scrolls.
- **Stamps:** Helper `2236` · homepage `2222` · sites `s2213` · cheatsheets `c2218`
  · live panel `46` · raid food `5` · ui-chrome `uc14`

## What’s new in 2.2.3.2

- **Title crest:** High-res gold medallion (`crest-hero`) replaces the satchel/sword emblem
  on Hero-style pad title bars.
- **Opaque panel wash:** `panel-wash` fill for ImGui pads + CEF pages — no feathered
  StandardWindow edges showing the game through.
- **Fixed page background:** Browse/hub HTML pins the wash with `background-attachment:
  fixed` so tall pages scroll over the texture (no mid-page black cut).
- **Chrome layout:** Crest hangs past the left frame; title controls sit flush-right;
  rail/OSR gutters tightened so the wash reaches the panel edges.
- **Stamps:** Helper `2235` · homepage `2222` · sites `s2213` · cheatsheets `c2218`
  · live panel `46` · raid food `5` · ui-chrome `uc9`

## What’s new in 2.2.3.1

- **Hero-style title bars:** Dark charcoal plate from StandardWindow top strip; emblem
  overhangs the bar; cream title with dark halo (in-game Hero panel read).
- **Title controls:** Contacts-style `button-exit` X + gold minimize bar (no ImGui frame
  borders on those glyphs); stay inside content clip / scrollbar gutter.
- **Side rail fit (Windows):** Measure real labels + icon reserve; raise max width with
  FontScale so Completion / COMPANIONS no longer clip on native Windows fonts.
- **Pad readability:** Eased cream text + gold checkbox/input frame borders; Blish fill
  darkened just enough for contrast without crushing the chrome.
- **Stamps:** Helper `2234` · homepage `2222` · sites `s2213` · cheatsheets `c2218`
  · live panel `46` · raid food `5` · ui-chrome `uc6`

## What’s new in 2.2.3.0


- **Immersive ImGui chrome:** Pads (and the main Helper) use Blish-style
  **StandardWindow** fill (`155985`) + emblem (`156022`) with a custom title bar
  (`NoTitleBar` / `NoBackground`) — minimize / Contacts-style exit glyphs, not stock
  ImGui chrome.
- **UI chrome pack:** Curated textures in `data/ui-chrome/` pack into the DLL, extract
  to `addons/…/ui-chrome/` (stamp `uc6`). ArenaNet owns those assets — not MIT.
- **Pad shell polish:** Title close/minimize stay inside the content clip; pads clamp
  so the title bar stays on-screen; Compass min size / layout (slider labels above).
- **Immersive HTML theme:** Homepage, cheat sheets, Browse/CheatSheets hubs, live panels,
  Raid Food, and Legendary Ledger deepen parchment/gold tokens (`HelperTheme` /
  `ImmersiveShell`) to match ImGui pads.
- **Install:** `make install` removes leftover `GW2-InGame-Helper-Beta.dll` so Beta
  cannot shadow shipping; use `make install-beta` only when you want Beta.
- **Version source:** `src/app/AddonVersion.h` is the single shipping revision
  (feeds `entry.cpp` / shipping version).
- **Stamps:** Helper `2233` · homepage `2219` · sites `s2213` · cheatsheets `c2215`
  · live panel `43` · raid food `5` · ui-chrome `uc6`

## What’s new in 2.2.2.0

- **Completion companion:** Side-rail **Completion** pad — checklist + Atlas + route
  modes (**Nearest** / **Zone loop**). Nearest = closest remaining; Zone loop =
  checklist order on the focus map (never falls back to Nearest). **Open Pathing**
  is a one-shot Lady MC handoff (not a route mode). GPS via Pathing search guide;
  auto-arrive from Mumble proximity. Persist `config/completion-checklist.txt` /
  `config/completion-favorites.txt`. Curated **78 Public Tyria zones**.
- **Farming companion:** Side-rail **Farming** pad — curated run checklists with
  Pathing handoff + fishing catch log (`config/farming-state.txt`).
- **Overlays:** Floating GPS arrow toward the active guide; short zone-entry banner
  on map change.
- **Panel keybinds:** Addon-owned chords in **Settings → Keybinds** (capture / clear /
  reset). Legacy Nexus `KB_HELPER_*` panel ids deregistered on load. Helper open stays
  Ctrl+Shift+H / QuickAccess.
- **IPC DACLs:** Named `CreateFileMapping` / `CreateEvent` objects use a current-user
  DACL (falls back to Win32 defaults if ACL setup fails).
- **PathingLua:** Expanded Blish-shaped APIs (Storage, Debug, Category/Marker visibility,
  Pack:Require, instance helpers) — still opt-in via Features.
- **Trail Tools desk:** Trails / Markers can open as their own **XML desks** (shared
  OverlayData New/Load/Save/Save As; Combined or Split). Up to **four TrailsN** and
  **four MarkersN** collapsible raw editors at once (cascaded; keybind recording
  targets the focused TrailsN). Hub tabs stay usable; **Insert into XML** upserts
  before Save. Empty `PathingEnabled` on first load stays off (no Lady auto-enable).
- **Immersive HTML theme (initial):** Homepage / hubs / live panels parchment tokens
  aligned with ImGui (`HelperTheme`).
- **Stamps:** Helper `2232` · homepage `2218` · sites `s2213` · cheatsheets `c2214`
  · live panel `42` · raid food `4` · ui-chrome `uc2`

## What’s new in 2.2.1.0

- **Helper lifecycle:** Closing Browse soon after open is no longer treated as a helper
  crash (no quick-death lockout). Unexpected helper deaths still disable relaunch and
  surface exit code / uptime in status.
- **Stamps:** Helper `2224` · homepage `2212` · sites `s2213` · cheatsheets `c2211`
  · live panel `39` (as shipped at tag)

## What’s new in 2.2.0.20

- **Economy companion pad:** Side-rail Flip Finder (fee-adjusted net from official commerce
  prices), local buy/sell charts from scan history, read-only crafting cart with Account Crafting
  handoff, BLTC opens in a new in-addon tab. Verified material seed ids; status shows API/HTTP
  errors in red.
- **Instances companion pad:** Story / fractal / raid / strike checklist journal with per-entry
  progress, reset, and wiki search in a new tab.
- **Side rail:** Shorter labels + scrollable companions section; Browse / Ledger / Sheets / API Check
  open sites in a **new tab** (not only the active tab).
- **Helper maintainability:** Split `HelperPaths` / `HelperResolve` / `HelperBrowseActions` out of
  `HelperState`; homepage + Browse hub credit line.
- **Stamps:** Helper `2224` · homepage `2212` · sites `s2213` · cheatsheets `c2211`
  · live panel `39`

## What’s new in 2.2.0.19

- **Trail Tools pads:** Live / Trails / Markers / Pack / Keybinds in one side-rail window;
  Trails and Markers can optionally **Open in window** (collapse to title bar). Draft preview
  while the pad is open.
- **Trail authoring:** New / Load… / Save / Save As… for `.trl`; window title shows the file stem;
  map-only vs map+vector segments; select nearest / move to feet / delete; default category + Trail
  XML line; last `.trl` folder remembered in settings.
- **XML layout:** Combined one-file OverlayData, or **Split** `Pack_Menu.xml` + `Pack_Data.xml`
  (MarkerCategory menu vs `<POIs>` data). Pathing already merges every `.xml` in the taco.
- **Trail Tools keybinds:** Addon-polled chords for trail start/pause/section/delete and marker
  delete, plus **10 place-marker slots** (category + chord) for mount/route markers along a trail.
- **Browse favorite folders (hub):** Side-rail Browse hub has **+ Folder** and **⇄** on each
  favorite to move it into Unfiled or a named folder (`config/favorites.json`). Tab-bar **+**
  picker also right-click → Move to folder.
- **Stamps:** Helper `2223` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `38`

## What’s new in 2.2.0.18

- **Trail Tools Looks + Blish parity:** Pack-tab look presets (chevron/ribbon/dashed/heart;
  disc/pin/star/square) with tint, trailScale/iconSize, fade. WYSIWYG textured draft
  preview on compass/world GPS. UTC `schedule` / `schedule-duration` filter at draw time.
  Texture browser, import `.taco`, multi-trail editing, richer POI attrs (behavior, tips,
  script-*, hide/show), draft session persist.
- **PathingLua (opt-in, default off):** Blish-shaped subset under Pathing → Features —
  `script-once/trigger/filter/tick/focus`, Marker/Trail mutators, `Menu.Add`, CDN
  `SetTexture(id)` via `assets.gw2dat.com`, `GetBehavior`, World marker/trail lookups,
  Pack:CreateMarker, Mumble/Event/User. Vendored Lua 5.4.7 (`deps/lua`).
- **Stamps:** Helper `2221` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `38` (helper stamp unchanged)

## What’s new in 2.2.0.17

- **Trail Tools:** New Tools side-rail pad to author TacO/Blish marker packs in-addon —
  live Map ID / XYZ, trail recording (Insert Map / Vector, section breaks), drop markers,
  category tree + XML, build `.taco` into `pathing/`, then reload Pathing. Magenta draft
  preview on compass + world GPS while the pad is open. Workspace under
  `pathing/authoring/<PackName>/`.
- **Stamps:** Helper `2221` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `38` (unchanged from 2.2.0.16)

## What’s new in 2.2.0.16

- **Favorite folders:** Create named folders from the Browse hub (**+ Folder**). Tap **⇄** on a
  favorite to move it into a folder (or Unfiled). ImGui tab-bar picker: right-click → Move to
  folder; drag onto a folder header to refile. Delete a folder to send its sites back to Unfiled.
  Stored in `config/favorites.json` (migrates from legacy `FavoriteIds=`).
- **Stamps:** Helper `2220` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `38`

## What’s new in 2.2.0.15

- **Favorites on Browse pages:** Starring no longer deletes and rebuilds the open
  category HTML (Wiki is ~1MB / 2k+ sites). That race caused Nexus
  `Failed to write Live panel HTML` when adding a second favorite. Stars update
  in-page; the Browse hub still refreshes when open. Duplicate CEF click events
  are debounced.
- **Stamps:** Helper `2219` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `37`

## What’s new in 2.2.0.14

- **`config/` data folder:** Notes, profiles, session history, confirmed waypoints,
  log-index, marker behaviors, and `ei-helper.conf` live under
  `addons/GW2-InGame-Helper/config/`. Existing root copies migrate automatically on
  first open (dest wins if both exist). `make install` preserves `config/` and
  `settings.ini`.
- **Favorites persist:** Favorites are no longer pruned before the Browse catalog
  loads (which wiped them on every startup). Star / reorder saves immediately;
  favorite ID buffer raised to 4096.
- **Side rail title:** Gold **IN-GAME HELPER** label moved from the top chrome into
  the side rail above Browse.
- **Stamps:** Helper `2218` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `37`

## What’s new in 2.2.0.13

- **Side rail layout:** Browse → Legendary Ledger → Cheat Sheets → GW2 API Check,
  then a **Tools** section (Account, Compass, Pathing, Events, Notes, DPS Logs),
  then Settings.
- **Favorites stay current:** Toolbar / Browse-row stars and reorder invalidate the
  Browse hub and reload it when open. Hub HTML always rebuilds on open so the
  Favorites list cannot go stale.
- **Legendary icons:** Ledger list and detail pages show each item’s official
  in-game icon from the GW2 API (cached ~7 days), with letter fallback if missing.
- **Stamps:** Helper `2217` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `37`

## What’s new in 2.2.0.12

- **Browse is Home:** Home button, empty-tab landing, and restored How-to-use
  landing tabs open **Browse** (`about:browse-hub`). Saved `ActiveSiteId=home` /
  `DefaultSiteId=home` migrate to `browse`. How to use stays in the catalog.
- **Ledger Armory line:** Shows the legendary **name** (e.g. Ad Infinitum), not `#id`.
- **Instant craft tree reopen:** Opening a legendary no longer wipes the craft-tree
  cache every time. Ready detail pages load from disk immediately; Sync still forces
  a full rebuild. Detail HTML TTL is 2 hours.
- **Stamps:** Helper `2216` · homepage `2210` · sites `s2213` · cheatsheets `c2211`
  · live panel `36`

## What’s new in 2.2.0.11

- **Browse HTML hub:** Side rail **Browse** opens themed `about:browse-hub`
  (Favorites + category index, client-side filter). Each category is its own
  `about:browse-cat-…` page with sections/subsections and site buttons that open
  in a **new helper tab**. Stars toggle Favorites (`FavoriteIds=`). Category pages
  with multiple sections get sticky jump buttons at the top (esp. Wiki). Tab bar
  **+** and Settings default-site picker keep the ImGui Browse popup. Cheat Sheets
  stay on their own side-rail hub.
- **Legendary Ledger:** Gold/dark helper theme (matched to Browse / Cheat Sheets).
  Own side-rail button under Notes (`about:legendary-vault`); removed from the
  Cheat Sheets hub. Cheat sheet tiles open in a **new helper tab**.
- **Legendary Ledger armory:** Opening the Ledger (or clicking the brand / All
  legendaries) always re-fetches `/v2/account/legendaryarmory` so Owned / Missing
  stays current. Vault HTML TTL is 45s. Badges show **Owned ×N** (and piece counts
  for multi-id sets).
- **Auto craft tree:** Opening a legendary always rebuilds the gifts → mats tree
  (even when already owned, so you can plan a second copy). No Sync button required.
- **Open in Wiki / Account Crafting:** Matching gold CTAs at the top of each
  detail page — Wiki opens `wiki.guildwars2.com` in a **new helper tab**; Account
  Crafting opens the Account pad on the Crafting tab and queues the plan.
- **Faster craft sync:** Wiki-first for gifts/legendaries (skip failed station
  searches), up to 16 parallel recipe lookups per depth, parallel wiki fallbacks,
  and inventory fetch overlapped with the root recipe lookup.
- **Default landing:** Home / first tab uses **Browse** (`about:browse-hub`) instead of
  How to use. How to use remains under Help → Getting Started.
- **Stamps:** Helper `2215` · homepage `2210` · sites `s2212` · cheatsheets `c2211`
  · live panel `35`

## What’s new in 2.2.0.10

- **Addon data layout:** Generated HTML and home assets live under `pages/`; live API
  JSON under `live/cache/`; unlocks/waypoints/stash under `cache/`; helper↔DLL
  `*-cmd.txt` under `cmds/`. Config, helper exe, `cef/`, `pathing/`, `ei/`, and
  `cheatsheets/` stay at the data root.
- **Upgrade migration:** On load, legacy root copies of moved files are deleted;
  stamps force regenerate into the new folders. `make install` wipes the nested
  trees as well as root orphans.
- **Stamps:** Helper `2210` · homepage `2210` · sites `s2211` · cheatsheets `c2211`
  · live panel `28`

## What’s new in 2.2.0.9

- **Cheat Sheets hub:** Side rail **Cheat Sheets** (below Notes) opens themed
  `about:cheatsheets-hub` with a tile for every offline sheet. Browse **Cheat Sheets**
  category is hidden from the picker; individual sheet URLs still work. Hub tiles use
  real `file:///` hrefs (Chromium blocks unknown `about:` before the helper can rewrite).
- **GW2 Legendary Ledger (live):** Embedded catalog → Live `about:legendary-vault` —
  search, type filters, Missing chip, A–Z, Owned/Missing from `/v2/account/legendaryarmory`.
  Sync craft tree expands gifts → mats with have/need and %; themed loading/detail pages;
  bulk item-name resolve; Open in Account Crafting. Perfected Envoy covers Light / Medium /
  Heavy. Progress→Plan covers vendor legendaries (e.g. Eikasia). Idea credit:
  **Dark Sorcerer.6420**.
- **Navigation hardening:** Helper never hands CEF raw `about:` for Ledger CTAs
  (`?gw2igh-leg-open=` / `?gw2igh-craft-plan=` / `?gw2igh-leg-sync=`); Live panel finish
  uses Reload when already on the loading shell (same-file Navigate was a no-op).
- **Stamps:** Helper `2209` · homepage `2209` · sites `s2210` · cheatsheets `c2210`

## What’s new in 2.2.0.8

- **GW2 Legendary Ledger:** Offline `about:legendary-vault` sheet — alphabetical GW2
  legendaries collection (search, type filters, A–Z jump, acquisition notes). No
  in-page API Settings; uses the helper’s existing API key slot for account features.
  Idea credit: **Dark Sorcerer.6420**. Browse: Cheat Sheets → Account.
- **Legendary Crafting Ledger:** Tools → Account link to
  [forge-legend-track.base44.app](https://forge-legend-track.base44.app/).
- **Stamps:** Helper `2208` · homepage `2208` · sites `s2208` · cheatsheets `c2208`

## What’s new in 2.2.0.7

- **JsonView:** Shared bounds-checked GW2 JSON scrapers (`src/api/JsonView.h`) using
  `std::string_view` and a C++17 `Bytes` span stand-in. LivePanels, Unlocks,
  Inventory, Vault, and waypoints floor parsing no longer call `atoi`/`atoll`/`strtod`
  on raw `c_str()` offsets. Host smoke: `make test-json-view` (wired into CI).
- **World GPS range:** Overview **GPS range (m)** now drives draw/activation distance
  (removed ~480m / ~320m floors that made the slider look broken). Shrinking range
  refreshes the nearby cache immediately.
- **Marker size:** Separate Overview sliders — **World markers** (`WorldMarkerScale`,
  default **2.0×**) for in-world GPS icons, and **Compass icons** (`CompassMarkerScale`,
  default **1.0×**) for the stock compass / minimap.
- **Marker clear:** Overview **Marker clear** (`WorldMarkerPlayerClear`, default **1**)
  soft-clears world markers ~2–5.5 m from you (independent of trail **Player clear**).
  **0** = no hole. Mount / Barefoot shortcut icons keep a smaller bubble.
- **Pad layout:** Side-rail pad bodies keep a small right gutter so slider labels and
  wrapped text sit clear of the scrollbar (`PadNav` + theme padding).
- **Stamps:** Helper `2207` · homepage `2207` · sites `s2207` · cheatsheets `c2207`

## What’s new in 2.2.0.6

- **GW2 API Check:** New helper side-rail button (under Settings) opens a local
  diagnostic page that probes official `api.guildwars2.com` endpoints (public +
  saved API key). Requests run in parallel so a full check finishes in about one
  round-trip instead of summing every timeout. Not affiliated with third-party
  status sites.
- **Stamps:** Helper `2206` · homepage `2206` · sites `s2206` · cheatsheets `c2206`

## What’s new in 2.2.0.5

- **UI pads:** Help text wraps to the real content width after side rails (no more
  clipped Categories blurbs). Muted theme colors are brighter for readability.
  Notes / Compass / Pathing Features·Route share the same wrap helpers.
- **DPS Logs filters:** Column width fits checkbox / search labels at font scale;
  group-by and auto-parse labels wrap instead of clipping; Open folder creates the
  log directory when missing.
- **Pathing enables:** Lady Features toggles merge with Tekkit/Hero (no wipe).
  Map Completion presets strip only MC paths and restore non-MC Tekkit siblings
  when a broad `tw_guides` root was on. MC preset helpers live in
  `PathingTrailsPresetsMc.cpp`.
- **Stamps:** Helper `2205` · homepage `2205` · sites `s2205` · cheatsheets `c2205`

## What’s new in 2.2.0.4

- **Lady Features (current map):** Barefoot / WP Only / With Mounts stay exclusive
  map routes. New independent toggles: **Hearts** (`heartpath`) and **Hero Point
  Train** (`legs.hp` trails + icons only). WP Only is waypoint trails only (no
  markers). Barefoot keeps `bfs` shortcuts; With Mounts is mount route + mount
  markers. Features focus mode no longer spills the rest of the Lady pack.
- **World GPS:** Edition switches clear sticky ribbons; WP/HP/heart paths draw
  full TacO sections (minimap-complete routes no longer clipped in-world).
  Mount/`bfs` guide icons use a smaller avatar soft-clear so they stay visible
  on the path. Load ranking prefers the active Features edition.
- **Stamps:** Helper `2204` · homepage `2204` · sites `s2204` · cheatsheets `c2204`

## What’s new in 2.2.0.3

- **Settings pad:** Side-rail **Settings** holds default landing site, opacity,
  font scale / auto, warm CEF, and API key. Nexus Options is a short stub that
  opens the pad. Pathing / compass / TP / phone-companion toggles stay on their
  own pads (Sync QR removed from the shipping build).
- **Aspect layouts:** First-open window defaults for 16:9 / 21:9 / 32:9
  (`AspectLayout`); pads use shared dock helpers.
- **DPS Logs upload:** Queue drain race fixed (keep uploading while more files
  arrive); clearer dps.report status / tooltips; ~48 MB upload cap.
- **World GPS hearts:** Lady heart trails keep pack yellow tint, 1.5× ribbon
  width, and the same UV flow as arrow trails. Trail textures prioritize in the
  icon queue (no solid-yellow fallback when the PNG is missing). Sticky cache
  matches by geometry; nearby hearts prefer full TacO sections.
- **Docs:** Contributor refs for Pathing / Account / Modules / Nav+ads expanded.
- **Stamps:** Helper `2203` · homepage `2203` · sites `s2203` · cheatsheets `c2203`

## What’s new in 2.2.0.2

- **D3D world GPS:** In-world trails use Nexus SwapChain D3D11 upright ribbons
  (Blish-style pack chevrons, fixed UV tile period, animspeed flow toward the
  route, soft player clear). Markers stay on ImGui. No Present hooks. GPS width
  **1.0×** = pack `trailScale` (Lady edition width bias removed). Along-path
  sampling, sticky cache + hysteresis reduce missing/blinking ribbons; compass
  still ImGui 2D.
- **Code layout:** Prefer **≤500 lines** per `.cpp`. Pathing / account / browse /
  logs / UI / entry / helper / browser megafiles split by concern (Shared +
  focused TUs). Former `TekkitTrails*` modules renamed `PathingTrails*` (Tekkit
  pack branding unchanged).
- **Stamps:** Helper `2202` · homepage `2202` · sites `s2202` · cheatsheets `c2202`

## What’s new in 2.2.0.1

- **Lady Features:** Barefoot / With Mounts / WP Only are **mutually exclusive**
  map-completion editions (no stacking). Enabling an edition turns on Lady
  categories when needed. Defaults: Barefoot on, With Mounts off.
- **World GPS (earlier 2.2.0.1):** Sticky nearby-cache and denser sampling for
  sparse / WP Only routes; Overview **Player clear** (default **1**, **0** = full
  path).
- **First run:** Empty `PathingEnabled` (legacy `TekkitEnabled=`) auto-enables
  Lady categories so trails appear without hunting Categories.
- **UI scale:** Per-panel font scale from Options × window size (`UiScale`);
  optional mild auto scale on tall displays; pads no longer touch Nexus
  `FontGlobalScale`.
- **Settings:** Longer enabled-category read buffer; persists as `PathingEnabled`
  (still loads legacy `TekkitEnabled` / `ShowTekkitTrails`); `SaveNow` on unload;
  Lady edition keys persist and normalize legacy multi-on configs.
- **Unload:** Nexus Disable allowed (`AF_None`) — pads close and CEF helper stops
  before deregister; prefer a full GW2 restart after replacing the DLL on disk.
- **Stamps:** Helper `2201` · homepage `2201` · sites `s2201` · cheatsheets `c2201`

## What’s new in 2.2.0.0

- **Product:** Former Beta channel features ship in `GW2-InGame-Helper.dll`
  (signature `HELP`, GitHub updates). Experimental Beta builds remain a separate
  `HELB` / `GW2-InGame-Helper-Beta` identity when needed.
- **Helper chrome:** Left **side rail** for Browse / Account / Pathing / Events /
  DPS Logs / Notes / Compass; favorite ★ beside Find. Floating pad positions and
  sizes persist in `settings.ini`.
- **Account hub:** Unlocks, inventory, session history, and wrapping chip rows;
  Overview / Stash / Vault / Trading / Item / Crafting / Progress stay on the
  official API. Per-character profiles for routing progress.
- **Pathing:** Curated packs auto-download into `pathing/`: **Tekkit** All-In-One,
  **Lady Elyssa** Guides + Achievements, and **Hero's Marker Pack** (QuitarHero);
  marker behaviors + Features / Categories; packs list fills Overview; trail-start
  **GPS** routing; TacO `.trl` section breaks and duplicate AIO pack skip (from
  2.1.0.4) retained. Map Completion Skyscale / All Lady / All Hero / All off
  presets. **Find nearest waypoints** uses the public waypoint index and works
  with **no categories enabled** (anchors on your position; orange guide toward
  the closest WP); with packs on, prefers trail start when available.
- **Direction compass:** Side-rail **Compass** — world N/E/S/W (letter size +
  world radius sliders); independent of Tekkit compass-trail overlay.
- **Browse catalog:** News Digest under **Help → News**; removed Live category
  and Fashion Wishlist; Addon Development category; catalog **2,718** entries;
  sites stamp `s2200`.
- **DPS Logs:** Auto-parse after scan (default on); toolbar **Parse**; wider
  Filters pane; one **Search** field (file + encounter).
- **Stamps:** Helper `2200` · homepage `2200` · sites `s2200` · cheatsheets `c2200`

## What’s new in 2.1.0.4

- **Direction compass:** Toolbar **Compass** — world N/E/S/W around the character
  (Nexus `FontBig`, letter size + world radius sliders); gold theme; independent of
  Tekkit compass-trail overlay. (Heading strip / bearing / edit mode removed.)
- **Pathing presets:** Map Completion **Skyscale** beside Foot / Griffon; **All Lady**
  (`legs` + `leag`); **All off** clears Tekkit, Lady, and MC.
- **Pathing trails:** Honor TacO/Blish/Taimi `.trl` **section breaks** (`(0,0,0)`);
  skip duplicate Tekkit AIO pack aliases; tighter compass/GPS drawing (no map-wide
  spaghetti / stacked ribbons); soft-hide POI markers near the avatar.
- **Browse:** New **Addon Development** category (Raidcore under Nexus, GitHub under
  Source); catalog **2,719** entries; sites stamp `s2104`.
- **Stamps:** Helper `2103` · homepage `2104` · sites `s2104` · cheatsheets `c2103`

## What’s new in 2.1.0.3

- **Maintainability:** Modularize god files — Sites, UI Browse/Options, LogManager,
  LivePanels, Tekkit, WikiBrowser, and helper nav/OSR into focused translation units;
  CI (`make ci`), onboarding, architecture / whitepaper / kernel docs.
- **Browse catalog:** Runtime schema-v2 `sites.json` (embedded + extracted) replaces
  generated `Sites.gen.cpp`; edit `addons/GW2-InGame-Helper/sites.json` and restart
  for no-rebuild tweaks.
- **UI input state:** File-local `UiContext` owns helper focus / game-input routing flags.
- **Cheat sheets:** Offline pages live in `data/cheatsheets/` (embedded zip extract).
- **Pathing:** Panel renamed from Tekkit’s Guides; auto-downloads latest **Tekkit**
  All-In-One + **Lady Elyssa** packs (GitHub / Tekkit CDN) into `pathing/`; user
  `.taco` files are kept. Credits for both authors in-panel.
- **Stamps:** Helper `2103` · homepage `2104` · sites `s2103` · cheatsheets `c2103`

## What’s new in 2.1.0.2

- **DPS Logs filters:** Mode radios wrap to two rows so **CM** / **LCM** labels are not
  clipped in the narrow filter pane.
- **Progress Wiki:** Legendary armory **Wiki** opens by item **name** (wiki does not
  resolve numeric API ids). Same fix on the Live progress page (panel ver 19).
- **Stamps:** Helper `2102` · homepage `2102`

## What’s new in 2.1.0.1

- **Shared pad theme:** Account, Notes, Events, DPS Logs, Tekkit, Stash, Vault, TP,
  and Item Lookup use the same gold/dark helper chrome as Browse.
- **Waypoints fix:** Floor JSON parser now indexes **all** POIs per map (was ~1 each);
  cache schema v4; **Reload** re-downloads instead of reusing a bad cache.
- **Pad filters without popup combos:** Stash location chips; DPS Logs / Progress /
  Notes kind use in-window radios — Nexus was eating ImGui dropdown popup clicks.
- **DPS Logs layout:** Near-full client default size, wider middle list, group-by
  sections expand on open; KillProof tab + group-by default from 2.1.0.0 retained.
- **Stamps:** Helper `2201` · homepage `2201`

## What’s new in 2.1.0.0

- **Account pad:** Toolbar **Account** — tabbed Overview / Stash / Vault / Trading /
  Item / Crafting / Progress on the official API (original ImGui UI). Wizard’s Vault
  dailies and legendary / character progress moved out of Browse → Live into this pad.
- **Crafting / Progress:** Daily crafts, recipe tree + mat ownership / TP cost;
  legendary armory + character roster under Account tabs.
- **Notes → Waypoints:** Official API waypoint / POI search (by name, map, or current
  Mumble map) with chat-code copy.
- **DPS Logs:** ArcDPS EVTC / ZEVTC browser — Elite Insights CLI auto-install
  (`addons/.../ei/`), parse to JSON, filters, players / guilds / fastest, dps.report
  upload + report-meta hydrate. Requires **.NET 8 Desktop Runtime** in the Windows /
  Proton prefix — see [`DPS_LOGS.md`](DPS_LOGS.md). Layout mirrors ArcDPS Log Manager
  (filters | list | detail): display-scaled default size, proportional filter column,
  persisted list|detail splitter + window geometry; **Group by encounter** on by
  default (collapsible boss sections). Players / Guilds / **KillProof** show the
  **selected log** only. KillProof tab loads LI / LD / UFE + encounter tokens from
  killproof.me (public profiles).
- **Help setup pages:** Browse → Help → **DPS Log Setup Help** and **API Key Setup**
  (offline `about:` sheets; mirror [`DPS_LOGS.md`](DPS_LOGS.md) / [`API_KEY.md`](API_KEY.md)).
- **Keybinds:** Panel toggles registered with Nexus — Account `Ctrl+Shift+A`,
  Tekkit `Ctrl+Shift+G`, Events `Ctrl+Shift+E`, Notes `Ctrl+Shift+N` (rebind under
  Nexus Options → Keybinds).
- **Toolbar:** Browse stays left of Account on the pads row; Notes tooltip covers
  waypoints.
- **Install fix:** `make install` clears `*.ver` / `*.ok` with extracted HTML so
  stale homepage stamps cannot leave CEF on a missing `file:///…/helper-home.html`.
  Saved tabs that still point at those file URLs remap to `about:` builtins.
- **Browse:** Removed Live Dailies / Live Progress and Wiki Easy Objectives rows
  (covered by Account). Catalog **2,718** entries.
- **Stamps:** Helper `2100` · homepage `2100`

## What’s new in 2.0.2.11

- **gw2efficiency ads:** CEF lays out that host at a full desktop size (1920×≥900)
  so NitroPay `matchMedia` slots (side rails, large footers, etc.) unlock like a
  normal browser. The panel scales that view to fit; clicks stay mapped correctly.
  Other sites are unchanged.
- **Stamps:** Helper `2051` · homepage `2014`

## What’s new in 2.0.2.10

- **Viewability:** CEF is marked `was_hidden` when the helper is collapsed, the page
  slot is tiny, or fully off-screen — so ads are less likely to record 0%-viewable
  impressions while the player cannot see the panel. Process stays alive (no hitch
  on expand); size uses hysteresis so resize does not flap visibility; pending
  navigations are not cleared on occlusion. Opacity is not a hide gate (low opacity
  must still render). Full close still follows **Keep browser warm**.
- **Stamps:** Helper `2051` · homepage `2014`

## What’s new in 2.0.2.9

- **Publisher ID:** User-Agent ends with product token `GW2-InGame-Helper` so sites
  can allow or deny this client cleanly — see [`PUBLISHER_ACCESS.md`](PUBLISHER_ACCESS.md)
  (Cloudflare / nginx / Apache / page-JS examples)
- **Stamps:** Helper `2051` · homepage `2014`

## What’s new in 2.0.2.8

- **World Events:** Toolbar **Events** — UTC timers for bosses / metas / other world
  schedules; optional API claim marks; searchable sections (invasions / festivals /
  fractals stay collapsed until opened or tracked)
- **Tekkit's Guides:** Toolbar **Tekkit** — load Tekkit’s All-In-One `.taco` from
  `addons/GW2-InGame-Helper/pathing/` (© Tekkit's Workshop, used with permission);
  read-only MumbleLink compass trails + in-world GPS overlays (display only, no automation)
- **OSR screen metrics:** `GetScreenInfo` now fills `rect` / `available_rect` from the
  real primary monitor and Windows work area (`GetSystemMetrics` /
  `SPI_GETWORKAREA`). `GetViewRect` remains the ImGui overlay size. Keeps
  `device_scale_factor` at `1.0` so IPC mouse and OSR paint stay in view pixels.
  JavaScript `screen.width` / `screen.height` no longer match the tiny panel —
  a common anti-bot / non-billable impression signal. Does **not** make OSR a full
  desktop browser substitute (viewability %, click trackers, etc. remain separate).
- **Snow Crows ads:** Stopped setting `pointer-events:none` on NitroPay / ad iframes
  (that made every ad click a silent no-op). Ads stay at a low z-index under the
  elevated header so Profile/Inbox remain clickable; clicks on the ad itself still
  reach the creative and Open Ext / tracker handoff.
- **Stamps:** Helper `2049` · homepage `2014`

## What’s new in 2.0.2.7

- **Item Lookup:** Toolbar **Item** — paste a chat code, item ID, or name; official API
  returns name/rarity/TP prices with Wiki / BLTC / Add to TP. Free-floating (not docked)
- **Wallet & Stash:** Toolbar **Wallet** — searchable currencies, material storage, bank,
  shared inventory, and per-character bags. Parallel account fetch, then character bags;
  disk name cache (`stash-names.cache`). Needs API scopes **account**, **wallet**,
  **inventories**, **characters**. Default window size matches Notes (~420×560)
- **Vault pad:** Toolbar **Vault** — free-floating Dailies & Wizard’s Vault (same API as
  Browse → Live); season + daily/weekly/special objectives with UTC reset countdowns
- **TP Watchlist:** Optional sell-price alerts; Add to TP from Item Lookup no longer
  re-docks an already-open TP window
- **Browse:** Official sites folded into **Help**; Fast Farming under **Guides** (no
  separate Official / Farming categories)
- **OSR `<select>`:** In-page dropdown polyfill on all sites — native PET_POPUP under
  CEF OSR was crashing the helper on Windows (looked like constant page refreshes).
  Menu dismisses on page scroll; ghost mouse-up after popup hide is swallowed
- **Favorites QR:** Larger quiet zone so companion QR scan is more reliable
- **Stamps:** Helper `2047` · homepage `2014`

## What’s new in 2.0.2.6

- **Live panels:** Browse → Live — Dailies & Wizard’s Vault (optional API key), news/patch
  digest, fashion wishlist, legendaries & characters. Built as offline HTML from read-only
  official API / RSS / wiki; still one DLL
- **API speed:** Parallel WinHTTP GETs (per-thread sessions), short-TTL account caches, and
  longer public caches so Live pages are not a long serial chain of requests
- **TP Watchlist:** Toolbar **TP** opens an ImGui pad (chat codes / item IDs, prices, BLTC in
  a new addon tab). CEF `about:live-tp` is tip-only
- **Notes:** Toolbar **Notes** ImGui clipboard helpers (waypoints, chat codes, builds, LFG)
- **Pad docking:** Notes/TP open beside the helper; if one is already open the other stacks
  below it. Both start closed each session and stay freely movable
- **Snow Crows:** Header stays above NitroPay (Profile/Inbox clickable); gw2armory trait/
  skill hover cards elevated and armory `overflow-clip` relaxed so tips are visible
- **Stamps:** Helper `2045` · homepage `2014`

## What’s new in 2.0.2.5

- **Nexus:** Library / addon search works again after loading this addon. The helper
  was clearing shared ImGui `WantTextInput` / `KeysDown` while closed (and when the
  cursor left the overlay), so typed letters fell through to GW2 hotkeys (e.g. `G`
  opened Guild). Capture is no longer wiped for other ImGui windows
- **ImGui IDs:** InputText, browse popups, options widgets, and related chrome use
  unique `###gw2igh_…` / `##gw2igh_…` suffixes so they cannot collide with Nexus
  widgets in the shared context; Options is wrapped in `PushID("GW2-InGame-Helper")`
- **Hover:** Browse / More / tab popups no longer use `ImGuiHoveredFlags_AnyWindow`
  (that treated Nexus UI as part of the helper and stole keyboard)
- **Companion:** Options → **Show favorites QR…** encodes favorite site IDs as a
  `gw2helper://sync/v1?favorites=…` deep link for the Android app’s QR import
- **Stamps:** Helper `2044` · homepage `2014`

## What’s new in 2.0.2.4

- **Catalog:** Snow Crows Browse links restored (47 entries) — raid profession builds,
  AccessiBuilds, Open World / PvP / WvW, guides hub, W1–W7 per-encounter guides,
  Discord — kept **alongside** MetaBattle (not a replacement)
- **Twitch:** Embeds become a Watch card / Open Ext. Official CEF binaries omit
  proprietary H.264 / AAC codecs, so Twitch reports Error #4000 in-page; same
  approach as YouTube Watch cards
- **Companion:** Android APK catalog refreshed to **2,709** sites (`GW2-Helper-1.0.1`)
- **Stamps:** Helper `2044` · homepage `2014`

## What’s new in 2.0.2.3

- **Search:** Default Web / help-box search uses **DuckDuckGo** instead of Google
  (Google stays available under Browse → Search). Avoids the `/sorry` “unusual
  traffic” captcha wall that Windows users often cannot solve in OSR
- **Profile:** Chromium cache moved from `%TEMP%` to
  `%LOCALAPPDATA%\GW2-InGame-Helper\cef-cache` so Storage Sense / Disk Cleanup
  cannot wipe cookies every session (a major Windows-vs-Proton difference)
- **Sign-in:** Google account, consent, and `/sorry` / reCAPTCHA URLs open in
  the system browser via Open Ext instead of being cancelled with no action
  (the dead “Sign in” button)
- **Ads:** Click-id detection (`gclid`, `gad_source`, …) and ad-referrer /
  cross-site popup routing continue to send revenue-bearing clicks external
- **Stamps:** Helper `2043` · homepage `2014`

## What’s new in 2.0.2.2

- **Ads:** Open-Ext URL buffer enlarged (2 KB → 8 KB) so long DoubleClick /
  NitroPay click trackers are not truncated mid-query (that produced a blank page)
- **Ads:** Every ad click route now leaves the addon — click-tracker navigations,
  links from known ad iframes, top-level jumps onto an ad network, and any
  new-window link aimed at a third-party domain open in the system browser, so
  the advertiser records the click
- **Browsing:** New-window links that stay on the current site (and links on the
  bundled homepage / cheat sheets) still navigate in-tab
- **Safety:** Refuse external open when a URL still exceeds the buffer (status tip)
  rather than ShellExecute of a half URL
- **Stamps:** Helper `2039` · homepage `2014`

## What’s new in 2.0.2.1

- **Input:** With the helper open, GW2 chat works when the cursor is on the game
  (including **Space**). Keys are only stolen while the pointer is over the helper
- **Close:** Stale CEF/ImGui key ownership is cleared on hide so chat/WASD work after close
- **Browse:** Site-picker / toolbar popups no longer dismiss when the cursor moves onto them
- **Find:** Toolbar field **Enter** runs find-in-page; **Web** runs site/Google search
- **Focus:** CEF editable caret shows again when clicking/typing in page fields
- **Safety:** WndProc ImGui IO access is guarded when no ImGui context exists
- **Stamps:** Helper `2038` · homepage `2013`

## What’s new in 2.0.2.0

- **Runtime:** Private CEF **150.0.14** / Chromium **150.0.7871.129** under
  `addons/GW2-InGame-Helper/cef/` (SHA-256 verify + extract). Never uses or writes
  game `bin64/cef`
- **Product:** Former Beta channel is now the shipping addon (`GW2-InGame-Helper.dll`)
- **Catalog:** No Snow Crows Browse links (test hub / login removed for release)
- **Sites:** Modern CSS/JS via CEF 150; Discord **Continue to Discord** handoff;
  in-page `<select>` polyfill; header chrome above NitroPay when needed
- **Input:** CEF 150 key events set `cef_key_event_t.size` (typing works); same Nexus
  routing so page typing does not drive skills / WASD
- **Reliability:** Crash-loop brake / process caps under Proton; Discord deep links
  via game process; LNA relaxed for Discord localhost RPC
- **Limits (honest):** MetaBattle Cloudflare / Google login often need **Open Ext**;
  OSR ≠ desktop Chrome; no Cloudflare bypass

## What’s new in 2.0.1.1

- **Input:** Typing in on-page wiki search no longer drives the character / skills
- **Input:** Game-owned key-ups stay paired (no stuck WASD); hovering after RMB
  camera look no longer flushes movement keys when the cursor lands on the overlay
- **Input:** Left-click into the helper can still release held game keys so autorun
  does not stick when you start using the UI
- **Stamps:** Helper / homepage `2011`

## What’s new in 2.0.1.0

- **Release:** Stable cut of the 2.0.0.x line for Nexus / GitHub distribution
- **Browse:** GW2.app deep links under Tools; MetaBattle / Guildjen / Accessibility Wars
  (Snow Crows removed at their request); site ads allowed
- **Reliability:** Windows first-paint stall fixed; YouTube embeds forced to Watch cards;
  Browse/Search/Find keys no longer leak to GW2
- **Docs:** Architecture and code-audit markdown; documentation index
- **Stamps:** Helper / homepage `2010` (fully restart GW2 after updating)

## What’s new in 2.0.0.21

- **Browse:** **GW2.app** links under Tools → GW2.app (hub, lists, database, maps, vault, TP, sign-in)
- **Helper:** CSS downlevel + wide viewport for `gw2.app`; login/account pages show an
  Open Ext tip (cookie banners left alone so ads / consent can work)
- **How to use:** Homepage cache stamp `221`

## What’s new in 2.0.0.20

- **Catalog:** Snow Crows removed at their request. Builds → Raids uses MetaBattle
  raid builds (hub + professions); Guides → Raid Boss uses MetaBattle wing strategy
  guides; Accessibility Wars remains for accessible builds; Snow Crows Discord invite gone
- **Ads:** Site ads are allowed — NitroPay / AdSense / analytics / consent hosts are no
  longer cancelled or stripped in the helper
- **Present:** Staging → DEFAULT GPU upload (avoids Mapping the ImGui-bound texture) plus
  paint-wait reason text if first paint stalls
- **How to use:** Homepage cache stamp `220`

## What’s new in 2.0.0.19

- **Present:** Fix native Windows stuck on “Waiting for first paint…” while status
  already says Ready. First GPU upload no longer uses `DO_NOT_WAIT` (Defender-busy
  devices were failing Map forever); falls back to a blocking `WRITE_DISCARD`.
  Helper also kicks `was_resized` after load if no paint has arrived yet
- **Security note:** Windows Defender may ML-flag the unsigned MinGW build as
  `Trojan:Win32/Wacatac.B!ml`. That is a known false positive on unsigned / statically
  linked tools — allow/restore the DLL, or submit it at
  [Microsoft file submission](https://www.microsoft.com/en-us/wdsi/filesubmission)
  (Software developer → incorrectly detected). Source is open on GitHub
- **How to use:** Homepage cache stamp `219`

## What’s new in 2.0.0.18

- **Embeds:** Guildjen HTML is rewritten before paint — YouTube `<iframe>` tags become
  **Watch on YouTube** cards (Complianz never activates the player). youtube.com /
  googlevideo subframe loads are cancelled. Helper exe re-extracts when the stamp
  mismatches so a warm/old helper cannot keep the refresh bug
- **How to use:** Homepage cache stamp `218`

## What’s new in 2.0.0.17

- **Embeds:** In-page YouTube players are replaced with a “Watch on YouTube” card.
  Clicking it opens your system browser and leaves the guide alone. Loading the
  player in CEF 103 OSR still caused mid-play refreshes even after navigation
  guards, so embeds are no longer activated in-helper
- **How to use:** Homepage cache stamp `217`

## What’s new in 2.0.0.16

- **Embeds:** Clicking Play on a YouTube iframe in a guide (e.g. Guildjen) no longer
  replaces the page with `youtube.com/watch` / googlevideo / accounts — that looked like
  a mid-playback refresh. Main-frame CDN and YouTube navigations are blocked while you
  are on a normal site; Guildjen embeds prefer youtube-nocookie + playsinline
- **Note:** In-overlay decode is still best-effort on CEF 103 OSR. If the player stalls,
  use **Open Ext**. The guide itself should stay put
- **How to use:** Homepage cache stamp `216`

## What’s new in 2.0.0.15

- **Input:** Typing in Browse filter, toolbar Search, or Find no longer leaks keys to
  Guild Wars 2. Previously only the CEF page blocked the keyboard, so typing `R`
  toggled autorun and could leave the character stuck running
- **Input:** Key-up is paired with the sink that ate key-down (CEF vs ImGui) so focus
  flips mid-press do not leave a lonely up/down for the game
- **How to use:** Homepage cache stamp `215`

## What’s new in 2.0.0.14

- **Removed YouTube:** GW2's bundled CEF 103 off-screen renderer cannot play YouTube
  reliably. The playback experiment still caused constant page refreshes, so the site and
  Video-On-Demand section have been removed instead of shipping a broken feature
- **Stability:** Restored the proven software-only CEF flags (`--disable-gpu`,
  `--disable-gpu-compositing`, `--disable-d3d11`) and the existing Google/Gemini UA.
  This removes the global SwiftShader experiment and minimizes Wine / Proton risk
- **Catalog:** 2,674 entries; Search returns to Google, DuckDuckGo, and Gemini
- **How to use:** Homepage cache stamp `214`

## What’s new in 2.0.0.13

- **Superseded by 2.0.0.14:** Clicking Play no longer promoted googlevideo / accounts / embed popups into
  the main frame (that looked like a refresh or crash). Popups stay cancelled on YouTube;
  CDN top-level navigations are blocked; BootJs no longer auto-clicks Play in a loop
- **How to use:** Homepage cache stamp `213`

## What’s new in 2.0.0.12

- **Browse:** Builds → Raids showed (10) but listed nothing — the Guides-only Raid Wings /
  Raid Boss nesting was incorrectly applied to Builds. Snow Crows raid builds draw as a
  flat list again
- **YouTube:** First pass at in-helper playback — drop hard `--disable-gpu` (it broke HTML5
  video in OSR), use ANGLE SwiftShader + software video decode, match UA to CEF 103, and
  nudge the HTML5 player / consent. If videos still fail after this, YouTube will be removed
  (use **Open Ext**)
- **How to use:** Homepage cache stamp `212`

## What’s new in 2.0.0.11

- **Browse:** Expanding some sections (Food / Minis / Armory / etc.) could show a blank body.
  `ImGuiListClipper` was auto-measuring row height next to the favorite-star `SameLine` layout;
  a zero height under nested headers made the clipper seek by zero — no rows, no scrollbar.
  Clipper now uses an explicit row height; small lists draw unclipped; section caches invalidate
  together with the category index; unmatched sub-buckets fall back to the hub row
- **How to use:** Homepage cache stamp `211`

## What’s new in 2.0.0.10

- **Fix:** Game could freeze while closing out. Unload never joined the helper-launch
  worker thread, so Nexus unmapped the IPC and unloaded the DLL while that thread was
  still inside `CreateProcess`/extract — the loader then stalled on exit. Shutdown now
  blocks new launches and joins the worker (3 s cap) before freeing anything
- **Fix:** Helper process had no host watchdog — it only exited on a clean IPC `QUIT`.
  If GW2 died hard the helper was orphaned and kept the named shared sections alive,
  stalling the next launch. It now waits on the GW2 process handle and exits with it
- **How to use:** Homepage cache stamp `210`

## What’s new in 2.0.0.9

- **Search:** YouTube under Browse → Search → Video-On-Demand (toolbar search via `results?search_query=`); BootJs skips ad-strip on YouTube; popups allowed when already browsing YouTube
- **Note:** In-overlay playback is best-effort (software CEF). Use **Open Ext** if video fails
- **How to use:** Homepage cache stamp `209`

## What’s new in 2.0.0.8

- **Input:** Fix window drag broken by 2.0.0.7 — addon WndProc runs *before* Nexus ImGui input; eating `WM_MOUSEMOVE` starved drag. Now feed ImGui on button-down/wheel only, always pass move/up
- **Input:** `CaptureMouseFromApp` while over the overlay so Nexus `WantCaptureMouse` gating stays sticky
- **How to use:** Homepage cache stamp `208`

## What’s new in 2.0.0.7

- **Input:** Fix click-through — WndProc now eats mouse down/up/move/wheel over the overlay (`return 0`); `WantCaptureMouse` alone never blocked GW2 skills/camera
- **Input:** Collapsed title bar also blocks clicks; press-latch keeps capture until button-up if a drag leaves the window
- **How to use:** Homepage cache stamp `207`

## What’s new in 2.0.0.6

- **Scroll:** Smoother page scrolling — disable multi-frame GPU staging while interacting; ~120 Hz present during wheel; accumulate fractional trackpad deltas; snappier helper input drain
- **How to use:** Homepage cache stamp `206`

## What’s new in 2.0.0.5

- **Present:** Fix black panel stuck on “Waiting for first paint…” — first GPU upload uses `WRITE_DISCARD` to initialize the dynamic texture; chunked staging `WRITE` only runs after the texture already has content
- **How to use:** Homepage cache stamp `205`

## What’s new in 2.0.0.4

Full audit follow-up (#1–#19):

- **Quit/reopen:** Never `TerminateProcess` from `SetVisible` — relaunch waits for graceful quit on `Tick`
- **Launch:** `CreateProcess` / helper extract run on a worker thread (RT only queues/polls)
- **Present:** Large frames can snapshot to CPU staging then chunked GPU upload (after first paint); pin released before Map
- **Browse:** Cached favorites / Raids / Achievements; URL warm mostly when overlay closed
- **BootJs:** Single-flight armory fetch queue + 429 backoff across skills/traits/items
- **Status:** Local status also writes IPC `status[]` (one logical source)
- **Build:** CssProxy WinHTTP path removed; ad-block + CSS downlevel filter retained
- **Docs:** `docs/COMPLIANCE.md`; hot-reload / TOS / dual-load notes; site id prefix validation
- **How to use:** Homepage cache stamp `204`

## What’s new in 2.0.0.3

Engine / Browse audit follow-up:

- **Launch:** `CreateProcess` / helper extract deferred to `WikiBrowser::Tick` — no longer blocks `RT_Render` on first open
- **Present:** Large full-frame GPU uploads split across frames (~180 rows / tick) when `Map(WRITE)` is available
- **Input:** Full IPC input ring drops oldest events instead of newest (paste / fast typing)
- **Browse:** Filter matches and Food / Minis section buckets are cached (no per-frame rebuild)
- **URL warm:** 32 sites/frame while the overlay is open; 64 when closed
- **BootJs:** Snow Crows armory API fetches are serialized with HTTP 429 backoff
- **Wiki:** Legendary Armory nesting — armor, weapons (by gen), accessories, amulet, rings, back items, legendary upgrades
- **How to use:** Homepage cache stamp `203`

## What’s new in 2.0.0.2

- **Warm hide:** Closing with Keep browser warm no longer posts `SET_VISIBLE` / wakes the helper every render frame
- **Settings:** Default-site Options picker no longer force-writes `settings.ini` on the UI thread
- **Load:** Slightly gentler URL-index warm (64 sites/frame); single debounced save path per render
- **Unload:** Helper quit wait shortened to 50 ms

## What’s new in 2.0.0.1

- **Browse:** Smaller display-scaled picker (~540×370 on 1080p) so it no longer eats half the screen
- **Browse:** Anchored dropdown under Browse / + / default-site (no move, no resize) — stays with the helper window

## What’s new in 2.0.0.0

Major stability release (engine / IPC audit):

- **IPC v5:** Shared memory and wake events are scoped by the GW2 process ID — multiple game clients no longer collide
- **Quit:** Closing the helper posts `QUIT` and finishes across render frames (~120 ms grace) instead of instantly `TerminateProcess` on the game thread
- **Settings:** Overlay close no longer force-writes `settings.ini` on the render thread (debounced flush; force only on unload)
- **URL index:** `BestMatchForUrl` never finishes the ~2600-site warm-up synchronously — chunked only (`TickWarmUrlKeys`)
- **Present:** Dirty-rect metadata from CEF; partial GPU upload when `Map(WRITE)` is available (full `WRITE_DISCARD` fallback)
- **UI:** First-open window size applied once (~30% display); Browse popup layout cached; warmer URL ticks at 96 sites/frame
- **Input:** Status tip when the key/click ring is full
- **How to use:** Homepage cache stamp `200`

## What’s new in 1.7.8.53

- **Browse:** Display-scaled popup (credit always visible; roomier on 4K, capped on 1080p)
- **Window:** First-open size ~30% of the display (saved size still wins after that)

## What’s new in 1.7.8.52

- **Browse:** Credit footer layout — Created by Xydroc; IGN and Discord on one line

## What’s new in 1.7.8.51

- **Browse:** Credit footer under the site lists (Created by Xydroc · IGN · Discord)

## What’s new in 1.7.8.50

- **Typing:** Fix dropped letters when typing fast — synthesize characters with `ToUnicode` on keydown (Nexus often skips `TranslateMessage` when keys are swallowed), pass full `lParam` scan codes to CEF, grow input ring to 256 (IPC v4), snappier helper input drain

## What’s new in 1.7.8.49

Final **audit** cleanup:

- **Load:** URL-match indexes build in chunks across `UI_Render` frames (no AddonLoad stall); first navigate still finishes sync if needed
- **Navigate:** Exact `about:` / `file:` homeUrl map; per-host candidates sorted longest-path-first (first hit wins)
- **Present:** `Map(..., DO_NOT_WAIT)` — skip a frame instead of stalling the GPU when the dynamic texture is busy
- **How to use:** Homepage cache stamp bumped to `49` (aligned with addon version)

## What’s new in 1.7.8.48

Follow-up **audit** pass (on top of 1.7.8.47):

- **Load:** `Sites::WarmUrlKeys()` runs at addon init — URL-match indexes are ready before the first navigate (no first-click hitch on the render thread)
- **Navigate:** Host→site index for `BestMatchForUrl` — only same-host catalog entries are scanned (was a full ~2600-site walk every URL/title sync)
- **Present:** Adaptive OSR upload — ~60 FPS while the page is receiving input; drops to ~30 FPS after 500 ms idle
- **Status:** `StatusCStr` refreshes via `strcmp` against the cache (removed per-frame FNV hash walk)
- **How to use:** Homepage refresh (stamp `48`) — tab hotkeys, Keep browser warm / collapse tip, Browse clipping note

## What’s new in 1.7.8.47

Render / UX / IPC **audit** fixes (less host hitching, keep CEF alive):

- **Collapse:** Title-bar collapse no longer calls `SetVisible(false)` / `TerminateProcess` — expanding the window no longer relaunches the helper
- **Navigate:** Precomputed site URL keys (`path` / `host` / `path/`) so `BestMatchForUrl` does not allocate thousands of strings per click
- **Present:** OSR D3D texture allocated once at max size (1920×1200) with UV crop via `FrameUvMax` — window drag no longer `CreateTexture2D` every pixel
- **Present:** `SET_BOUNDS` helper wakes throttled (~100 ms) while still publishing `view_w` / `view_h` immediately for CEF `GetViewRect`
- **Status:** Cached `StatusCStr` for the loading chip and “Waiting for first paint…” path — no per-frame `std::string` / mutex
- **Input:** In-window Ctrl+T / Ctrl+W / Ctrl+F / Ctrl+Tab read Nexus-filled `ImGuiIO::KeysDown` (not `GetAsyncKeyState`)
- **Input:** Closed-window hotkey fallback poll capped ~30 Hz (WndProc + Nexus bind remain primary)

## What’s new in 1.7.8.46

- **Browse:** Smoother scrolling on large Wiki lists (Food / Utility / Minis) — category/section caching + ImGui list clipping

## What’s new in 1.7.8.45

- **Wiki → Utility:** Utility items nested by primary effect attribute (same attribute sections as Food)
- **Wiki → Minis:** All miniature wiki pages nested by hub subsections (Sets, Core, expansions, festivals, Gem Store, etc.)

## What’s new in 1.7.8.44

- **Wiki → Food:** All non-ascended food pages nested by primary attribute (Power through All Attributes + Other), matching the wiki TOC
- **Wiki → Ascended Food:** Ascended feasts only (hub + Gourmet Training + feast pages), nested by the same attribute sections

## What’s new in 1.7.8.43

- **Legendary Weapons:** Generation 3 Variants (6 dragon set hubs, Facet collections, all 96 skins)
- **Wiki → Utility:** Utility item hubs (list, enhancement, slayer potions, oils/stones/crystals)
- **Wiki → Upgrades:** Superior Runes, Relics, and Superior Sigils (all wiki pages)

## What’s new in 1.7.8.42

- **Wiki:** Lifestyle (Fishing, Jade Bot, Skiff, Home Instance, Homestead); Crafting (disciplines + related); Food; Ascended Feasts (all feast pages)
- **Guides → Crafting:** Full [GW2 Crafts](https://gw2crafts.net/) catalog (Normal / Fast / 400-500 / Special)

## What’s new in 1.7.8.41

- **Wiki:** Cosmetic Infusions moved here from Guides; new **Legendary Weapons** section (all Gen 1–3 wiki pages, nested by generation)

## What’s new in 1.7.8.40

- **Guides → Cosmetic Infusions:** Each infusion opens its GW2 Wiki page (Guildjen how-to kept as overview)

## What’s new in 1.7.8.39

- **Guides → Cosmetic Infusions:** Wiki hub + all 46 cosmetic infusions (nested by Wizard's Vault / Mystic Forge / Open World / Instanced / Festival / WvW) via Guildjen’s how-to guide
- **Achievements:** Side Stories hub + Wizard's Portal Tome; HoT Verdant Brink + Auric Basin map guides

## What’s new in 1.7.8.38

- **Browse → Guides:** New **Raids** section with **Raid Wings** and **Raid Boss** as nested subsections

## What’s new in 1.7.8.37

- **How to use:** Always rewrite helper-home.html and open it with a `?v=` cache-bust so Browse/Favorites pills cannot stick from CEF/disk cache

## What’s new in 1.7.8.36

- **How to use:** Bump helper-home stamp so the Browse/Favorites pills removal actually rewrites on disk (was stuck on stale HTML)

## What’s new in 1.7.8.35

- **Guides → Jumping Puzzles:** Guildjen category hub + all 44 JP guides ([pages 1–3](https://guildjen.com/category/gw2/gw2-guides/gw2-jps/))

## What’s new in 1.7.8.34

- **Guides → Mounts:** Guildjen [Siege Turtle unlock](https://guildjen.com/siege-turtle-mount-unlock-guide/)

## What’s new in 1.7.8.33

- **Guides:** Fill major gaps — Guildjen fractals (full set), Harvest Temple, Mount Balrior W8 bosses, beginner PvP/WvW/raids/fractals, new-player roadmap, gold / Gem Store / Wizard’s Vault, rifts & convergences
- **Guides → Achievements:** New section with nested Living World / HoT / PoF / EoD / SotO / Janthir Wilds / Visions of Eternity / Festivals hubs and guides ([Guildjen](https://guildjen.com/gw2-achievements/))

## What’s new in 1.7.8.32

- **Home:** Drop non-functional Browse / Favorites pills from the hero header (keep Ctrl+Shift+H and One DLL)

## What’s new in 1.7.8.31

- **Helper:** Fix stuck “Starting…” / “Loading browser…” — IPC `ready` when the helper can accept `CREATE_TAB` (the 1.7.8.30 audit deferred ready until the first browser, which deadlocked tab sync)

## What’s new in 1.7.8.30

Render / IPC / BootJs **audit** fixes (less host hitching, safer tab IPC):

- **Present:** Cap overlay frame upload at ~60 FPS (16 ms); CEF OSR `windowless_frame_rate` 60
- **Input:** Throttle mouse-move wakes to the helper (~30 Hz)
- **IPC:** Retry queue for CREATE / ACTIVATE / CLOSE when the command ring is full; `MemoryBarrier` on frame publish
- **Helper:** Softer close (no 50 ms wait on the render thread); visible idle 8 ms (was 1 ms busy-wait)
- **BootJs:** Inject once per load (`OnLoadEnd` only); MutationObservers debounced 100 ms; armory API response cache

## What’s new in 1.7.8.29

- **Helper:** Stop ad-strip from deleting Snow Crows guide bodies (`id="nitro-article-*"` matched the old `[id*="nitro"]` rule — content flashed then vanished)

## What’s new in 1.7.8.28

- **Helper:** Snow Crows raid guides — fill empty GW2 armory skill chips via the official API, reveal Alpine-cloaked TLDR, convert `<image>` diagrams to `<img>`

## What’s new in 1.7.8.27

- **Helper:** Hydrate Guildjen Breeze lazy images (`data-breeze`) so raid wing guides show diagrams/screenshots in CEF

## What’s new in 1.7.8.26

- **Helper:** Don’t replace the current page when YouTube/media embeds open popups (fixes Guildjen guide “refresh” on play); auto-activate Complianz YouTube embeds on guildjen.com

## What’s new in 1.7.8.25

- **Guides → Raid Wings:** Guildjen hub, beginner raid guide, and W1–W8 wing guides (replaces MetaBattle wing pages)

## What’s new in 1.7.8.24

- **Guides → Raid Boss:** Snow Crows per-encounter guides (W1–W7) instead of Hardstuck — Hardstuck H.264 clips don’t play in CEF; Video.js CDN unblocked for Snow Crows
- Prep links under Raid Boss now use Snow Crows squad guides; W8 omitted until Snow Crows publishes Mount Balrior

## What’s new in 1.7.8.23

- **Browse:** Sections start collapsed; expanded sections are saved in settings and restored next time

## What’s new in 1.7.8.22

- **Browse:** Raid Boss wings (W1–W8) are collapsible subsections under a single **Raid Boss** section

## What’s new in 1.7.8.21

- **Guides → Raid Boss:** Hardstuck 10-Player Content, Squad Composition, Envoy Armor
- **Tools → Logs / KP:** Hardstuck ArcDPS setup guide

## What’s new in 1.7.8.20

- **Guides:** Hardstuck [Raid Boss](https://hardstuck.gg/gw2/guides/raids/) section — every encounter from W1–W8, grouped by wing (collapsible)

## What’s new in 1.7.8.19

- **Browse:** Sections within a category are collapsible (click header; start expanded; shows site count)

## What’s new in 1.7.8.18

- **Fix:** Google Search + DuckDuckGo on the same CSS downlevel path as Gemini (response filter + BootJs)
- **Compat:** Rewrite `color(display-p3 …)` → `rgba(...)`; strip `@property` / map `dvh` on DDG sheets
- **UX:** Don’t strip “ad” DOM on DuckDuckGo (same SPA-chrome breakage as Google)

## What’s new in 1.7.8.17

- **Fix:** Gemini / Google Material theme — rewrite `color-mix(...)` in `<style>` **before first paint** via a CEF response filter (JS-after-load was too late)
- **Fix:** Prefer Chrome User-Agent (Firefox spoof for login confused Google frontends)
- **Compat:** Map `dvh`/`dvw` → `vh`/`vw`; flatten invalid nesting `&` selectors; Gemini readability fallback CSS

## What’s new in 1.7.8.16

- **Workaround:** Spoof a desktop Firefox User-Agent so Google Account / Gemini Pro sign-in is less likely to hit “This browser may not be secure” (Google still blocks many embedded browsers; if login fails, use **Open Ext**)
- **UX:** Tip banner on Google sign-in pages; Home tips note that Open Ext sessions are separate from in-game tabs

## What’s new in 1.7.8.15

- **Fix:** Google Search / Gemini hard-to-read styling on CEF 103
  - Downlevel `oklch` / `color-mix` / `@property` CSS on `*.google.com`
  - Skip aggressive ad DOM stripping on Google hosts (was breaking SPA chrome)
  - Do not force `viewport=1280` on Google / Gemini
  - Allow `googletagmanager.com` (needed for Google SPA UI init)

## What’s new in 1.7.8.14

- **Search:** Google Gemini (`https://gemini.google.com/app`) under a new AI Browse section

## What’s new in 1.7.8.13

- **Guides:** TLDR Fractals grouped under Guides → TLDR with TLDR Raids / Dungeons

## What’s new in 1.7.8.12

- **Guides:** GW2 TLDR Dungeons (`TLDR Dungeons`) under Guides → TLDR
- Fractals / Meta Timers were already present (`TLDR Fractals`, Tools → Meta Timers)

## What’s new in 1.7.8.11

- **Guides:** MetaBattle [PvP Guides](https://metabattle.com/wiki/PvP_Guides) and [WvW Guides](https://metabattle.com/wiki/WvW_Guides) hubs
- **Browse:** new Guides sections for PvP and WvW

## What’s new in 1.7.8.10

- **Builds:** Accessibility Wars lives under AccessiBuilds with Snowcrows AccessiBuilds

## What’s new in 1.7.8.9

- **Builds:** Snowcrows per-profession raid builds (SC Raid Elementalist … Warrior), hub at `/builds/raids`
- **Guides:** MetaBattle Raid Wing 4 — Bastion of the Penitent

## What’s new in 1.7.8.8

- **Sites:** Snowcrows AccessiBuilds / Open World / PvP / WvW builds + Guides hub
- **Guides:** MetaBattle PvE hub with Fractals, Raid Wings, and Strikes Browse sections
- **Dev:** `tools/validate_sites.py` (+ `make validate-sites`) checks unique ids and Browse mappings

## What’s new in 1.7.8.7

- **Fix:** URL/title IPC reads are seq+len fenced (no torn / unterminated string crash risk)
- **Fix:** Frame path is double-buffered with a reader lock (no mid-copy tear under CEF paint)
- **Fix:** Full cmd ring no longer falls back to legacy slot for tab CREATE/CLOSE/ACTIVATE (avoids reorder)
- **Perf:** Cached URL/title on the render hot path; helper wakes on IPC instead of busy Sleep(1) when idle

## What’s new in 1.7.8.6

- **Fix:** Tab URL/title no longer corrupted when activating a tab before its CEF browser exists
- **Fix:** Helper restart no longer replays stale CLOSE/CREATE commands
- **Fix:** Reopening the helper no longer reloads every live tab
- **Fix:** Shutdown only kills this addon’s helper (multi-client safe)
- **Dev:** `make install` keeps `settings.ini`; use `make install-reset` to wipe it

## What’s new in 1.7.8.5

- **Updates:** Nexus uses `UP_GitHub` + repo URL again (correct auto-update); direct DLL link kept for manual download

## What’s new in 1.7.8.4

- **Update link:** briefly pointed Nexus at the direct release DLL (`UP_Direct`) — superseded by 1.7.8.5

## What’s new in 1.7.8.3

- **Fix:** Tab close is one control per tab (`Title  x`) — clicking the last tab’s x no longer hits the previous tab

## What’s new in 1.7.8.2

- **Fix:** Closing the last tab no longer closes / corrupts the tab before it (IPC commands were running twice after slot compact; URL/title sync waited for helper active_tab)

## What’s new in 1.7.8.1

- **Builds:** MetaBattle PvP and WvW live under Builds (PvP / WvW sections) — no separate PvP/WvW categories
- **Uber's All-In-One:** axe / sickle / pick each labeled on its own card (Rayhan Bayt · Beetletun · Rata Pten)
- **Wiki:** Easy Objectives under Wizards Vault (percent-encoded wiki URL)

## What’s new in 1.7.8.0

- **Fix:** Closing tab 2/3/4+ no longer destroys the first tab’s CEF browser (serialize creates; always compact slots)
- **Browse section headers** for every category (Help, Search, Wiki, Builds, Farming, …)
- **Wiki:** Vault Easy Objectives (Wizard’s Vault)
- **Uber's All-In-One:** axe / sickle / pick labeled with Rayhan Bayt, Beetletun, Rata Pten
- **Cheat sheet checklists** use real checkboxes (tick reliably in OSR)
- Nexus description: Wiki, Snowcrows, MetaBattle, and more

## What’s new in 1.7.7.0

- **Fix:** Restoring / opening multiple tabs no longer swaps CEF browsers (closing the wrong tab’s page)
- **Cheat sheet checklists** are clickable — tick items off; progress is remembered per sheet

## What’s new in 1.7.6.0

- **Browse section headers** for Tools, Guides, Discord, Builds, Wiki, and Official (same visual grouping as Cheat Sheets)
- **Fix:** Back after changing site no longer lands on a white `about:blank` page

## What’s new in 1.7.5.0

- **Browse + chrome UI refresh** (same gold/bronze theme):
  - Larger search-first Browse picker with autofocus filter
  - Cheat Sheets grouped into Prep / Gear / Squad / Fractals / Encounters / Account / WvW section headers
  - Tab pin mark, tighter tabs, compact nav cluster; Home tooltip matches default landing site
- **Fix:** Browse/`?` glyphs — sanitize titles for ProggyClean (no em dash / ellipsis / middle-dot)

## What’s new in 1.7.4.0

- **Six new Cheat Sheets:**
  - **Daily / Weekly Checklist** — raids, strikes, T4/CMs, Wizard’s Vault, metas
  - **Currency Sinks** — laurels, unbound/volatile, spirit shards, mystic coins, karma
  - **Ascended Start** — armor/weapons/trinkets path + fractal AR gearing
  - **Portals / Pulls / Utility** — portals, pulls, reflects (squad QoL)
  - **Homestead Extras** — stations, nodes, QoL (beyond Home Garden)
  - **WvW Consumables** — siege, food/utility, supply glance
- Home button uses Options default landing site; fractal AR tables corrected (T2–T4 / CM)

## What’s new in 1.7.3.0

- **Removed** tabbed cheat sheet hubs (Raid Prep, Squad Utility, Encounters, Fractals)
- Cheat sheets remain as separate Browse entries only

## What’s new in 1.7.2.2

- **Uber's All-In-One** cheat sheet — themed waypoint cards with copy-to-clipboard chat codes (hubs, Wizard’s Vault, Chak Egg, Obsidian Shards, Provisioner Tokens)
- Credit: waypoint list curated by **uberduber.1249**
- **Additional Cheat Sheets:**
  - **Strike Missions Overview** — IBS / EoD / SotO / Old Lion’s Court
  - **Fractal CM / T4 List** — scales 95–100, AR glance
  - **Squad Template** — typical 10-man roles
  - **Stability / Cleanse** — group stab & condi cleanse
  - **Material Conversions** — mystic forge staples & sinks
  - **Legendary Short Paths** — gen / armor / backpack checklists
  - **Mount Unlock Checklist** — griffon, skyscale, siege turtle

## What’s new in 1.7.2.1

- **Fix:** Game freeze from writing `settings.ini` every frame (tab title sync + window pos). Saves are debounced; titles no longer mark dirty every tick

## What’s new in 1.7.2.0

- **Tab hotkeys** — Ctrl+T new-tab picker · Ctrl+W close · Ctrl+Tab / Ctrl+Shift+Tab cycle
- **Fix:** Tab names update when the page changes (CEF title + URL→site match); titles are saved

## What’s new in 1.7.1.1

- **Fix:** Opening a site in a new tab no longer navigates the previous tab to the same page

## What’s new in 1.7.1.0

- **Cheat Sheets** category — built-in offline pages (Raid Food style), including Raid Food, utilities, fractals, gear, boons, CC, wings, garden
