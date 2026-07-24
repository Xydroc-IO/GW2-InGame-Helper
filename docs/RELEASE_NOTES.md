# GW2 In-Game Helper v2.0.0.0

**Signature:** `0x48454C50` (`HELP`) · **License:** MIT · **Author:** xydroc

In-game browser for Guild Wars 2 — Wiki, Snowcrows, MetaBattle, and more.
One DLL for Nexus — no memory reads.

## Install

Copy **only** `GW2-InGame-Helper.dll` into `<Guild Wars 2>/addons/`.

Requires [Raidcore Nexus](https://raidcore.gg/gw2/nexus) + Guild Wars 2 (Windows / Wine / Proton).

**Updates:** GitHub Releases · [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper) ·
[latest DLL](https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/latest/download/GW2-InGame-Helper.dll)

---

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
