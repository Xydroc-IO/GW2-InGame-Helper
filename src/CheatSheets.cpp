#include "CheatSheets.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
		if (n > 0)
			WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	std::string PathToFileUrl(const std::wstring& path)
	{
		std::string utf8 = WideToUtf8(path);
		for (char& c : utf8)
		{
			if (c == '\\')
				c = '/';
		}
		if (utf8.size() >= 2 && utf8[1] == ':')
			return std::string("file:///") + utf8;
		return std::string("file://") + utf8;
	}

	static const char* kSharedCss = R"CSS(
  :root {
    --bg: #06070a;
    --panel: rgba(16, 18, 24, 0.92);
    --panel-2: #12141a;
    --border: #5a4a28;
    --border-soft: rgba(235, 192, 71, 0.22);
    --gold: #f0c65a;
    --gold-bright: #ffe08a;
    --gold-dim: #c9a227;
    --text: #f0f2f5;
    --muted: #a8aeb8;
    --accent: #1a1510;
    --power: #c45c4a;
    --condi: #9b7bb8;
    --heal: #6aaa6a;
    --tank: #5a8fbf;
    --support: #c9a227;
  }
  * { box-sizing: border-box; }
  html { scroll-behavior: smooth; }
  body {
    margin: 0;
    min-height: 100vh;
    font-family: "Segoe UI", Tahoma, sans-serif;
    background:
      radial-gradient(ellipse 80% 55% at 50% 0%, rgba(235, 192, 71, 0.12) 0%, transparent 55%),
      linear-gradient(180deg, #12141a 0%, var(--bg) 45%),
      var(--bg);
    color: var(--text);
    line-height: 1.55;
  }
  .wrap { max-width: 860px; margin: 0 auto; padding: 28px 22px 64px; }
  .hero { margin-bottom: 22px; padding-bottom: 18px; border-bottom: 1px solid var(--border-soft); }
  .eyebrow {
    margin: 0 0 8px; font-size: 0.78rem; letter-spacing: 0.14em;
    text-transform: uppercase; color: var(--gold-dim);
  }
  h1 { margin: 0 0 8px; font-size: 2rem; font-weight: 700; color: var(--gold); letter-spacing: 0.01em; }
  .tagline { margin: 0; color: var(--muted); font-size: 1.02rem; }
  nav.toc { display: flex; flex-wrap: wrap; gap: 8px; margin: 0 0 22px; }
  nav.toc a {
    color: var(--gold); text-decoration: none; font-size: 0.86rem;
    padding: 6px 10px; border: 1px solid var(--border); background: var(--accent);
  }
  nav.toc a:hover { background: #2c2416; }
  section.block {
    background: var(--panel); border: 1px solid var(--border); margin-bottom: 18px;
  }
  section.block > .head {
    padding: 14px 18px 14px 16px; border-bottom: 1px solid var(--border-soft);
    border-left: 3px solid var(--gold);
    background: linear-gradient(90deg, #1a1710 0%, var(--panel-2) 70%);
  }
  section.block > .head h2 { margin: 0; font-size: 1.2rem; color: var(--gold-bright); font-weight: 700; }
  section.block > .head p { margin: 6px 0 0; font-size: 0.92rem; color: var(--muted); }
  .body { padding: 16px 18px 18px; }
  .note {
    margin: 0 0 14px; padding: 12px 14px; background: var(--accent);
    border-left: 3px solid var(--gold-dim); color: var(--muted); font-size: 0.92rem;
  }
  .note strong { color: var(--text); }
  table.sheet { width: 100%; border-collapse: collapse; font-size: 0.92rem; }
  table.sheet th, table.sheet td {
    text-align: left; padding: 10px 12px; border: 1px solid var(--border); vertical-align: top;
  }
  table.sheet th { background: #1a1710; color: var(--gold); font-weight: 600; }
  table.sheet td { background: var(--panel-2); }
  table.sheet td.role { white-space: nowrap; font-weight: 650; color: var(--gold-bright); width: 22%; }
  table.sheet td.num {
    width: 2.5rem; text-align: center; font-weight: 700; color: var(--gold-dim);
    font-variant-numeric: tabular-nums;
  }
  table.sheet tr:hover td { background: #16130c; }
  .badge {
    display: inline-block; padding: 2px 8px; font-size: 0.72rem; font-weight: 700;
    letter-spacing: 0.05em; text-transform: uppercase; border: 1px solid var(--border);
    background: var(--accent); color: var(--gold-dim);
  }
  .badge-cm {
    color: #e8a090; border-color: rgba(196, 92, 74, 0.45);
    background: rgba(196, 92, 74, 0.18);
  }
  .badge-ok {
    color: #a8d0a8; border-color: rgba(106, 170, 106, 0.45);
    background: rgba(106, 170, 106, 0.14);
  }
  .badge-scale {
    color: var(--gold-bright); border-color: var(--border);
    background: #1a1710; font-family: Consolas, "Courier New", monospace;
  }
  .checks { list-style: none; margin: 0; padding: 0; }
  .checks li {
    display: flex; gap: 12px; align-items: flex-start;
    padding: 10px 12px; margin: 0 0 8px;
    background: var(--panel-2); border: 1px solid var(--border-soft);
    border-left: 3px solid var(--gold-dim); color: var(--muted); font-size: 0.92rem;
  }
  .checks li:last-child { margin-bottom: 0; }
  .checks .box {
    flex: 0 0 auto; width: 1.05rem; height: 1.05rem; margin-top: 2px;
    border: 1px solid var(--border); background: var(--accent);
  }
  .checks strong { color: var(--text); }
  .callout-grid {
    display: grid; grid-template-columns: 1fr 1fr; gap: 10px;
  }
  @media (max-width: 640px) { .callout-grid { grid-template-columns: 1fr; } }
  .callout {
    padding: 14px 14px 12px; background: var(--panel-2);
    border: 1px solid var(--border-soft); border-left: 3px solid var(--gold);
  }
  .callout h3 {
    margin: 0 0 6px; font-family: Georgia, "Palatino Linotype", Palatino, serif;
    font-size: 1.02rem; color: var(--text);
  }
  .callout p { margin: 0; font-size: 0.86rem; color: var(--muted); line-height: 1.45; }
  .callout .tags { margin: 0 0 8px; }
  .tag {
    display: inline-block; padding: 2px 8px; border-radius: 2px; font-size: 0.78rem;
    font-weight: 650; letter-spacing: 0.02em; margin: 0 4px 4px 0; white-space: nowrap;
  }
  .tag-power { background: rgba(196, 92, 74, 0.22); color: #e8a090; border: 1px solid rgba(196, 92, 74, 0.45); }
  .tag-condi { background: rgba(155, 123, 184, 0.2); color: #c4aee0; border: 1px solid rgba(155, 123, 184, 0.45); }
  .tag-heal  { background: rgba(106, 170, 106, 0.2); color: #a8d0a8; border: 1px solid rgba(106, 170, 106, 0.45); }
  .tag-tank  { background: rgba(90, 143, 191, 0.2); color: #9bc0e0; border: 1px solid rgba(90, 143, 191, 0.45); }
  .tag-sup   { background: rgba(235, 192, 71, 0.12); color: var(--gold); border: 1px solid var(--border); }
  .tag-misc  { background: rgba(168, 174, 184, 0.12); color: var(--muted); border: 1px solid rgba(168, 174, 184, 0.3); }
  .muted { color: var(--muted); }
  .list { margin: 0; padding-left: 1.15em; color: var(--muted); font-size: 0.92rem; }
  .list li { margin: 4px 0; }
  .list strong { color: var(--text); }
  /* —— Waypoint / chat-link cards (Uber's All-In-One) —— */
  .howto {
    display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px;
    margin: 0;
  }
  @media (max-width: 640px) {
    .howto { grid-template-columns: 1fr; }
  }
  .howto .step {
    padding: 12px 14px; background: var(--panel-2);
    border: 1px solid var(--border-soft); border-top: 2px solid var(--gold-dim);
  }
  .howto .step .n {
    display: block; font-size: 0.72rem; letter-spacing: 0.12em;
    text-transform: uppercase; color: var(--gold-dim); margin-bottom: 6px; font-weight: 700;
  }
  .howto .step p { margin: 0; font-size: 0.88rem; color: var(--muted); line-height: 1.45; }
  .howto .step strong { color: var(--text); }
  .wp-grid {
    display: grid; grid-template-columns: 1fr 1fr; gap: 10px;
  }
  @media (max-width: 640px) {
    .wp-grid { grid-template-columns: 1fr; }
  }
  .wp-grid.single { grid-template-columns: 1fr; max-width: 420px; }
  .wp {
    display: flex; flex-direction: column; gap: 10px;
    padding: 14px 14px 12px;
    background: var(--panel-2);
    border: 1px solid var(--border-soft);
    border-left: 3px solid var(--gold-dim);
    transition: border-color 0.15s ease, background 0.15s ease;
  }
  .wp:hover {
    border-color: var(--border);
    background: #16130c;
  }
  .wp.kind-hub { border-left-color: var(--gold); }
  .wp.kind-vault { border-left-color: #5a8fbf; }
  .wp.kind-farm { border-left-color: #6aaa6a; }
  .wp.kind-token { border-left-color: #9b7bb8; }
  .wp .meta {
    display: flex; flex-wrap: wrap; align-items: center; gap: 6px;
  }
  .wp .kind {
    display: inline-block; padding: 2px 7px; font-size: 0.68rem; font-weight: 700;
    letter-spacing: 0.06em; text-transform: uppercase;
    color: var(--gold-dim); border: 1px solid var(--border); background: var(--accent);
  }
  .wp .purpose {
    display: inline-block; padding: 2px 7px; font-size: 0.68rem; font-weight: 650;
    letter-spacing: 0.04em; text-transform: uppercase;
    color: #9bc0e0; border: 1px solid rgba(90, 143, 191, 0.4);
    background: rgba(90, 143, 191, 0.12);
  }
  .wp .purpose.farm {
    color: #a8d0a8; border-color: rgba(106, 170, 106, 0.4);
    background: rgba(106, 170, 106, 0.12);
  }
  .wp .purpose.token {
    color: #c4aee0; border-color: rgba(155, 123, 184, 0.4);
    background: rgba(155, 123, 184, 0.12);
  }
  .wp .name {
    margin: 0; font-family: Georgia, "Palatino Linotype", Palatino, serif;
    font-size: 1.05rem; font-weight: 700; color: var(--text); line-height: 1.25;
  }
  .wp .map {
    margin: 0; color: var(--muted); font-size: 0.84rem;
  }
  .wp .actions {
    display: flex; flex-wrap: wrap; align-items: center; gap: 8px;
    margin-top: auto; padding-top: 2px;
  }
  .wp .code {
    appearance: none; -webkit-appearance: none;
    font-family: Consolas, "Courier New", monospace; font-size: 0.82rem;
    color: var(--gold-bright); background: var(--accent);
    border: 1px solid var(--border); padding: 6px 10px; cursor: pointer;
    letter-spacing: 0.02em; user-select: all; border-radius: 0; line-height: 1.3;
  }
  .wp .code:hover { background: #2c2416; border-color: var(--gold-dim); }
  .wp .code:focus { outline: 1px solid var(--gold); outline-offset: 1px; }
  .wp .code.copied {
    color: #a8d0a8; border-color: rgba(106, 170, 106, 0.55);
    background: rgba(106, 170, 106, 0.12);
  }
  .wp .hint {
    font-size: 0.72rem; letter-spacing: 0.04em; text-transform: uppercase;
    color: var(--gold-dim); font-weight: 650;
  }
  .wp a.wiki {
    margin-left: auto; font-size: 0.78rem; color: var(--muted);
    text-decoration: none; border-bottom: 1px solid transparent;
  }
  .wp a.wiki:hover { color: var(--gold); border-bottom-color: var(--border); }
  .credit {
    margin: 8px 0 0; padding: 14px 18px;
    border: 1px solid var(--border-soft); background: var(--panel);
    color: var(--muted); font-size: 0.88rem; text-align: center;
  }
  .credit strong { color: var(--gold); font-weight: 650; }
  footer { margin-top: 10px; color: var(--muted); font-size: 0.85rem; }
  @media (max-width: 560px) {
    h1 { font-size: 1.55rem; }
    table.sheet td.role { white-space: normal; width: auto; }
  }
)CSS";

	std::string BuildHtml(
		const char* title,
		const char* eyebrow,
		const char* heading,
		const char* tagline,
		const char* toc,
		const char* body)
	{
		std::string out;
		out.reserve(12000);
		out += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\"/>\n";
		out += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n<title>";
		out += title;
		out += "</title>\n<style>\n";
		out += kSharedCss;
		out += "\n</style>\n</head>\n<body>\n<div class=\"wrap\">\n<header class=\"hero\">\n";
		out += "<p class=\"eyebrow\">";
		out += eyebrow;
		out += "</p>\n<h1>";
		out += heading;
		out += "</h1>\n<p class=\"tagline\">";
		out += tagline;
		out += "</p>\n</header>\n";
		if (toc && toc[0])
		{
			out += "<nav class=\"toc\" aria-label=\"Sections\">\n";
			out += toc;
			out += "\n</nav>\n";
		}
		out += body;
		out += "\n<footer>Built into GW2 In-Game Helper. Not affiliated with ArenaNet or NCSoft. "
			   "Informational reference only — check current meta for your squad.</footer>\n"
			   "</div>\n</body>\n</html>\n";
		return out;
	}

	/* —— Page bodies —— */

	std::string HtmlRaidUtilities()
	{
		return BuildHtml(
			"Raid Utilities — Oils, Stones &amp; Crystals",
			"Guild Wars 2 · Consumable Reference",
			"Raid Utilities",
			"Enhancement (utility) picks by role — oils, sharpening stones, focusing crystals, and enrichment.",
			"<a href=\"#enhancement\">Enhancement</a>\n"
			"<a href=\"#enrichment\">Enrichment</a>\n"
			"<a href=\"#notes\">Notes</a>",
			R"BODY(
  <section class="block" id="enhancement">
    <div class="head">
      <h2>Enhancement by Role</h2>
      <p>Slot in the utility / enhancement consumable bar (30 min). Pair with feast food from Raid Food.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Role</th><th>Common pick</th><th>What it gives</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role"><span class="tag tag-power">Power</span></td>
            <td>Superior Sharpening Stone</td>
            <td>Power + Ferocity</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-condi">Condition</span></td>
            <td>Toxic Focusing Crystal</td>
            <td>Condition Damage + Expertise</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-heal">Heal</span></td>
            <td>Bountiful Maintenance Oil</td>
            <td>Healing Power + Concentration</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-sup">Boon DPS / Support</span></td>
            <td>Enhanced Lucent Oil <span class="muted">or</span> Furious Tuning Crystal</td>
            <td>Concentration-focused (Lucent) or Power/Precision/Ferocity (Furious) — match the build</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-tank">Tank</span></td>
            <td>Hardened Sharpening Stone <span class="muted">or</span> Bountiful Maintenance Oil</td>
            <td>Toughness + Power, or Healing/Concentration if the tank heals</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Also fine:</strong> Master Tuning Crystal (condi), Peppermint Oil (heal/support seasonal),
      Potent variants when you have them. Feasts already cover nourishment — don’t double-stack the same food type.</p>
    </div>
  </section>

  <section class="block" id="enrichment">
    <div class="head">
      <h2>Enrichment (Ascended Jewelry)</h2>
      <p>One enrichment per ascended accessory / ring / amulet (account-bound upgrades).</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Role</th><th>Typical enrichment</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role"><span class="tag tag-power">Power</span></td>
            <td>Mighty / Precise / Malign (+attribute) — often Power or Precision; some use Ferocity</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-condi">Condition</span></td>
            <td>Malign / Raven / Expertise-leaning picks matching the build’s condition attribute</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-heal">Heal / Quick / Alac</span></td>
            <td>Concentration enrichments (or Healing Power if the build is heal-stacked)</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-tank">Tank</span></td>
            <td>Toughness or Concentration depending on whether you need more toughness or boon duration</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Agony:</strong> Fractal / CM players often prefer +5 Agony Resistance enrichments on jewelry.
      Raids do not require AR — prioritize attributes for raid-only characters.</p>
    </div>
  </section>

  <section class="block" id="notes">
    <div class="head">
      <h2>Quick Notes</h2>
      <p>Stacking and shopping reminders.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Food + utility</strong> — both should be up; feast nourishment + enhancement utility.</li>
        <li><strong>Shared feasts</strong> — one cook can cover the squad; utilities are personal.</li>
        <li><strong>WvW / open world</strong> — different oils/stones exist; this sheet is raid/strike oriented.</li>
        <li><strong>Build overrides</strong> — always follow your build page if it names a specific utility.</li>
      </ul>
    </div>
  </section>
)BODY");
	}

	std::string HtmlFractalConsumables()
	{
		return BuildHtml(
			"Fractal Consumables — Potions, Mistlock &amp; Agony",
			"Guild Wars 2 · Fractals Reference",
			"Fractal Consumables",
			"Potions, mist utilities, agony resistance basics — offline glance sheet for fractal runs.",
			"<a href=\"#potions\">Potions</a>\n"
			"<a href=\"#utility\">Utility</a>\n"
			"<a href=\"#agony\">Agony</a>\n"
			"<a href=\"#mistlock\">Mistlock</a>",
			R"BODY(
  <section class="block" id="potions">
    <div class="head">
      <h2>Mist Potions</h2>
      <p>Fractal-specific potions from the Mist Potions vendor / crafting. Use one offensive stack.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Potion</th><th>Use</th></tr>
        </thead>
        <tbody>
          <tr>
            <td><strong>Mist Offensive Potion</strong></td>
            <td>Default DPS pick — outgoing damage in fractals</td>
          </tr>
          <tr>
            <td><strong>Mist Defensive Potion</strong></td>
            <td>Tank / rough CMs — incoming damage reduction</td>
          </tr>
          <tr>
            <td><strong>Mist Mobility Potion</strong></td>
            <td>Skip-heavy or movement-heavy scales (situational)</td>
          </tr>
          <tr>
            <td><strong>Integrated / account potions</strong></td>
            <td>If you own combined/infinite variants, use those instead of buying stacks</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Tip:</strong> Offensive is the usual T4 / CM default. Defensive is fine on tank or when learning a CM.</p>
    </div>
  </section>

  <section class="block" id="utility">
    <div class="head">
      <h2>Food &amp; Enhancement</h2>
      <p>Same role logic as raids; fractals still benefit from feast-quality food when available.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Role</th><th>Enhancement</th><th>Nourishment</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role"><span class="tag tag-power">Power</span></td>
            <td>Superior Sharpening Stone</td>
            <td>Power feast / steak equivalents</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-condi">Condition</span></td>
            <td>Toxic Focusing Crystal</td>
            <td>Condition flatbread feasts</td>
          </tr>
          <tr>
            <td class="role"><span class="tag tag-heal">Heal / Alac / Quick</span></td>
            <td>Bountiful Maintenance Oil / Lucent Oil</td>
            <td>Support feast / crème brûlée as needed</td>
          </tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="agony">
    <div class="head">
      <h2>Agony Resistance (AR)</h2>
      <p>Required for higher fractal scales. Comes from infusions + some enrichment.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Tier</th><th>Typical AR needed</th></tr>
        </thead>
        <tbody>
          <tr><td>T1 (1–25)</td><td>0</td></tr>
          <tr><td>T2 (26–50)</td><td>~10+</td></tr>
          <tr><td>T3 (51–75)</td><td>~20+</td></tr>
          <tr><td>T4 (76–100)</td><td>~40+ base agony</td></tr>
          <tr><td>Challenge Motes / CM</td><td>Often <strong>150+</strong> via +5 / +7 / +9 agony infusions</td></tr>
        </tbody>
      </table>
      <ul class="list" style="margin-top:14px">
        <li><strong>Infusions</strong> — socket into ascended/legendary gear. Recycle via mystic forge / vendors when upgrading.</li>
        <li><strong>+9 agony</strong> — endgame fractal standard on full ascended.</li>
        <li><strong>Enrichment</strong> — +5 AR jewelry enrichments help early AR goals.</li>
      </ul>
    </div>
  </section>

  <section class="block" id="mistlock">
    <div class="head">
      <h2>Mistlock Observatory</h2>
      <p>Lobby buffs and quality-of-life between fractals.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Mistlock Singularities</strong> — banked account buffs (damage, etc.); keep them active for T4/CM.</li>
        <li><strong>Fractal potions vendor</strong> — restock Mist potions here.</li>
        <li><strong>Agony Impedance</strong> / related mastery — reduces AR needs; unlock fractal masteries.</li>
        <li><strong>Encapsulated Essences</strong> — daily CM / recommended rewards currency path.</li>
        <li><strong>No strategies here</strong> — use Mukluk / TLDR Fractals links in Guides for fight notes.</li>
      </ul>
    </div>
  </section>
)BODY");
	}

	std::string HtmlSigilsRunes()
	{
		return BuildHtml(
			"Sigils &amp; Runes — Common Role Picks",
			"Guild Wars 2 · Gear Reference",
			"Sigils &amp; Runes",
			"Common raid / strike / fractal picks by role — not every option, just the usual defaults.",
			"<a href=\"#power\">Power</a>\n"
			"<a href=\"#condition\">Condition</a>\n"
			"<a href=\"#support\">Support</a>\n"
			"<a href=\"#tank\">Tank</a>",
			R"BODY(
  <section class="block" id="power">
    <div class="head">
      <h2>Power Damage</h2>
      <p>Direct damage weapons and armor.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Slot</th><th>Common picks</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role">Runes</td>
            <td>Scholar · Strength · Fireworks · Thief · Eagle · Ranger (build-dependent)</td>
          </tr>
          <tr>
            <td class="role">Sigils</td>
            <td><strong>Force</strong> + Accuracy / Air / Impact / Illusions (chrono) — match weapon swap &amp; fight length</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Rule of thumb:</strong> Force on the high-uptime weapon; Accuracy if you need crit cap; Air/Impact for burst windows.</p>
    </div>
  </section>

  <section class="block" id="condition">
    <div class="head">
      <h2>Condition Damage</h2>
      <p>Bleed / burn / torment / poison focused sets.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Slot</th><th>Common picks</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role">Runes</td>
            <td>Tormenting · Afflicted · Nightmare · Trapper · Krait · Undead</td>
          </tr>
          <tr>
            <td class="role">Sigils</td>
            <td><strong>Malice</strong> + Doom / Earth / Bursting / Torment / Geomancy — per condition type</td>
          </tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="support">
    <div class="head">
      <h2>Heal / Quick / Alac</h2>
      <p>Boon duration and healing power sets.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Slot</th><th>Common picks</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role">Runes</td>
            <td>Monk · Water · Fireworks · Sanctuary · Altruism · Leadership (situational)</td>
          </tr>
          <tr>
            <td class="role">Sigils</td>
            <td><strong>Concentration</strong> + Water / Docile / Transference / Timing — heal vs pure boon DPS differs</td>
          </tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="tank">
    <div class="head">
      <h2>Tank / Toughness</h2>
      <p>Banners / chronotank / heal tank variants.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Slot</th><th>Common picks</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role">Runes</td>
            <td>Fireworks · Traveler · Dolyak · Monk (heal-tank)</td>
          </tr>
          <tr>
            <td class="role">Sigils</td>
            <td>Concentration + Absorption / Superiority / or DPS sigils if the tank still deals damage</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Always defer</strong> to Snowcrows / MetaBattle / your static’s sheet — rune/sigil meta shifts with balance patches.</p>
    </div>
  </section>
)BODY");
	}

	std::string HtmlRelics()
	{
		return BuildHtml(
			"Relics — Picks by Role",
			"Guild Wars 2 · Relic Reference",
			"Relics",
			"Short “what to socket” sheet by role and damage type. One relic per build.",
			"<a href=\"#power\">Power</a>\n"
			"<a href=\"#condition\">Condition</a>\n"
			"<a href=\"#support\">Support</a>\n"
			"<a href=\"#tank\">Tank</a>",
			R"BODY(
  <section class="block" id="power">
    <div class="head">
      <h2>Power</h2>
      <p>Crit / burst / on-tag relics common in raids and strikes.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Relic of Fireworks</strong> — classic on-crit damage; still a safe default for many power builds</li>
        <li><strong>Relic of the Thief</strong> / <strong>Eir</strong> / <strong>Dragonhunter</strong> — profession or rotation-gated power options</li>
        <li><strong>Relic of the Wizard's Tower</strong> / <strong>Leadership</strong> — when the build stacks might or specific triggers</li>
        <li><strong>Relic of the Warrior</strong> — banner / power-support hybrids</li>
      </ul>
    </div>
  </section>

  <section class="block" id="condition">
    <div class="head">
      <h2>Condition</h2>
      <p>Condition application and duration oriented relics.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Relic of the Fractal</strong> — strong general condi default for many builds</li>
        <li><strong>Relic of Ahamkara</strong> / <strong>Peitha</strong> / <strong>Afflicted</strong> — condition-type specialists</li>
        <li><strong>Relic of the Nightmare</strong> — torment / fear oriented kits</li>
        <li><strong>Relic of the Astral Ward</strong> — when the build page calls for it</li>
      </ul>
    </div>
  </section>

  <section class="block" id="support">
    <div class="head">
      <h2>Heal / Quick / Alac</h2>
      <p>Healing and boon uptime relics.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Relic of the Monk</strong> — heal support staple</li>
        <li><strong>Relic of the Flock</strong> / <strong>Mercy</strong> / <strong>Karakosa</strong> — alternate heal procs</li>
        <li><strong>Relic of Leadership</strong> / <strong>Speed</strong> / <strong>Chronomancer</strong> — boon DPS / quick / alac variants</li>
        <li><strong>Relic of the Mirage</strong> — when playing mirage support lines</li>
      </ul>
    </div>
  </section>

  <section class="block" id="tank">
    <div class="head">
      <h2>Tank</h2>
      <p>Survivability and group utility.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Relic of the Midnight King</strong> — common tank / toughness-oriented pick</li>
        <li><strong>Relic of Durability</strong> — mitigation focused</li>
        <li><strong>Heal-tank</strong> — often shares heal relics (Monk / Flock) instead</li>
      </ul>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>One relic only.</strong> Follow your build’s named relic when it disagrees with this sheet — balance patches move the goalposts.</p>
    </div>
  </section>
)BODY");
	}

	std::string HtmlBoonChecklist()
	{
		return BuildHtml(
			"Boon Checklist — Squad Coverage",
			"Guild Wars 2 · Raid Squad Reference",
			"Boon Checklist",
			"Who usually covers what — quick, alac, might, fury, and the rest. Roles overlap; assign explicitly in squad.",
			"<a href=\"#priority\">Priority</a>\n"
			"<a href=\"#sources\">Sources</a>\n"
			"<a href=\"#situational\">Situational</a>",
			R"BODY(
  <section class="block" id="priority">
    <div class="head">
      <h2>Priority Boons</h2>
      <p>These should be covered every raid / strike pull.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Boon</th><th>Why</th><th>Typical coverage</th></tr>
        </thead>
        <tbody>
          <tr>
            <td><strong>Quickness</strong></td>
            <td>Cast / skill speed</td>
            <td>Chronomancer · Catalyst · Harbinger · Willbender · Specter · others</td>
          </tr>
          <tr>
            <td><strong>Alacrity</strong></td>
            <td>Cooldown reduction</td>
            <td>Chronomancer · Specter · Mechanist · Druid · Catalyst · others</td>
          </tr>
          <tr>
            <td><strong>Might (25)</strong></td>
            <td>Power &amp; condi damage</td>
            <td>Firebrand · Warrior banners · many DPS kits · supports</td>
          </tr>
          <tr>
            <td><strong>Fury</strong></td>
            <td>Crit chance</td>
            <td>Almost every squad — DPS + supports share this</td>
          </tr>
          <tr>
            <td><strong>Protection</strong></td>
            <td>Damage reduction</td>
            <td>Firebrand · healers · tanks</td>
          </tr>
          <tr>
            <td><strong>Regeneration</strong></td>
            <td>Passive healing</td>
            <td>Healers · some supports</td>
          </tr>
          <tr>
            <td><strong>Resolution</strong></td>
            <td>Condition damage taken ↓</td>
            <td>Firebrand · guardians · healers</td>
          </tr>
          <tr>
            <td><strong>Vigor</strong></td>
            <td>Endurance regen</td>
            <td>Supports / thieves / warriors — often incidental</td>
          </tr>
          <tr>
            <td><strong>Swiftness</strong></td>
            <td>Movement</td>
            <td>Usually covered passively; pull if pulls are slow</td>
          </tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="sources">
    <div class="head">
      <h2>Squad Slot Checklist</h2>
      <p>Use before ready-check — tick coverage, don’t assume.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><span class="tag tag-sup">Quick</span> named player / build</li>
        <li><span class="tag tag-sup">Alac</span> named player / build</li>
        <li><span class="tag tag-heal">Heal</span> (often doubles prot / regen / resolution)</li>
        <li><span class="tag tag-tank">Tank</span> (if fight needs one) — toughness check</li>
        <li><span class="tag tag-power">Might</span> plan (FB / banners / kits)</li>
        <li><span class="tag tag-misc">Fury</span> — confirm someone actually brings it</li>
      </ul>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Double quick / double alac</strong> comps are normal. Hybrid boon-DPS can cover both — still say it in chat.</p>
    </div>
  </section>

  <section class="block" id="situational">
    <div class="head">
      <h2>Situational</h2>
      <p>Bring when the fight asks for them.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Boon</th><th>When</th></tr>
        </thead>
        <tbody>
          <tr><td><strong>Stability</strong></td><td>Knockback / fear / pull fights (e.g. many raid bosses, escort)</td></tr>
          <tr><td><strong>Aegis</strong></td><td>Blocked hit mechanics</td></tr>
          <tr><td><strong>Resistance</strong></td><td>Heavy condition phases</td></tr>
          <tr><td><strong>Superspeed</strong></td><td>Not a boon, but Scrapper / accel often requested on skips</td></tr>
        </tbody>
      </table>
    </div>
  </section>
)BODY");
	}

	std::string HtmlCcDefiance()
	{
		return BuildHtml(
			"CC / Defiance — Breakbar by Profession",
			"Guild Wars 2 · Combat Reference",
			"CC / Defiance",
			"Common hard and soft CC for breakbars — mid-fight glance sheet, not a full skill list.",
			"<a href=\"#basics\">Basics</a>\n"
			"<a href=\"#professions\">Professions</a>\n"
			"<a href=\"#tips\">Tips</a>",
			R"BODY(
  <section class="block" id="basics">
    <div class="head">
      <h2>Breakbar Basics</h2>
      <p>Blue defiance bar = dump CC. Soft CC builds slowly; hard CC spikes.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Type</th><th>Examples</th></tr>
        </thead>
        <tbody>
          <tr>
            <td class="role">Hard CC</td>
            <td>Stun · Knockdown · Knockback · Launch · Float · Sink · Daze (strong) · Pull</td>
          </tr>
          <tr>
            <td class="role">Soft CC</td>
            <td>Immobilize · Cripple · Chill · Fear · Taunt · Slow</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Save big CC</strong> for the bar. Don’t waste long cooldowns on empty bars.</p>
    </div>
  </section>

  <section class="block" id="professions">
    <div class="head">
      <h2>Common Tools by Profession</h2>
      <p>Typical raid/fractal skills people reach for — elite specs vary.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Profession</th><th>Go-to CC</th></tr>
        </thead>
        <tbody>
          <tr><td class="role">Guardian</td><td>Hammer 3 / 4 · Shield · Banish · Binding Blade pull · Willbender leaps / sword</td></tr>
          <tr><td class="role">Revenant</td><td>Hammer · Staff knockbacks · Legendary Demon / Centaur skills · Renegade citadel</td></tr>
          <tr><td class="role">Warrior</td><td>Hammer · Headbutt · Rampage · Bull's Charge · Earthen Impact (Spellbreaker)</td></tr>
          <tr><td class="role">Engineer</td><td>Electric Discharge · Tool kit · Mortar · Soft CC kits · Holosmith photon</td></tr>
          <tr><td class="role">Ranger</td><td>Pet CC · Short bow · Thunderclap · Soulbeast merges · Untamed Unleash</td></tr>
          <tr><td class="role">Thief</td><td>Basilisk Venom · Pistol whip · Impact strike · Shadowswarm (Specter)</td></tr>
          <tr><td class="role">Elementalist</td><td>Lightning Flash / draughts · Earthquake · Frozen Ground · Catalyst jade spheres</td></tr>
          <tr><td class="role">Mesmer</td><td>Illusionary Wave · Into the Void · Signet of Domination · Continuum Split setups · Mirage ambush</td></tr>
          <tr><td class="role">Necromancer</td><td>Fear (various) · Spectral Grasp · Flesh Wurm · Harbinger elixirs · Harbinger shroud</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="tips">
    <div class="head">
      <h2>Squad Tips</h2>
      <p>Coordination beats random CC.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Call the bar</strong> — “CC now” beats everyone holding or everyone dumping early.</li>
        <li><strong>Stuns stack poorly on immunity</strong> — after a break, many bosses go defiance-immune briefly.</li>
        <li><strong>Pulls / knocks</strong> — can ruin stack; know if the boss can be moved.</li>
        <li><strong>Special action key</strong> — some encounters give encounter CC (use it).</li>
      </ul>
    </div>
  </section>
)BODY");
	}

	std::string HtmlRaidWings()
	{
		return BuildHtml(
			"Raid Wings Overview",
			"Guild Wars 2 · Raids Reference",
			"Raid Wings Overview",
			"Wing → bosses → typical clear KP. No strategies — just the map of what exists.",
			"<a href=\"#w1\">W1</a>\n"
			"<a href=\"#w2\">W2</a>\n"
			"<a href=\"#w3\">W3</a>\n"
			"<a href=\"#w4\">W4</a>\n"
			"<a href=\"#w5\">W5</a>\n"
			"<a href=\"#w6\">W6</a>\n"
			"<a href=\"#w7\">W7</a>",
			R"BODY(
  <section class="block" id="w1">
    <div class="head"><h2>Wing 1 — Spirit Vale</h2><p>Forsaken Thicket · clear KP per boss.</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Boss</th><th>Typical KP</th></tr></thead>
        <tbody>
          <tr><td>Vale Guardian</td><td>VG clear</td></tr>
          <tr><td>Gorseval the Multifarious</td><td>Gorseval clear</td></tr>
          <tr><td>Sabetha the Saboteur</td><td>Sabetha clear</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="w2">
    <div class="head"><h2>Wing 2 — Salvation Pass</h2><p>Forsaken Thicket.</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Boss</th><th>Typical KP</th></tr></thead>
        <tbody>
          <tr><td>Slothasor</td><td>Slothasor clear</td></tr>
          <tr><td>Bandit Trio (Berg / Zane / Narella)</td><td>Trio clear</td></tr>
          <tr><td>Matthias Gabrel</td><td>Matthias clear</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="w3">
    <div class="head"><h2>Wing 3 — Stronghold of the Faithful</h2><p>Forsaken Thicket.</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Boss / Encounter</th><th>Typical KP</th></tr></thead>
        <tbody>
          <tr><td>Siege the Stronghold (Escort)</td><td>Escort clear</td></tr>
          <tr><td>Keep Construct</td><td>KC clear</td></tr>
          <tr><td>Twisted Castle</td><td>Twisted Castle clear</td></tr>
          <tr><td>Xera</td><td>Xera clear</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="w4">
    <div class="head"><h2>Wing 4 — Bastion of the Penitent</h2><p></p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Boss</th><th>Typical KP</th></tr></thead>
        <tbody>
          <tr><td>Cairn the Indomitable</td><td>Cairn clear</td></tr>
          <tr><td>Mursaat Overseer</td><td>MO clear</td></tr>
          <tr><td>Samarog</td><td>Samarog clear</td></tr>
          <tr><td>Deimos</td><td>Deimos clear</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="w5">
    <div class="head"><h2>Wing 5 — Hall of Chains</h2><p></p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Boss / Encounter</th><th>Typical KP</th></tr></thead>
        <tbody>
          <tr><td>Soulless Horror</td><td>SH clear</td></tr>
          <tr><td>River of Souls</td><td>River clear</td></tr>
          <tr><td>Statues of Grenth (Eyes / Voice)</td><td>Statues clear</td></tr>
          <tr><td>Dhuum</td><td>Dhuum clear</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="w6">
    <div class="head"><h2>Wing 6 — Mythwright Gambit</h2><p></p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Boss</th><th>Typical KP</th></tr></thead>
        <tbody>
          <tr><td>Conjured Amalgamate</td><td>CA clear</td></tr>
          <tr><td>Twin Largos (Nikare / Kenut)</td><td>Largos clear</td></tr>
          <tr><td>Qadim</td><td>Qadim clear</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="w7">
    <div class="head"><h2>Wing 7 — The Key of Ahdashim</h2><p></p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Boss</th><th>Typical KP</th></tr></thead>
        <tbody>
          <tr><td>Cardinal Sabir</td><td>Sabir clear</td></tr>
          <tr><td>Cardinal Adina</td><td>Adina clear</td></tr>
          <tr><td>Qadim the Peerless</td><td>QPeerless / QtP clear</td></tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>KP:</strong> KillProof.me tracks clears / CMs / title KP. LFG often asks “full KP” meaning prior clears on that wing’s bosses.
      Strategies → TLDR Raids / Snowcrows links in Guides.</p>
    </div>
  </section>
)BODY");
	}

	std::string HtmlHomeGarden()
	{
		return BuildHtml(
			"Home Garden — Cultivated Herbs",
			"Guild Wars 2 · Cooking Reference",
			"Home Garden",
			"What to plant for raid feast seasonings — companion to the Raid Food sheet.",
			"<a href=\"#seasonings\">Seasonings</a>\n"
			"<a href=\"#planting\">Planting</a>\n"
			"<a href=\"#other\">Also useful</a>",
			R"BODY(
  <section class="block" id="seasonings">
    <div class="head">
      <h2>Cultivated Herbs → Feast Utility</h2>
      <p>Matches the Universal Seasoning Guide on Raid Food.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead>
          <tr><th>Plant</th><th>Feast utility</th><th>Priority</th></tr>
        </thead>
        <tbody>
          <tr>
            <td><strong>Cultivated Cilantro</strong></td>
            <td><span class="tag tag-power">Life Steal on Crit</span></td>
            <td>High — most DPS feasts</td>
          </tr>
          <tr>
            <td><strong>Cultivated Peppercorn</strong></td>
            <td><span class="tag tag-tank">−10% Incoming Damage</span></td>
            <td>High — tank / safe feasts</td>
          </tr>
          <tr>
            <td><strong>Cultivated Mint</strong></td>
            <td><span class="tag tag-heal">+10% Outgoing Healing</span></td>
            <td>High — heal / support desserts &amp; plates</td>
          </tr>
          <tr>
            <td><strong>Cultivated Clove</strong></td>
            <td><span class="tag tag-condi">−20% Incoming Condition Duration</span></td>
            <td>Medium — condi-heavy fights</td>
          </tr>
          <tr>
            <td><strong>Cultivated Sesame</strong></td>
            <td><span class="tag tag-heal">Health Regeneration</span></td>
            <td>Medium — hybrid / openers</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Lime &amp; Divinity Fair Herbs</strong> still appear in recipes — garden herbs are the cultivated third ingredient that sets the utility.</p>
    </div>
  </section>

  <section class="block" id="planting">
    <div class="head">
      <h2>Planting Tips</h2>
      <p>Home instance garden / homestead garden nodes.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Stock Cilantro + Peppercorn first</strong> — cover the two most requested feast utilities.</li>
        <li><strong>Mint</strong> — keep a row if you cook for healers / crème brûlée.</li>
        <li><strong>Harvest daily</strong> — cultivated plants are account-bound ingredients; grow more than you think you need before raid night.</li>
        <li><strong>Open Raid Food</strong> — full recipes &amp; bases live on that sheet.</li>
      </ul>
    </div>
  </section>

  <section class="block" id="other">
    <div class="head">
      <h2>Also Useful in the Garden</h2>
      <p>Not seasonings, but common feast ingredients.</p>
    </div>
    <div class="body">
      <ul class="list">
        <li><strong>Sawgill / mushrooms</strong> — some clove steak variants</li>
        <li><strong>Butternut Squash</strong> — veggie flatbreads</li>
        <li><strong>Allium / garlic nodes</strong> — coq au vin &amp; sauce bases (as available)</li>
        <li><strong>Channel Bound / other gardens</strong> — same cultivated herbs if your instance layout differs</li>
      </ul>
    </div>
  </section>
)BODY");
	}

	std::string HtmlUbersAllInOne()
	{
		/* Waypoint / landmark chat links — copy & paste into game chat to open map. */
		return BuildHtml(
			"Uber's All-In-One — Waypoints",
			"Guild Wars 2 · Waypoint Reference",
			"Uber's All-In-One",
			"Curated hub and farm waypoints — copy a chat code, paste in-game, click to travel.",
			"<a href=\"#hubs\">Hubs</a>\n"
			"<a href=\"#vault\">Wizard’s Vault</a>\n"
			"<a href=\"#chak\">Chak Egg</a>\n"
			"<a href=\"#obsidian\">Obsidian</a>\n"
			"<a href=\"#provisioner\">Provisioner</a>",
			R"BODY(
  <section class="block">
    <div class="head">
      <h2>How to travel</h2>
      <p>Map links open from game chat — not from the browser.</p>
    </div>
    <div class="body">
      <div class="howto">
        <div class="step"><span class="n">Step 1</span><p><strong>Click a chat code</strong> on any card to copy it.</p></div>
        <div class="step"><span class="n">Step 2</span><p><strong>Paste into game chat</strong> and send (or leave it in the input).</p></div>
        <div class="step"><span class="n">Step 3</span><p><strong>Click the blue link</strong> to center your map on that waypoint.</p></div>
      </div>
    </div>
  </section>

  <section class="block" id="hubs">
    <div class="head">
      <h2>Hubs</h2>
      <p>Expansion and city hubs for quick travel across Tyria.</p>
    </div>
    <div class="body">
      <div class="wp-grid">
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Landmark</span></div>
          <h3 class="name">Arcane Council</h3>
          <p class="map">Rata Sum</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BJIEAAA=]" title="Copy chat code">[&amp;BJIEAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BJIEAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Landmark</span></div>
          <h3 class="name">Camp Resolve</h3>
          <p class="map">The Silverwastes</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BKcHAAA=]" title="Copy chat code">[&amp;BKcHAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BKcHAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Waypoint</span></div>
          <h3 class="name">Amnoon Waypoint</h3>
          <p class="map">Crystal Oasis</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BLsKAAA=]" title="Copy chat code">[&amp;BLsKAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BLsKAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Landmark</span></div>
          <h3 class="name">Mist Portals</h3>
          <p class="map">Lion’s Arch</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BB0EAAA=]" title="Copy chat code">[&amp;BB0EAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BB0EAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Waypoint</span></div>
          <h3 class="name">Arborstone Waypoint</h3>
          <p class="map">Arborstone</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BGMNAAA=]" title="Copy chat code">[&amp;BGMNAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BGMNAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Waypoint</span></div>
          <h3 class="name">Eye of the North Waypoint</h3>
          <p class="map">Eye of the North</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BAkMAAA=]" title="Copy chat code">[&amp;BAkMAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BAkMAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Waypoint</span></div>
          <h3 class="name">Base Camp Waypoint</h3>
          <p class="map">Drizzlewood Coast</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BGQMAAA=]" title="Copy chat code">[&amp;BGQMAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BGQMAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Waypoint</span></div>
          <h3 class="name">Bastion of the Celestial</h3>
          <p class="map">Amnytas</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BAoOAAA=]" title="Copy chat code">[&amp;BAoOAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BAoOAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Waypoint</span></div>
          <h3 class="name">Alliance Staging Ground</h3>
          <p class="map">Mistburned Barrens</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BFAPAAA=]" title="Copy chat code">[&amp;BFAPAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BFAPAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-hub">
          <div class="meta"><span class="kind">Waypoint</span></div>
          <h3 class="name">Pub Canach Waypoint</h3>
          <p class="map">Shipwreck Strand</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BJEPAAA=]" title="Copy chat code">[&amp;BJEPAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BJEPAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
      </div>
    </div>
  </section>

  <section class="block" id="vault">
    <div class="head">
      <h2>Wizard’s Vault</h2>
      <p>Gathering routes and a reliable kill objective.</p>
    </div>
    <div class="body">
      <div class="wp-grid">
        <article class="wp kind-vault">
          <div class="meta"><span class="kind">Landmark</span><span class="purpose">Axe · Sickle · Pick</span></div>
          <h3 class="name">Rayhan Bayt</h3>
          <p class="map">Malchor’s Leap</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BJ4CAAA=]" title="Copy chat code">[&amp;BJ4CAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BJ4CAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-vault">
          <div class="meta"><span class="kind">Waypoint</span><span class="purpose">Axe · Sickle · Pick</span></div>
          <h3 class="name">Beetletun Waypoint</h3>
          <p class="map">Queensdale</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BPoAAAA=]" title="Copy chat code">[&amp;BPoAAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BPoAAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-vault">
          <div class="meta"><span class="kind">Landmark</span><span class="purpose">Axe · Sickle · Pick</span></div>
          <h3 class="name">Rata Pten</h3>
          <p class="map">Mount Maelstrom</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BMQCAAA=]" title="Copy chat code">[&amp;BMQCAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BMQCAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-vault">
          <div class="meta"><span class="kind">Waypoint</span><span class="purpose">25 Enemies</span></div>
          <h3 class="name">Westwatch Waypoint</h3>
          <p class="map">Auric Basin</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BAYIAAA=]" title="Copy chat code">[&amp;BAYIAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BAYIAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
      </div>
    </div>
  </section>

  <section class="block" id="chak">
    <div class="head">
      <h2>Chak Egg Sac</h2>
      <p>Tangled Depths farm route.</p>
    </div>
    <div class="body">
      <div class="wp-grid single">
        <article class="wp kind-farm">
          <div class="meta"><span class="kind">Waypoint</span><span class="purpose farm">Chak Egg</span></div>
          <h3 class="name">Ley-Line Confluence Waypoint</h3>
          <p class="map">Tangled Depths</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BPUHAAA=]" title="Copy chat code">[&amp;BPUHAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BPUHAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
      </div>
    </div>
  </section>

  <section class="block" id="obsidian">
    <div class="head">
      <h2>Obsidian Shards</h2>
      <p>Orr farming hub in Straits of Devastation.</p>
    </div>
    <div class="body">
      <div class="wp-grid single">
        <article class="wp kind-farm">
          <div class="meta"><span class="kind">Waypoint</span><span class="purpose farm">Obsidian Shards</span></div>
          <h3 class="name">Glorious Victory Waypoint</h3>
          <p class="map">Straits of Devastation</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BPoCAAA=]" title="Copy chat code">[&amp;BPoCAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BPoCAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
      </div>
    </div>
  </section>

  <section class="block" id="provisioner">
    <div class="head">
      <h2>Provisioner Tokens</h2>
      <p>Heart of Thorns maps — common token routes.</p>
    </div>
    <div class="body">
      <div class="wp-grid">
        <article class="wp kind-token">
          <div class="meta"><span class="kind">Waypoint</span><span class="purpose token">Provisioner</span></div>
          <h3 class="name">Shipwreck Peak Waypoint</h3>
          <p class="map">Verdant Brink</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BN4HAAA=]" title="Copy chat code">[&amp;BN4HAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BN4HAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-token">
          <div class="meta"><span class="kind">Waypoint</span><span class="purpose token">Provisioner</span></div>
          <h3 class="name">Wanderer’s Waypoint</h3>
          <p class="map">Auric Basin</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BNYHAAA=]" title="Copy chat code">[&amp;BNYHAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BNYHAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
        <article class="wp kind-token">
          <div class="meta"><span class="kind">Waypoint</span><span class="purpose token">Provisioner</span></div>
          <h3 class="name">Ogre Camp Waypoint</h3>
          <p class="map">Tangled Depths</p>
          <div class="actions">
            <button type="button" class="code" data-copy="[&BMwHAAA=]" title="Copy chat code">[&amp;BMwHAAA=]</button>
            <span class="hint">Copy</span>
            <a class="wiki" href="https://wiki.guildwars2.com/index.php?search=%5B%26BMwHAAA%3D%5D" target="_blank" rel="noopener">Wiki</a>
          </div>
        </article>
      </div>
    </div>
  </section>

  <p class="credit">Waypoint list curated by <strong>uberduber.1249</strong></p>

