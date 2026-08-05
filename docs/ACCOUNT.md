# Account — official API pads

**Revision:** 2.2.0.11 · **Audience:** contributors and players configuring keys  
**Companions:** [`API_KEY.md`](API_KEY.md), [`COMPLIANCE.md`](COMPLIANCE.md), [`WHITEPAPER.md`](WHITEPAPER.md) §17.1

---

## 1. Purpose

The Account side-rail hub surfaces **read-only** Guild Wars 2 account data via `api.guildwars2.com`. It never performs account actions (buys, mail, builds equip, etc.).

---

## 2. Pads / areas (shipping)

| Area | Typical endpoints / notes |
|------|---------------------------|
| Overview | Identity / summary chips |
| Unlocks | Legendary armory / unlock lists |
| Inventory | Character bags / shared |
| Stash / materials / bank | inventories scopes |
| Wallet | wallet currencies |
| Vault | Wizard’s Vault / dailies (progression) |
| Trading / TP watch | tradingpost + public prices |
| Item lookup | public `/v2/items` (+ wiki search) — key optional |
| Crafting | recipes / plan resolve |
| Progress | mastery / related progression |
| Session history | local continuity helpers |
| Character profiles | routing/progress continuity |

Exact UI labels follow the side-rail Account chrome in-game.

---

## 3. API key

Create at [account.arena.net/applications](https://account.arena.net/applications). Paste under **Nexus Options → GW2-InGame-Helper**. Stored only in local `settings.ini`.

Recommended scopes: `account`, `wallet`, `inventories`, `characters`, `progression`, `unlocks`, `tradingpost`. Full table: [`API_KEY.md`](API_KEY.md). Also Browse → Help → API Key Setup (`about:api-key-setup`).

**GW2 API Check:** Side-rail button under Settings opens `about:gw2-api-check` — a local LivePanels page that probes official `api.guildwars2.com` endpoints in parallel (public routes always; account routes use the saved key). Use it to confirm ArenaNet reachability and key scopes from inside the game. Not a third-party status mirror.

---

## 4. Engineering constraints

- HTTP via `Gw2Http` on **worker threads only** (never block `RT_Render`).
- Respect **429** backoff; cache item names (`stash-names.cache`) where applicable.
- Prefer **`JsonView.h`** (`src/api/`) for GW2 API scrapes — `string_view` / `Bytes` bounds checks, no JSON libs in the CEF helper. Remaining Wallet/TP/Logs copies should migrate onto it iteratively.
- UI: wrapping chips / in-window radios (Nexus-safe; avoid fragile combo popups).

---

## 5. Module map (indicative)

| Concern | Typical TUs |
|---------|-------------|
| Hub chrome | `AccountPad*` |
| Crafting | `CraftingApi*`, `CraftingPlan*` |
| TP watch | `TpWatch*` |
| Wallet / vault / lookup | `Wallet*`, `Vault*`, `Lookup*` |
| Progress / unlocks | `Progress*`, `Unlocks*` |
| Shared decls | `*Shared.h` / `*Internal.h` — one defining TU |

See [`MODULES.md`](MODULES.md) and [`ARCHITECTURE.md`](ARCHITECTURE.md) §7.

---

## 6. Compliance

Allowed: official API reads with user key; local caches.  
Forbidden: automating account actions; exfiltrating keys; putting keys in QR / log uploads.
