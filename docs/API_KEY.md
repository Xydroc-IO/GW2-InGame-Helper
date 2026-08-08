# GW2 API key setup

Several pads and Live features need a **read-only** Guild Wars 2 API key from
[account.arena.net/applications](https://account.arena.net/applications).

The same guide is available in-game under **Browse → Help → API Key Setup**
(`about:api-key-setup`).

---

## Create a key

1. Open [account.arena.net/applications](https://account.arena.net/applications) and sign in.
2. Create a **New Key** (name it e.g. `In-Game Helper`).
3. Enable the scopes listed below.
4. Copy the key string.

Treat it like a password. Revoke or recreate keys anytime on the same page.

---

## Required scopes

| Scope | Used for |
|-------|----------|
| **account** | Base identity — almost every personal API call |
| **wallet** | Wallet pad (currencies) |
| **inventories** | Bank, material storage, shared inventory, bags / stash |
| **characters** | Character roster and per-toon bags; Instances story quest sync |
| **progression** | Wizard’s Vault / Dailies; world-event claim marks; Instances raids/fractal level/achievements |
| **unlocks** | Legendary Armory (Account → Progress) |
| **tradingpost** | Trading Post delivery box, open orders / history |

**Recommended:** enable all of the above on one key. Item lookup and public TP
prices / listings / gem exchange work without a key; personal Vault, wallet, mats,
unlocks, delivery, open orders, Instances sync, and Completion AP overlay need
these scopes (`progression` + `characters` for story quests).

---

## Add it to the addon

1. Open the helper side rail → **Settings** (or Nexus Options → **Open Settings**).
2. Find **GW2 API key (Live panels)**.
3. Paste the key (masked field). It is saved only in this addon’s `settings.ini`.
4. Reload Live / Account tabs if they were already open.

Settings also has **Create key on account.arena.net** (opens the applications page)
and **Clear API key**.

---

## Privacy

- Stored only in local `settings.ini` — never shared, never put in QR codes,
  never uploaded with combat logs.
- Do not share screenshots that show the full key string.
