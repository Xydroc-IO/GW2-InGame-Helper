# DPS Logs — setup and install

The same guide is available in-game under **Browse → Help → DPS Log Setup Help**
(`about:dps-log-setup`).

The **DPS Logs** pad browses your ArcDPS `.evtc` / `.zevtc` combat logs, can
upload to [dps.report](https://dps.report/), and shows encounter metadata, squad
DPS, and key boon uptimes.

It uses two optional pieces that live **outside** the game client:

1. **Elite Insights CLI** — auto-downloaded by the addon (like CEF) into
   `addons/GW2-InGame-Helper/ei/`
2. **.NET 8 Desktop Runtime (x64)** — must be installed in the **Windows
   environment that runs Guild Wars 2** (native Windows, or your Proton/Wine
   prefix on Linux)

Elite Insights is MIT software by [baaron4 / GW2-Elite-Insights-Parser](https://github.com/baaron4/GW2-Elite-Insights-Parser).
We do not ship their binaries inside the DLL; the pad downloads the official
`GW2EICLI.zip` and verifies SHA-256.

---

## Quick checklist

| Step | Windows | Linux (Proton / Steam) |
|------|---------|------------------------|
| ArcDPS logging on | Required | Required |
| Open **DPS Logs** in the helper | Required | Required |
| Elite Insights CLI | Auto **Install / Update EI** | Same (into the prefix) |
| .NET 8 Desktop Runtime x64 | Install once on Windows | Install **into the GW2 Wine prefix** (not your distro packages) |
| Optional dps.report token | Paste in the pad | Same |

**Important for Linux:** Distro packages such as `dotnet-runtime-8.0` from apt/pacman
**do not** help. Elite Insights is a Windows `.exe` and only sees .NET installed
inside the Guild Wars 2 Proton/Wine prefix.

---

## 1. Prerequisites

- Guild Wars 2 with **Raidcore Nexus** and **GW2-InGame-Helper** installed
- **ArcDPS** recording combat logs (default folder below)
- Network access the first time EI is downloaded (and for dps.report uploads)

Default log folder (Windows path; under Proton this is inside the prefix):

```text
Documents\Guild Wars 2\addons\arcdps\arcdps.cbtlogs
```

On Proton that usually maps to something like:

```text
…/steamapps/compatdata/<appid>/pfx/drive_c/users/steamuser/Documents/Guild Wars 2/addons/arcdps/arcdps.cbtlogs
```

---

## 2. Open the pad

1. Open the In-Game Helper window.
2. Click **DPS Logs** on the pad row (or **⋯ → Show DPS Logs**, or Nexus Options → **Show DPS Logs**).
3. Confirm the **log folder** path looks correct (edit if you moved ArcDPS logs).

---

## 3. Elite Insights (automatic)

1. In **DPS Logs**, click **Install / Update EI** (also runs when the pad opens if EI is missing).
2. The addon:
   - Prefers a local `GW2EICLI.zip` next to the DLL / in the addon folder, or
   - Downloads the latest `GW2EICLI.zip` from GitHub releases
   - SHA-256 verifies and extracts to `addons/GW2-InGame-Helper/ei/`
   - Fills the Elite Insights CLI path
3. Status text should show Elite Insights ready (version stamp in `ei/ei.ver`).

You can still paste a custom path to `GuildWars2EliteInsights-CLI.exe` if you maintain your own install.

---

## 4. .NET 8 Desktop Runtime

EI’s CLI needs the **Windows .NET 8 Desktop Runtime (x64)**.

Official installer (always use the current x64 Desktop Runtime):

- [Download .NET 8 Desktop Runtime (x64)](https://dotnet.microsoft.com/download/dotnet/8.0)
- Direct installer redirect: `https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe`

In the pad, **Install .NET 8 Runtime** opens that installer link. **Recheck .NET** rescans after you finish installing.

### Windows (native)

1. Download and run `windowsdesktop-runtime-*-win-x64.exe`.
2. Complete the wizard.
3. Back in **DPS Logs**, click **Recheck .NET** — the yellow warning should clear.
4. Use **Parse pending** or **Load report meta** as needed.

### Linux / Steam Deck (Proton) — Protontricks GUI

Wine/Proton gives the game a fake Windows `C:` drive. Your real Linux disk is
mapped as the **`Z:`** drive, so you can run a Downloads-folder installer
**into the GW2 prefix** without copying files by hand.

#### A. Download the installer on Linux

1. On your normal Linux desktop/browser, download:
   [**.NET 8 Desktop Runtime (x64)**](https://dotnet.microsoft.com/download/dotnet/8.0)
   (the Windows **Desktop** runtime x64 installer, e.g.
   `windowsdesktop-runtime-8.0.x-win-x64.exe`).
2. Save it to your usual Linux **Downloads** folder
   (`~/Downloads` / `/home/<you>/Downloads`).

#### B. Open the GW2 prefix with Protontricks

1. Open **Protontricks** from your Linux application menu.
2. Wait for the game list to load, select **Guild Wars 2**, click **OK**.
3. When asked “What do you want to do?”, choose **Select the default wineprefix** → **OK**.
4. In the next menu, choose **Run explorer** → **OK**.
5. A Windows-style **Explorer** window opens — this is the file browser *inside*
   the GW2 prefix.

#### C. Find the installer via the `Z:` drive

Wine maps your entire Linux filesystem to **`Z:`**.

1. In Explorer, open **My Computer** / This PC (or the left sidebar).
2. Open the drive labeled **`(Z:)`**.
3. Open **`home`**.
4. Open the folder with **your Linux username**.
5. Open **`Downloads`** (or wherever you saved the `.exe`).
6. Double-click `windowsdesktop-runtime-*-win-x64.exe`.

The normal Windows install wizard runs on your desktop. Click through **Install**.
When it finishes, .NET 8 lives on the prefix **`C:`** drive permanently for that
game.

#### D. Confirm in the addon

1. Restart Guild Wars 2 (or at least fully restart after the install).
2. Open **DPS Logs** → **Recheck .NET**.
3. The “.NET 8 not detected” banner should go away.

If Protontricks is not installed, install it from your distro (or Flatpak) first.
Some Steam Deck setups use Discover / flatpak `com.github.Matoking.protontricks`.

---

## 5. Using DPS Logs day-to-day

| Action | What it does |
|--------|----------------|
| **Rescan** | Finds logs under the log folder; may auto **Load report meta** for linked uploads |
| **Parse pending** | Runs Elite Insights CLI → JSON (needs .NET 8) |
| **Install / Update EI** | Refreshes EI from GitHub latest |
| **Upload** / **Upload filtered** | Uploads to dps.report; stores permalink + basic encounter info |
| **Load report meta** | Fills encounter, squad DPS, and boon uptimes from dps.report (`getJson`) for existing links |
| Optional **dps.report user token** | Associates uploads with your dps.report account (treat like a password) |

Detail tab shows squad **DPS / Power / Condi** and key boon uptimes
(**Quick / Alac / Might / Fury / Prot**). Open the full HTML report with
**Open report** when you need everything Elite Insights renders in a browser.

### KillProof tab

Right-pane tab **KillProof** (alongside Detail / Players / Guilds / Fastest / Setup)
loads public [killproof.me](https://killproof.me/) profiles for the **selected log’s
squad**:

| Column | Meaning |
|--------|---------|
| **LI** / **LD** / **UFE** | Account-wide Legendary Insights, Divinations, Unstable Fractal Essences |
| Encounter token (e.g. **Xera**) | Token count for the **selected** encounter |
| Rows | Players in that log only |

Click **Load KillProof** to refresh (also auto-starts when you open the tab).
Click an account name to open their killproof.me page. Private / unregistered
profiles show **—**.

### Layout

Filters | log list | Detail / Players / **KillProof** / … — drag the vertical
splitter between the list and the right pane to resize (saved).

**Group by encounter** is **on by default** (Filters pane): collapsible sections
per boss with count / kills / best kill / last time; newest encounters first.
Uncheck for a flat chronological-style list. Result / Mode / Time filters use
**in-window radios** (not popup combos — Nexus often eats ImGui dropdown clicks).
Window size and position are remembered; first-open size uses most of the game
client (~92%×84%, e.g. ~1760×900 on 1080p) so filters | list | Detail/KillProof
match the intended layout. Tables scroll horizontally if a pane is still tight.

---

## 6. Troubleshooting

### “.NET 8 Desktop Runtime not detected”

- **Windows:** Install the Desktop x64 runtime, then **Recheck .NET**.
- **Proton:** You installed .NET on Linux itself, or into the wrong prefix.
  Repeat the Protontricks + `Z:\home\…\Downloads` steps for **Guild Wars 2** only.
- Fully quit GW2 after installing .NET, then reopen.

### Elite Insights install / parse fails

- Click **Install / Update EI** again (needs network).
- Confirm `addons/GW2-InGame-Helper/ei/GuildWars2EliteInsights-CLI.exe` exists.
- Confirm .NET 8 is detected first — EI will not parse without it.
- Check the Detail pane for a red parse error line after **Parse**.

### Logs list shows filenames but `?` / no DPS

- Uploads alone used to store only the link. Use **Load report meta** (or Rescan)
  to pull DPS/boons from dps.report, or **Parse pending** with EI + .NET.
- Confirm the log folder path points at real `.zevtc` / `.evtc` files.

### KillProof shows only dashes / “none/private”

- Accounts need a **public** killproof.me profile (or the site returns not found).
- Load full EI JSON first (**Load DPS/boons** / Parse) so account names exist.
- Open the **KillProof** tab and click **Load KillProof**; check the status line
  (`N loaded · M none/private`).
- Network to `killproof.me` must work from the game environment (Wine/Proton included).

### Proton: Install .NET button opens a browser but “nothing installs”

- The browser downloads the `.exe` to Linux Downloads. You still must run it
  **through Protontricks → Run explorer → Z: → … → Downloads** so it installs
  into the GW2 prefix. Double-clicking the `.exe` from a native Linux file
  manager will not put .NET into the prefix.

### Wrong game prefix

- Always pick **Guild Wars 2** in Protontricks. Installing .NET into another
  game’s prefix will not help this addon.

---

## 7. Privacy and compliance notes

- Combat logs and dps.report links can contain account names and guild tags.
  Keep them local unless you choose to upload.
- Uploads use your optional dps.report user token; do not share it.
- See [`COMPLIANCE.md`](COMPLIANCE.md) for addon boundaries (read-only logs,
  no combat automation, EI as an optional external tool).

---

## Related

- In-game: **Browse → Help → DPS Log Setup Help**
- API key (Wallet / Vault / Account / TP delivery): [`API_KEY.md`](API_KEY.md)
- [Elite Insights releases](https://github.com/baaron4/GW2-Elite-Insights-Parser/releases)
- [dps.report](https://dps.report/) · [API docs](https://dps.report/api)
- [.NET 8 downloads](https://dotnet.microsoft.com/download/dotnet/8.0)
- CEF first-run pattern (similar download/verify model): [`CEF_RUNTIME.md`](CEF_RUNTIME.md)