<script>
(function(){
  function fallbackCopy(t){
    var a=document.createElement("textarea");
    a.value=t; a.setAttribute("readonly","");
    a.style.position="fixed"; a.style.left="-9999px";
    document.body.appendChild(a); a.select();
    try{ document.execCommand("copy"); }catch(e){}
    document.body.removeChild(a);
  }
  document.querySelectorAll(".code[data-copy]").forEach(function(el){
    el.addEventListener("click", function(){
      var t=el.getAttribute("data-copy")||"";
      var hint=el.parentNode && el.parentNode.querySelector(".hint");
      function done(){
        el.classList.add("copied");
        var prev=el.textContent;
        el.textContent="Copied";
        if(hint) hint.textContent="Done";
        setTimeout(function(){
          el.classList.remove("copied");
          el.textContent=prev;
          if(hint) hint.textContent="Copy";
        }, 1000);
      }
      if (navigator.clipboard && navigator.clipboard.writeText)
        navigator.clipboard.writeText(t).then(done).catch(function(){ fallbackCopy(t); done(); });
      else { fallbackCopy(t); done(); }
    });
  });
})();
</script>
)BODY");
	}



	std::string HtmlStrikeMissions()
	{
		return BuildHtml(
			"Strike Missions Overview",
			"Guild Wars 2 · Raids / Strikes Reference",
			"Strike Missions Overview",
			"Single-boss raid encounters by expansion — map → boss. No strategies.",
			"<a href=\"#ibs\">Icebrood</a>\n"
			"<a href=\"#eod\">End of Dragons</a>\n"
			"<a href=\"#soto\">SotO</a>\n"
			"<a href=\"#other\">Other</a>",
			R"BODY(
  <section class="block">
    <div class="head">
      <h2>How to open</h2>
      <p>These appear in the Raid Encounters UI alongside raid bosses.</p>
    </div>
    <div class="body">
      <div class="howto">
        <div class="step"><span class="n">IBS</span><p><strong>Eye of the North</strong> — Path of Fire required.</p></div>
        <div class="step"><span class="n">EoD</span><p><strong>Arborstone</strong> — Cantha story unlocks CMs.</p></div>
        <div class="step"><span class="n">SotO</span><p><strong>Wizard’s Tower</strong> — story gates apply.</p></div>
      </div>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Strategies:</strong> use TLDR / Snowcrows — this sheet is a map only.</p>
    </div>
  </section>

  <section class="block" id="ibs">
    <div class="head"><h2>Icebrood Saga</h2><p>Eye of the North</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th></th><th>Mission</th><th>Encounter(s)</th></tr></thead>
        <tbody>
          <tr><td class="num">1</td><td>Shiverpeaks Pass</td><td>Icebrood Construct</td></tr>
          <tr><td class="num">2</td><td>Voice of the Fallen and Claw of the Fallen</td><td>Voice · Claw</td></tr>
          <tr><td class="num">3</td><td>Fraenir of Jormag</td><td>Fraenir of Jormag</td></tr>
          <tr><td class="num">4</td><td>Boneskinner</td><td>Boneskinner</td></tr>
          <tr><td class="num">5</td><td>Whisper of Jormag</td><td>Whisper of Jormag</td></tr>
          <tr><td class="num">6</td><td>Forging Steel</td><td>Ancient Forgeman <span class="muted">(story unlock)</span></td></tr>
          <tr><td class="num">7</td><td>Cold War</td><td>Minister of Morale</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="eod">
    <div class="head"><h2>End of Dragons</h2><p>Arborstone · challenge modes available</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th></th><th>Mission</th><th>Encounter</th><th></th></tr></thead>
        <tbody>
          <tr><td class="num">1</td><td>Aetherblade Hideout</td><td>Mai Trin</td><td><span class="badge badge-cm">CM</span></td></tr>
          <tr><td class="num">2</td><td>Xunlai Jade Junkyard</td><td>Ankka</td><td><span class="badge badge-cm">CM</span></td></tr>
          <tr><td class="num">3</td><td>Kaineng Overlook</td><td>Minister Li</td><td><span class="badge badge-cm">CM</span></td></tr>
          <tr><td class="num">4</td><td>Harvest Temple</td><td>The Dragonvoid</td><td><span class="badge badge-cm">CM</span></td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="soto">
    <div class="head"><h2>Secrets of the Obscure</h2><p>Wizard’s Tower</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th></th><th>Mission</th><th>Encounter</th><th></th></tr></thead>
        <tbody>
          <tr><td class="num">1</td><td>Cosmic Observatory</td><td>Dagda</td><td><span class="badge badge-cm">CM</span></td></tr>
          <tr><td class="num">2</td><td>Temple of Febe</td><td>Cerus</td><td><span class="badge badge-cm">CM</span> <span class="muted">LM</span></td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="other">
    <div class="head"><h2>Other</h2><p>Living World / seasonal</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Mission</th><th>Access</th><th></th></tr></thead>
        <tbody>
          <tr><td>Old Lion’s Court</td><td>Lion’s Arch</td><td><span class="badge badge-cm">CM</span></td></tr>
          <tr><td>Secret Lair of the Snowmen</td><td>Seasonal / special</td><td><span class="badge">—</span></td></tr>
        </tbody>
      </table>
    </div>
  </section>
)BODY");
	}

	std::string HtmlFractalCmList()
	{
		return BuildHtml(
			"Fractal CM / T4 List",
			"Guild Wars 2 · Fractals Reference",
			"Fractal CM / T4 List",
			"Challenge mote scales, agony glance, and daily notes — pairs with Fractal Consumables.",
			"<a href=\"#cm\">Challenge Modes</a>\n"
			"<a href=\"#ar\">Agony</a>\n"
			"<a href=\"#daily\">Dailies</a>\n"
			"<a href=\"#tips\">Tips</a>",
			R"BODY(
  <section class="block" id="cm">
    <div class="head">
      <h2>Challenge Modes</h2>
      <p>Scales 95–100 — activate Harbinger of Woe / challenge mote after unlocking.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Scale</th><th>Fractal</th><th>~AR</th></tr></thead>
        <tbody>
          <tr><td><span class="badge badge-scale">95</span></td><td>Kinfall CM</td><td>~141+</td></tr>
          <tr><td><span class="badge badge-scale">96</span></td><td>Nightmare CM</td><td>~143+</td></tr>
          <tr><td><span class="badge badge-scale">97</span></td><td>Shattered Observatory CM</td><td>~145+</td></tr>
          <tr><td><span class="badge badge-scale">98</span></td><td>Sunqua Peak CM</td><td>~147+</td></tr>
          <tr><td><span class="badge badge-scale">99</span></td><td>Silent Surf CM</td><td>~148+</td></tr>
          <tr><td><span class="badge badge-scale">100</span></td><td>Lonely Tower CM</td><td>~150+</td></tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>LFG:</strong> “96 CM” means Nightmare challenge mote. Bring Mist Offensive + food — see Fractal Consumables.</p>
    </div>
  </section>

  <section class="block" id="ar">
    <div class="head"><h2>Agony Resistance</h2><p>Base agony by tier; CMs sit on T4.</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Tier</th><th>Scales</th><th>Typical AR</th></tr></thead>
        <tbody>
          <tr><td>T1</td><td>1–25</td><td>0</td></tr>
          <tr><td>T2</td><td>26–50</td><td>~10+</td></tr>
          <tr><td>T3</td><td>51–75</td><td>~20+</td></tr>
          <tr><td>T4</td><td>76–100</td><td>~40+ base · <strong>150+</strong> for CMs</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="daily">
    <div class="head"><h2>Daily / Recommended</h2><p>Rotates every day — check Mistlock / fractal UI.</p></div>
    <div class="body">
      <div class="callout-grid">
        <div class="callout"><h3>Daily T4</h3><p>Three T4 scales (not always CMs).</p></div>
        <div class="callout"><h3>Recommended</h3><p>One per tier — easier AR / reward path.</p></div>
        <div class="callout"><h3>CM farming</h3><p>LFG often clears 96–100 CM for essences / KP.</p></div>
        <div class="callout"><h3>Instabilities</h3><p>Change daily — don’t memorize them here.</p></div>
      </div>
    </div>
  </section>

  <section class="block" id="tips">
    <div class="head"><h2>Quick Tips</h2><p></p></div>
    <div class="body">
      <ul class="list">
        <li>Unlock each CM via that fractal’s achievement track before the Harbinger appears.</li>
        <li>Party of 5 — same consumable logic as raids (Raid Food / Raid Utilities / Fractal Consumables).</li>
        <li>Fight notes → Mukluk / TLDR Fractals in Guides.</li>
      </ul>
    </div>
  </section>
)BODY");
	}

	std::string HtmlSquadTemplate()
	{
		return BuildHtml(
			"Squad Template — 10-Man Roles",
			"Guild Wars 2 · Raid Squad Reference",
			"Squad Template",
			"Typical 10-man slots — tank, heal, quick, alac, DPS. Pairs with Boon Checklist.",
			"<a href=\"#classic\">Classic</a>\n"
			"<a href=\"#modern\">Modern</a>\n"
			"<a href=\"#strike\">Strikes</a>\n"
			"<a href=\"#checklist\">Checklist</a>",
			R"BODY(
  <section class="block" id="classic">
    <div class="head"><h2>Classic 10-Man</h2><p>Common in statics and training squads.</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Count</th><th>Role</th><th>Notes</th></tr></thead>
        <tbody>
          <tr><td class="num">1</td><td><span class="tag tag-tank">Tank</span></td><td>Highest toughness · holds boss facing</td></tr>
          <tr><td class="num">1–2</td><td><span class="tag tag-heal">Heal</span></td><td>Often heal-alac or heal-quick hybrid</td></tr>
          <tr><td class="num">1</td><td><span class="tag tag-sup">Quickness</span></td><td>Pure quick DPS or heal-quick</td></tr>
          <tr><td class="num">1</td><td><span class="tag tag-sup">Alacrity</span></td><td>Pure alac DPS or heal-alac</td></tr>
          <tr><td class="num">5–7</td><td><span class="tag tag-power">DPS</span></td><td>Power and/or condi — fill remaining slots</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="modern">
    <div class="head"><h2>Modern Layouts</h2><p>Name roles in squad before ready-check.</p></div>
    <div class="body">
      <div class="callout-grid">
        <div class="callout">
          <div class="tags"><span class="tag tag-heal">Heal Quic</span> <span class="tag tag-heal">Heal Alac</span></div>
          <h3>Double heal-boon</h3>
          <p>2 heal-boon supports · 8 DPS. Tank optional on easier strikes.</p>
        </div>
        <div class="callout">
          <div class="tags"><span class="tag tag-tank">Tank</span> <span class="tag tag-heal">2× Heal</span></div>
          <h3>Tank + heal boons</h3>
          <p>1 tank · heal-quick · heal-alac · 7 DPS.</p>
        </div>
        <div class="callout">
          <div class="tags"><span class="tag tag-power">Power</span> <span class="tag tag-condi">Condi</span></div>
          <h3>Damage mix</h3>
          <p>Match hitbox &amp; condi caps — ask the static.</p>
        </div>
        <div class="callout">
          <div class="tags"><span class="tag tag-misc">Special</span></div>
          <h3>Hand kite / push</h3>
          <p>Extra role on specific bosses — replace one DPS.</p>
        </div>
      </div>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Boons:</strong> assign quick + alac explicitly — see Boon Checklist.</p>
    </div>
  </section>

  <section class="block" id="strike">
    <div class="head"><h2>Strikes &amp; Smaller Groups</h2><p></p></div>
    <div class="body">
      <ul class="list">
        <li><strong>5-man</strong> — heal or heal-boon + quick/alac coverage + DPS.</li>
        <li><strong>10-man strikes</strong> — same roles as raids; many groups skip tank on easier bosses.</li>
        <li><strong>CM strikes</strong> — full boon coverage + named mechanics roles.</li>
      </ul>
    </div>
  </section>

  <section class="block" id="checklist">
    <div class="head"><h2>Pre-Pull Checklist</h2><p>Call these in chat.</p></div>
    <div class="body">
      <ul class="checks">
        <li><span class="box" aria-hidden="true"></span><span><span class="tag tag-tank">Tank</span> named (if needed) · toughness check</span></li>
        <li><span class="box" aria-hidden="true"></span><span><span class="tag tag-sup">Quick</span> + <span class="tag tag-sup">Alac</span> named</span></li>
        <li><span class="box" aria-hidden="true"></span><span><span class="tag tag-heal">Heal</span> coverage confirmed</span></li>
        <li><span class="box" aria-hidden="true"></span><span>Food + utilities up (Raid Food / Raid Utilities)</span></li>
        <li><span class="box" aria-hidden="true"></span><span>Special roles (kite, push, reflect) assigned</span></li>
      </ul>
    </div>
  </section>
)BODY");
	}

	std::string HtmlStabilityCleanse()
	{
		return BuildHtml(
			"Stability / Cleanse — Squad Utility",
			"Guild Wars 2 · Combat Reference",
			"Stability / Cleanse",
			"Group stability and condition cleanse by profession — complements CC / Defiance.",
			"<a href=\"#stability\">Stability</a>\n"
			"<a href=\"#cleanse\">Cleanse</a>\n"
			"<a href=\"#tips\">Tips</a>",
			R"BODY(
  <section class="block" id="stability">
    <div class="head">
      <h2>Stability</h2>
      <p>Prefer group stab for knockbacks, pulls, and fears. Call “stab now.”</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Profession</th><th>Typical tools</th></tr></thead>
        <tbody>
          <tr><td class="role">Guardian / Firebrand</td><td>“Stand Your Ground!” · Mantras / Tomes · Hallowed Ground</td></tr>
          <tr><td class="role">Warrior</td><td>Balanced Stance · Banner of Defense</td></tr>
          <tr><td class="role">Revenant</td><td>Rite of the Great Dwarf · centaur / renegade utilities</td></tr>
          <tr><td class="role">Elementalist</td><td>Armor of Earth · aura / magnetic lines</td></tr>
          <tr><td class="role">Engineer</td><td>Elixir S · kit utilities</td></tr>
          <tr><td class="role">Mesmer</td><td>Continuum setups · wells (build-dependent)</td></tr>
          <tr><td class="role">Others</td><td>Often personal only — don’t assume group stab</td></tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Raid habit:</strong> Firebrand, chrono, or warrior usually holds the stab button for key mechanics.</p>
    </div>
  </section>

  <section class="block" id="cleanse">
    <div class="head">
      <h2>Condition Cleanse</h2>
      <p>Group cleanses for heavy condi phases.</p>
    </div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Profession</th><th>Typical tools</th></tr></thead>
        <tbody>
          <tr><td class="role">Guardian / Firebrand</td><td>Tome of Resolve · Contemplation of Purity</td></tr>
          <tr><td class="role">Elementalist / Tempest</td><td>Cleansing Fire · Signet of Water · Glyphs</td></tr>
          <tr><td class="role">Engineer</td><td>Elixir C · Med Kit</td></tr>
          <tr><td class="role">Necromancer / Scourge</td><td>Plague Signet · Consume Conditions · shades</td></tr>
          <tr><td class="role">Ranger / Druid</td><td>Spirits · staff &amp; glyph cleanses</td></tr>
          <tr><td class="role">Mesmer / Chrono</td><td>Well of Eternity · Mantra of Recovery · Null Field</td></tr>
          <tr><td class="role">Revenant / Herald</td><td>Facet / legend utilities</td></tr>
          <tr><td class="role">Thief / Specter</td><td>Shadow / shroud lines (often small group)</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="tips">
    <div class="head"><h2>Squad Tips</h2><p></p></div>
    <div class="body">
      <div class="callout-grid">
        <div class="callout"><h3>Resistance ≠ cleanse</h3><p>Resistance lowers condi damage taken; it does not remove conditions.</p></div>
        <div class="callout"><h3>Convert vs purge</h3><p>Some scourge / FB tools convert conditions instead of cleansing.</p></div>
        <div class="callout"><h3>Stack first</h3><p>Stab/cleanse on a spread squad wastes the button.</p></div>
        <div class="callout"><h3>Pair sheets</h3><p>Use with CC / Defiance and Boon Checklist.</p></div>
      </div>
    </div>
  </section>
)BODY");
	}

	std::string HtmlMaterialConversions()
	{
		return BuildHtml(
			"Material Conversions",
			"Guild Wars 2 · Economy Reference",
			"Material Conversions",
			"Mystic forge staples and common sinks — structural glance, not TP prices.",
			"<a href=\"#forge\">Mystic Forge</a>\n"
			"<a href=\"#ecto\">Ecto</a>\n"
			"<a href=\"#blood\">Bloodstone</a>\n"
			"<a href=\"#sinks\">Sinks</a>",
			R"BODY(
  <section class="block" id="forge">
    <div class="head"><h2>Mystic Forge Staples</h2><p>Four-slot recipes players use constantly.</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Output</th><th>Typical inputs</th></tr></thead>
        <tbody>
          <tr><td><strong>Mystic Clover</strong></td><td>Mystic Coin · Ectoplasm · Philosopher’s Stone · Elonian Wine</td></tr>
          <tr><td><strong>Gift of Condensed Magic / Might</strong></td><td>Stacks of T6 mats (blood, fang, claw, dust…)</td></tr>
          <tr><td><strong>Tier promotion</strong></td><td>4× same-tier crafting mats → higher tier (chance)</td></tr>
          <tr><td><strong>Ascended boxes</strong></td><td>Bloodstone Dust · Dragonite · Empyreal (+ catalyst)</td></tr>
          <tr><td><strong>Legendary gifts</strong></td><td>Follow the specific legendary collection page</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="ecto">
    <div class="head"><h2>Ectoplasm &amp; Salvage</h2><p></p></div>
    <div class="body">
      <div class="callout-grid">
        <div class="callout"><h3>Globs of Ectoplasm</h3><p>Salvage rare+ gear — fuel clovers, gifts, and forge gambles.</p></div>
        <div class="callout"><h3>Philosopher’s Stones</h3><p>Buy from mystic vendors with spirit shards / coins.</p></div>
        <div class="callout"><h3>Junk</h3><p>Vendor junk — never forge it.</p></div>
        <div class="callout"><h3>Prices</h3><p>Use BLTC / Trading Post — this sheet has no prices.</p></div>
      </div>
    </div>
  </section>

  <section class="block" id="blood">
    <div class="head"><h2>Bloodstone · Empyreal · Dragonite</h2><p>Ascended crafting triad.</p></div>
    <div class="body">
      <ul class="list">
        <li>Used together for <strong>ascended gear</strong> and many legendary gifts.</li>
        <li>Convert excess via mystic forge boxes when storage is full.</li>
        <li>Map completion, fractals, raids, and dailies drip these constantly.</li>
      </ul>
    </div>
  </section>

  <section class="block" id="sinks">
    <div class="head"><h2>Useful Sinks</h2><p>When the bank hurts.</p></div>
    <div class="body">
      <ul class="list">
        <li><strong>Trophy shipments</strong> — laurel / karma vendors for account-bound mats.</li>
        <li><strong>Material storage</strong> — deposit before forging.</li>
        <li><strong>Legendary collections</strong> — intentional sinks (see Legendary Paths).</li>
      </ul>
    </div>
  </section>
)BODY");
	}

	std::string HtmlLegendaryPaths()
	{
		return BuildHtml(
			"Legendary Short Paths",
			"Guild Wars 2 · Legendaries Reference",
			"Legendary Short Paths",
			"Checklists for gen weapons, armor, and backpacks — not full guides.",
			"<a href=\"#gen1\">Gen 1</a>\n"
			"<a href=\"#gen2\">Gen 2–3</a>\n"
			"<a href=\"#armor\">Armor</a>\n"
			"<a href=\"#back\">Backpacks</a>",
			R"BODY(
  <section class="block" id="gen1">
    <div class="head"><h2>Gen 1 Weapons</h2><p>Precursor + four gifts in the mystic forge.</p></div>
    <div class="body">
      <ul class="checks">
        <li><span class="box"></span><span><strong>Precursor</strong> — drop, craft, or buy</span></li>
        <li><span class="box"></span><span><strong>Gift of Fortune</strong> — clovers · ecto · mystic coins · destiny gifts</span></li>
        <li><span class="box"></span><span><strong>Gift of Mastery</strong> — world completion · shards · obsidians · battle / exploration gifts</span></li>
        <li><span class="box"></span><span><strong>Gift of (Weapon)</strong> — themed gift from collection + mats</span></li>
        <li><span class="box"></span><span><strong>Forge</strong> — precursor + Fortune + Mastery + Weapon gift</span></li>
      </ul>
    </div>
  </section>

  <section class="block" id="gen2">
    <div class="head"><h2>Gen 2 &amp; Gen 3</h2><p>Collection-driven — track in Achievements.</p></div>
    <div class="body">
      <div class="callout-grid">
        <div class="callout"><h3>Gen 2</h3><p>Long collection (metas, raids/fractals, crafting) → legendary.</p></div>
        <div class="callout"><h3>Gen 3</h3><p>Expansion collections with gift crafts (EoD / SotO era).</p></div>
        <div class="callout"><h3>Open the collection</h3><p>It lists every gate — pin it in the hero panel.</p></div>
        <div class="callout"><h3>Trackers</h3><p>Efficiency / Treasures help; this sheet is only the shape.</p></div>
      </div>
    </div>
  </section>

  <section class="block" id="armor">
    <div class="head"><h2>Legendary Armor</h2><p>Raid / PvP / WvW / fractal lines.</p></div>
    <div class="body">
      <table class="sheet">
        <thead><tr><th>Line</th><th>Core idea</th></tr></thead>
        <tbody>
          <tr><td><strong>Raid</strong></td><td>Raid tokens · Gift of Ascension · ascended pieces · forge</td></tr>
          <tr><td><strong>PvP / WvW</strong></td><td>Reward tracks · legendary inscriptions / gifts · ascended set</td></tr>
          <tr><td><strong>Fractal</strong></td><td>Mist / fractal currencies · ascended · gifts</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="block" id="back">
    <div class="head"><h2>Legendary Backpacks</h2><p>Pick one path and finish it.</p></div>
    <div class="body">
      <ul class="list">
        <li><strong>Ad Infinitum</strong> — fractal agony / pristine / collections</li>
        <li><strong>Expansion backs</strong> — map collections + gifts (HoT / PoF / EoD…)</li>
        <li><strong>Raid / strike backs</strong> — encounter currencies + collections</li>
      </ul>
      <p class="note" style="margin-top:14px;margin-bottom:0">Exact counts live on the wiki collection — balance patches rarely change the checklist shape.</p>
    </div>
  </section>
)BODY");
	}

	std::string HtmlMountUnlock()
	{
		return BuildHtml(
			"Mount Unlock Checklist",
			"Guild Wars 2 · Mounts Reference",
			"Mount Unlock Checklist",
			"Griffon, Skyscale, and Siege Turtle — offline tick-lists. Full steps on MetaBattle.",
			"<a href=\"#griffon\">Griffon</a>\n"
			"<a href=\"#skyscale\">Skyscale</a>\n"
			"<a href=\"#turtle\">Siege Turtle</a>\n"
			"<a href=\"#others\">Others</a>",
			R"BODY(
  <section class="block" id="griffon">
    <div class="head"><h2>Griffon</h2><p>Path of Fire</p></div>
    <div class="body">
      <ul class="checks">
        <li><span class="box"></span><span>Own <strong>Path of Fire</strong></span></li>
        <li><span class="box"></span><span>Train basic mounts required by the collection</span></li>
        <li><span class="box"></span><span>Complete the <strong>Open Skies</strong> / griffon collection</span></li>
        <li><span class="box"></span><span>Gather listed powders / materials</span></li>
        <li><span class="box"></span><span>Finish roost / forge step on the achievement</span></li>
      </ul>
      <p class="note" style="margin-top:14px;margin-bottom:0">Detail → Guides · Griffon Unlock (MetaBattle).</p>
    </div>
  </section>

  <section class="block" id="skyscale">
    <div class="head"><h2>Skyscale</h2><p>Living World + End of Dragons era</p></div>
    <div class="body">
      <ul class="checks">
        <li><span class="box"></span><span>Required Living World / IBS episodes owned</span></li>
        <li><span class="box"></span><span>Complete <strong>Skyscale Growing / Saving</strong> story steps</span></li>
        <li><span class="box"></span><span>Finish the <strong>Skyscale collection</strong></span></li>
        <li><span class="box"></span><span>Pay / craft materials at each gate</span></li>
        <li><span class="box"></span><span>Optional: mastery tracks for holds / dashes</span></li>
      </ul>
      <p class="note" style="margin-top:14px;margin-bottom:0">Detail → Guides · Skyscale Unlock (MetaBattle).</p>
    </div>
  </section>

  <section class="block" id="turtle">
    <div class="head"><h2>Siege Turtle</h2><p>End of Dragons</p></div>
    <div class="body">
      <ul class="checks">
        <li><span class="box"></span><span>Own <strong>End of Dragons</strong></span></li>
        <li><span class="box"></span><span>Progress Cantha story / turtle gates</span></li>
        <li><span class="box"></span><span>Complete <strong>Turtle Unlock</strong> collection</span></li>
        <li><span class="box"></span><span>Unlock passenger / siege skills via masteries</span></li>
      </ul>
    </div>
  </section>

  <section class="block" id="others">
    <div class="head"><h2>Other Mounts</h2><p>Usually shorter unlocks.</p></div>
    <div class="body">
      <div class="callout-grid">
        <div class="callout"><h3>Core PoF</h3><p>Raptor · Springer · Skimmer · Jackal — story / map unlocks.</p></div>
        <div class="callout"><h3>Roller Beetle</h3><p>PoF racing collection.</p></div>
        <div class="callout"><h3>Warclaw</h3><p>WvW reward track.</p></div>
        <div class="callout"><h3>Skiff</h3><p>EoD fishing masteries (travel, not a mount).</p></div>
      </div>
    </div>
  </section>
)BODY");
	}

	struct PageSpec
	{
		CheatSheets::Sheet meta;
		std::string (*build)();
	};

	const PageSpec* Pages(size_t* outCount)
	{
		static const PageSpec kPages[] = {
			{{"ubersaio", "about:ubers-aio", "ubers-all-in-one", "3",
			  "Uber's All-In-One", "Uber's All-In-One — Waypoints"},
			 HtmlUbersAllInOne},
			{{"raidutils", "about:raid-utilities", "raid-utilities", "1",
			  "Raid Utilities", "Raid Utilities — Oils, Stones & Crystals"},
			 HtmlRaidUtilities},
			{{"fractalcons", "about:fractal-consumables", "fractal-consumables", "1",
			  "Fractal Consumables", "Fractal Consumables — Potions & Agony"},
			 HtmlFractalConsumables},
			{{"fractalcm", "about:fractal-cm", "fractal-cm-list", "2",
			  "Fractal CM / T4", "Fractal CM / T4 List"},
			 HtmlFractalCmList},
			{{"sigilsrunes", "about:sigils-runes", "sigils-runes", "1",
			  "Sigils & Runes", "Sigils & Runes — Common Role Picks"},
			 HtmlSigilsRunes},
			{{"relics", "about:relics", "relics-guide", "1",
			  "Relics", "Relics — Picks by Role"},
			 HtmlRelics},
			{{"booncheck", "about:boon-checklist", "boon-checklist", "1",
			  "Boon Checklist", "Boon Checklist — Squad Coverage"},
			 HtmlBoonChecklist},
			{{"squadtmpl", "about:squad-template", "squad-template", "2",
			  "Squad Template", "Squad Template — 10-Man Roles"},
			 HtmlSquadTemplate},
			{{"stabcleanse", "about:stability-cleanse", "stability-cleanse", "2",
			  "Stability / Cleanse", "Stability / Cleanse — Squad Utility"},
			 HtmlStabilityCleanse},
			{{"ccdefiance", "about:cc-defiance", "cc-defiance", "1",
			  "CC / Defiance", "CC / Defiance — Breakbar by Profession"},
			 HtmlCcDefiance},
			{{"raidwings", "about:raid-wings", "raid-wings", "1",
			  "Raid Wings", "Raid Wings Overview"},
			 HtmlRaidWings},
			{{"strikes", "about:strike-missions", "strike-missions", "2",
			  "Strike Missions", "Strike Missions Overview"},
			 HtmlStrikeMissions},
			{{"matconv", "about:material-conversions", "material-conversions", "2",
			  "Material Conversions", "Material Conversions"},
			 HtmlMaterialConversions},
			{{"legpaths", "about:legendary-paths", "legendary-paths", "2",
			  "Legendary Paths", "Legendary Short Paths"},
			 HtmlLegendaryPaths},
			{{"mounts", "about:mount-unlock", "mount-unlock", "2",
			  "Mount Unlock", "Mount Unlock Checklist"},
			 HtmlMountUnlock},
			{{"homegarden", "about:home-garden", "home-garden", "1",
			  "Home Garden", "Home Garden — Cultivated Herbs"},
			 HtmlHomeGarden},
		};
		if (outCount)
			*outCount = sizeof(kPages) / sizeof(kPages[0]);
		return kPages;
	}
}

const CheatSheets::Sheet* CheatSheets::All(size_t* outCount)
{
	size_t n = 0;
	const PageSpec* pages = Pages(&n);
	/* Expose meta only — caller iterates All via Find or we return parallel. */
	static Sheet kMeta[32];
	static bool init = false;
	if (!init)
	{
		for (size_t i = 0; i < n && i < 32; ++i)
			kMeta[i] = pages[i].meta;
		init = true;
	}
	if (outCount)
		*outCount = n;
	return kMeta;
}

const CheatSheets::Sheet* CheatSheets::FindByAbout(const char* aboutUrl)
{
	if (!aboutUrl || !aboutUrl[0])
		return nullptr;
	size_t n = 0;
	const PageSpec* pages = Pages(&n);
	for (size_t i = 0; i < n; ++i)
	{
		if (std::strcmp(pages[i].meta.about, aboutUrl) == 0)
			return &All(nullptr)[i];
	}
	return nullptr;
}

std::string CheatSheets::EnsureFileUrl(const std::wstring& addonDir, const Sheet& sheet)
{
	if (addonDir.empty() || !sheet.fileStem || !sheet.version)
		return {};

	size_t n = 0;
	const PageSpec* pages = Pages(&n);
	const PageSpec* spec = nullptr;
	for (size_t i = 0; i < n; ++i)
	{
		if (std::strcmp(pages[i].meta.fileStem, sheet.fileStem) == 0)
		{
			spec = &pages[i];
			break;
		}
	}
	if (!spec || !spec->build)
		return {};

	const std::wstring path = addonDir + L"\\" + std::wstring(sheet.fileStem, sheet.fileStem + std::strlen(sheet.fileStem)) + L".html";
	const std::wstring verPath = addonDir + L"\\" + std::wstring(sheet.fileStem, sheet.fileStem + std::strlen(sheet.fileStem)) + L".ver";

	char buf[64] = {};
	HANDLE vin = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (vin != INVALID_HANDLE_VALUE)
	{
		DWORD read = 0;
		if (ReadFile(vin, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
		{
			buf[read] = '\0';
			if (std::strcmp(buf, sheet.version) == 0)
			{
				CloseHandle(vin);
				HANDLE probe = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (probe != INVALID_HANDLE_VALUE)
				{
					CloseHandle(probe);
					return PathToFileUrl(path);
				}
			}
		}
		CloseHandle(vin);
	}

	const std::string html = spec->build();
	const DWORD len = static_cast<DWORD>(html.size());
	HANDLE hout = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hout == INVALID_HANDLE_VALUE)
		return {};
	DWORD written = 0;
	const BOOL ok = WriteFile(hout, html.data(), len, &written, nullptr);
	CloseHandle(hout);
	if (!ok || written != len)
		return {};

	HANDLE vout = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (vout != INVALID_HANDLE_VALUE)
	{
		DWORD vw = 0;
		WriteFile(vout, sheet.version, static_cast<DWORD>(std::strlen(sheet.version)), &vw, nullptr);
		CloseHandle(vout);
	}

	return PathToFileUrl(path);
}

std::string CheatSheets::ResolveAboutUrl(const std::wstring& addonDir, const std::string& url)
{
	const Sheet* sheet = FindByAbout(url.c_str());
	if (!sheet)
		return {};
	return EnsureFileUrl(addonDir, *sheet);
}
