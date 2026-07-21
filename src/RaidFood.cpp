#include "RaidFood.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	static constexpr const char* kRaidFoodVersion = "2";

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
}

const char* RaidFood::Html()
{
	return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Raid Food — Universal Seasoning Guide</title>
<style>
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
    --life: #c45c4a;
    --mitigate: #5a8fbf;
    --heal: #6aaa6a;
    --condi: #9b7bb8;
    --regen: #7a9e6e;
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
  .wrap {
    max-width: 860px;
    margin: 0 auto;
    padding: 28px 22px 64px;
  }
  .hero {
    margin-bottom: 22px;
    padding-bottom: 18px;
    border-bottom: 1px solid var(--border-soft);
  }
  .eyebrow {
    margin: 0 0 8px;
    font-family: "Segoe UI", Tahoma, sans-serif;
    font-size: 0.78rem;
    letter-spacing: 0.14em;
    text-transform: uppercase;
    color: var(--gold-dim);
  }
  h1 {
    margin: 0 0 8px;
    font-size: 2rem;
    font-weight: 700;
    color: var(--gold);
    letter-spacing: 0.01em;
  }
  .tagline {
    margin: 0;
    font-family: "Segoe UI", Tahoma, sans-serif;
    color: var(--muted);
    font-size: 1.02rem;
  }
  nav.toc {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    margin: 0 0 22px;
    font-family: "Segoe UI", Tahoma, sans-serif;
  }
  nav.toc a {
    color: var(--gold);
    text-decoration: none;
    font-size: 0.86rem;
    padding: 6px 10px;
    border: 1px solid var(--border);
    background: var(--accent);
  }
  nav.toc a:hover { background: #2c2416; }
  section.block {
    background: var(--panel);
    border: 1px solid var(--border);
    margin-bottom: 18px;
  }
  section.block > .head {
    padding: 14px 18px 14px 16px;
    border-bottom: 1px solid var(--border-soft);
    border-left: 3px solid var(--gold);
    background: linear-gradient(90deg, #1a1710 0%, var(--panel-2) 70%);
  }
  section.block > .head h2 {
    margin: 0;
    font-size: 1.2rem;
    color: var(--gold-bright);
    font-weight: 700;
  }
  section.block > .head p {
    margin: 6px 0 0;
    font-family: "Segoe UI", Tahoma, sans-serif;
    font-size: 0.92rem;
    color: var(--muted);
  }
  .body { padding: 16px 18px 18px; font-family: "Segoe UI", Tahoma, sans-serif; }
  .note {
    margin: 0 0 14px;
    padding: 12px 14px;
    background: var(--accent);
    border-left: 3px solid var(--gold-dim);
    color: var(--muted);
    font-size: 0.92rem;
  }
  .note strong { color: var(--text); }
  table.season {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.92rem;
  }
  table.season th,
  table.season td {
    text-align: left;
    padding: 10px 12px;
    border: 1px solid var(--border);
    vertical-align: top;
  }
  table.season th {
    background: #1a1710;
    color: var(--gold);
    font-weight: 600;
    width: 42%;
  }
  table.season td { background: var(--panel-2); }
  .util {
    display: inline-block;
    padding: 2px 8px;
    border-radius: 2px;
    font-size: 0.78rem;
    font-weight: 650;
    letter-spacing: 0.02em;
    margin-right: 4px;
    white-space: nowrap;
  }
  .util-life { background: rgba(196, 92, 74, 0.22); color: #e8a090; border: 1px solid rgba(196, 92, 74, 0.45); }
  .util-mit  { background: rgba(90, 143, 191, 0.2); color: #9bc0e0; border: 1px solid rgba(90, 143, 191, 0.45); }
  .util-heal { background: rgba(106, 170, 106, 0.2); color: #a8d0a8; border: 1px solid rgba(106, 170, 106, 0.45); }
  .util-condi { background: rgba(155, 123, 184, 0.2); color: #c4aee0; border: 1px solid rgba(155, 123, 184, 0.45); }
  .util-regen { background: rgba(122, 158, 110, 0.2); color: #b5d0aa; border: 1px solid rgba(122, 158, 110, 0.45); }
  .util-stats { background: rgba(235, 192, 71, 0.12); color: var(--gold); border: 1px solid var(--border); }
  .base {
    margin: 0 0 14px;
    padding: 10px 12px;
    background: #16130c;
    border: 1px dashed var(--border);
    font-size: 0.9rem;
    color: var(--muted);
  }
  .base strong { color: var(--gold-dim); }
  .recipe {
    margin: 0 0 12px;
    padding: 14px 14px 12px;
    background: var(--panel-2);
    border: 1px solid var(--border-soft);
  }
  .recipe:last-child { margin-bottom: 0; }
  .recipe h3 {
    margin: 0 0 8px;
    font-family: Georgia, "Palatino Linotype", Palatino, serif;
    font-size: 1.05rem;
    font-weight: 700;
    color: var(--text);
  }
  .recipe .tags { margin: 0 0 10px; }
  .recipe .ings {
    margin: 0;
    padding-left: 1.15em;
    color: var(--muted);
    font-size: 0.9rem;
  }
  .recipe .ings li { margin: 3px 0; }
  .recipe .ings .qty { color: var(--gold); font-weight: 600; }
  .subnote {
    margin: 8px 0 0;
    font-size: 0.84rem;
    color: var(--muted);
    font-style: italic;
  }
  footer {
    margin-top: 10px;
    font-family: "Segoe UI", Tahoma, sans-serif;
    color: var(--muted);
    font-size: 0.85rem;
  }
  @media (max-width: 560px) {
    h1 { font-size: 1.55rem; }
    table.season th { width: auto; }
  }
</style>
</head>
<body>
<div class="wrap">
  <header class="hero">
    <p class="eyebrow">Guild Wars 2 · Cooking Reference</p>
    <h1>Raid Food</h1>
    <p class="tagline">Universal Seasoning Guide — feast utilities, bases, and full recipes for raid nights.</p>
  </header>

  <nav class="toc" aria-label="Sections">
    <a href="#seasoning">Seasoning</a>
    <a href="#power">Power</a>
    <a href="#condition">Condition</a>
    <a href="#support">Support</a>
    <a href="#hybrid">Hybrid &amp; Tank</a>
  </nav>

  <section class="block" id="seasoning">
    <div class="head">
      <h2>Universal Seasoning Guide</h2>
      <p>The third ingredient dictates the feast’s utility.</p>
    </div>
    <div class="body">
      <table class="season">
        <thead>
          <tr><th>Utility</th><th>Seasoning</th></tr>
        </thead>
        <tbody>
          <tr>
            <td><span class="util util-life">Life Steal on Crit (66%)</span></td>
            <td>Cultivated Cilantro Leaf <span class="muted">(or Lime for specific variations)</span></td>
          </tr>
          <tr>
            <td><span class="util util-mit">−10% Incoming Damage</span></td>
            <td>Cultivated Peppercorn <span class="muted">(or Pile of Divinity Fair Herbs)</span></td>
          </tr>
          <tr>
            <td><span class="util util-heal">+10% Outgoing Healing</span></td>
            <td>Cultivated Mint Leaf</td>
          </tr>
          <tr>
            <td><span class="util util-condi">−20% Incoming Condition Duration</span></td>
            <td>Cultivated Clove</td>
          </tr>
          <tr>
            <td><span class="util util-regen">Health Regeneration</span></td>
            <td>Cultivated Sesame Seed</td>
          </tr>
        </tbody>
      </table>
      <p class="note" style="margin-top:14px;margin-bottom:0"><strong>Note:</strong> Cultivated ingredients are grown in your Home Instance garden.</p>
    </div>
  </section>

  <section class="block" id="power">
    <div class="head">
      <h2>1. Power Feasts</h2>
      <p>Direct damage feasts built on sous-vide steak or coq au vin.</p>
    </div>
    <div class="body">
      <p class="base"><strong>Base:</strong> Filet of Sous-Vide Meat — craft with 1 Cut of Quality Red Meat + 1 Jug of Water.</p>

      <article class="recipe">
        <h3>Cilantro Lime Sous-Vide Steak</h3>
        <div class="tags"><span class="util util-life">Life Steal</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Filet of Sous-Vide Meat</li>
          <li><span class="qty">1</span> Cultivated Cilantro Leaf</li>
          <li><span class="qty">1</span> Lime</li>
          <li><span class="qty">1</span> Packet of Salt</li>
        </ul>
      </article>

      <article class="recipe">
        <h3>Peppercorn-Crusted Sous-Vide Steak</h3>
        <div class="tags"><span class="util util-mit">−10% Damage</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Filet of Sous-Vide Meat</li>
          <li><span class="qty">1</span> Cultivated Peppercorn</li>
          <li><span class="qty">1</span> Pile of Divinity Fair Herbs</li>
        </ul>
      </article>

      <article class="recipe">
        <h3>Mushroom Clove Sous-Vide Steak</h3>
        <div class="tags"><span class="util util-condi">−20% Condi Duration</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Filet of Sous-Vide Meat</li>
          <li><span class="qty">1</span> Cultivated Clove</li>
          <li><span class="qty">1</span> Pile of Divinity Fair Herbs</li>
          <li><span class="qty">2</span> Sawgill Mushroom</li>
        </ul>
      </article>

      <p class="base"><strong>Base:</strong> Slab of Poultry Meat + Bottle of Vintage Wine.</p>

      <article class="recipe">
        <h3>Plate of Coq Au Vin with Salsa</h3>
        <div class="tags"><span class="util util-life">Life Steal</span></div>
        <ul class="ings">
          <li><span class="qty">2</span> Slab of Poultry Meat</li>
          <li><span class="qty">1</span> Bottle of Vintage Wine</li>
          <li><span class="qty">1</span> Jar of Salsa Garnish</li>
        </ul>
      </article>

      <article class="recipe">
        <h3>Plate of Peppercorn-Spiced Coq Au Vin</h3>
        <div class="tags"><span class="util util-mit">−10% Damage</span></div>
        <ul class="ings">
          <li><span class="qty">2</span> Slab of Poultry Meat</li>
          <li><span class="qty">1</span> Bottle of Vintage Wine</li>
          <li><span class="qty">1</span> Cultivated Peppercorn</li>
          <li><span class="qty">1</span> Head of Garlic</li>
        </ul>
      </article>
    </div>
  </section>

  <section class="block" id="condition">
    <div class="head">
      <h2>2. Condition Feasts</h2>
      <p>Condition damage feasts built on Flatbread.</p>
    </div>
    <div class="body">
      <p class="base"><strong>Base:</strong> Flatbread — craft with 1 Bag of Cassava Flour + 3 Packet of Salt + 1 Jug of Water + 1 Packet of Yeast.</p>

      <article class="recipe">
        <h3>Cilantro and Cured Meat Flatbread</h3>
        <div class="tags"><span class="util util-life">Life Steal</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Flatbread</li>
          <li><span class="qty">2</span> Piece of Cured Meat</li>
          <li><span class="qty">1</span> Cultivated Cilantro Leaf</li>
          <li><span class="qty">1</span> Lime</li>
        </ul>
        <p class="subnote">Piece of Cured Meat is crafted from Cut of Quality Red Meat.</p>
      </article>

      <article class="recipe">
        <h3>Peppered Cured Meat Flatbread</h3>
        <div class="tags"><span class="util util-mit">−10% Damage</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Flatbread</li>
          <li><span class="qty">2</span> Piece of Cured Meat</li>
          <li><span class="qty">1</span> Cultivated Peppercorn</li>
        </ul>
      </article>

      <article class="recipe">
        <h3>Salsa-Topped Veggie Flatbread</h3>
        <div class="tags"><span class="util util-life">Life Steal</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Flatbread</li>
          <li><span class="qty">1</span> Butternut Squash</li>
          <li><span class="qty">1</span> Jar of Salsa Garnish</li>
        </ul>
      </article>

      <article class="recipe">
        <h3>Peppercorn and Veggie Flatbread</h3>
        <div class="tags"><span class="util util-mit">−10% Damage</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Flatbread</li>
          <li><span class="qty">1</span> Butternut Squash</li>
          <li><span class="qty">1</span> Cultivated Peppercorn</li>
        </ul>
      </article>
    </div>
  </section>

  <section class="block" id="support">
    <div class="head">
      <h2>3. Support Feasts</h2>
      <p>Boon duration and healing-focused plates, plus crème brûlée dessert variants.</p>
    </div>
    <div class="body">
      <p class="base"><strong>Base:</strong> Cut of Quality Red Meat or Plate of Eggs Benedict.</p>

      <article class="recipe">
        <h3>Plate of Beef Carpaccio with Salsa Garnish</h3>
        <div class="tags"><span class="util util-life">Life Steal</span></div>
        <ul class="ings">
          <li><span class="qty">2</span> Cut of Quality Red Meat</li>
          <li><span class="qty">1</span> Jar of Salsa Garnish</li>
          <li><span class="qty">1</span> Cultivated Sesame Seed <span class="muted">(or specific sauce)</span></li>
        </ul>
      </article>

      <article class="recipe">
        <h3>Salsa Eggs Benedict</h3>
        <div class="tags"><span class="util util-life">Life Steal</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Plate of Eggs Benedict</li>
          <li><span class="qty">1</span> Jar of Salsa Garnish</li>
        </ul>
        <p class="subnote">Eggs Benedict = 1 Egg + 1 Jar of Hollandaise Sauce + 1 Flatbread + 1 Piece of Cured Meat.</p>
      </article>

      <article class="recipe">
        <h3>Peppercorn-Spiced Eggs Benedict</h3>
        <div class="tags"><span class="util util-mit">−10% Damage</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Plate of Eggs Benedict</li>
          <li><span class="qty">1</span> Cultivated Peppercorn</li>
        </ul>
      </article>

      <p class="base"><strong>Base:</strong> Bowl of Ice Cream Base.</p>

      <article class="recipe">
        <h3>Crème Brûlée Variations</h3>
        <div class="tags">
          <span class="util util-mit">Peppercorn → −10% Damage</span>
          <span class="util util-heal">Mint → +Healing</span>
        </div>
        <ul class="ings">
          <li><span class="qty">2</span> Bowl of Ice Cream Base</li>
          <li><span class="qty">1</span> Bowl of Baker’s Dry Ingredients</li>
          <li><span class="qty">1</span> Cultivated Herb (Peppercorn or Mint)</li>
        </ul>
      </article>
    </div>
  </section>

  <section class="block" id="hybrid">
    <div class="head">
      <h2>4. Hybrid &amp; Tank Feasts</h2>
      <p>All-stats soups and ravioli for hybrid and tank roles.</p>
    </div>
    <div class="body">
      <p class="base"><strong>Base:</strong> Oyster + Glob of Gelatin.</p>

      <article class="recipe">
        <h3>Spherified Cilantro Oyster Soup</h3>
        <div class="tags">
          <span class="util util-life">Life Steal</span>
          <span class="util util-stats">+All Stats</span>
        </div>
        <ul class="ings">
          <li><span class="qty">1</span> Oyster</li>
          <li><span class="qty">1</span> Glob of Gelatin</li>
          <li><span class="qty">1</span> Cultivated Cilantro Leaf</li>
          <li><span class="qty">1</span> Bowl of Vegetable Stock</li>
        </ul>
      </article>

      <article class="recipe">
        <h3>Spherified Peppercorn-Spiced Oyster Soup</h3>
        <div class="tags">
          <span class="util util-mit">−10% Damage</span>
          <span class="util util-stats">+All Stats</span>
        </div>
        <ul class="ings">
          <li><span class="qty">1</span> Oyster</li>
          <li><span class="qty">1</span> Glob of Gelatin</li>
          <li><span class="qty">1</span> Cultivated Peppercorn</li>
          <li><span class="qty">1</span> Bowl of Vegetable Stock</li>
        </ul>
      </article>

      <p class="base"><strong>Base:</strong> Snow Truffle + Glob of Gelatin.</p>

      <article class="recipe">
        <h3>Truffle Ravioli Variations</h3>
        <div class="tags"><span class="util util-mit">Peppercorn → Tank (−10% Damage)</span></div>
        <ul class="ings">
          <li><span class="qty">1</span> Snow Truffle</li>
          <li><span class="qty">2</span> Glob of Gelatin</li>
          <li><span class="qty">1</span> Bowl of Garlic Butter Sauce</li>
          <li><span class="qty">1</span> Cultivated Herb (Peppercorn for tanking)</li>
        </ul>
      </article>
    </div>
  </section>

  <footer>Built into GW2 In-Game Helper. Not affiliated with ArenaNet or NCSoft. Informational reference only.</footer>
</div>
</body>
</html>
)HTML";
}

std::string RaidFood::EnsureFileUrl(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return {};

	const std::wstring path = addonDir + L"\\raid-food.html";
	const std::wstring verPath = addonDir + L"\\raid-food.ver";

	char buf[64] = {};
	HANDLE vin = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (vin != INVALID_HANDLE_VALUE)
	{
		DWORD read = 0;
		if (ReadFile(vin, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
		{
			buf[read] = '\0';
			if (std::strcmp(buf, kRaidFoodVersion) == 0)
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

	const char* html = Html();
	const DWORD len = static_cast<DWORD>(std::strlen(html));
	HANDLE hout = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hout == INVALID_HANDLE_VALUE)
		return {};
	DWORD written = 0;
	const BOOL ok = WriteFile(hout, html, len, &written, nullptr);
	CloseHandle(hout);
	if (!ok || written != len)
		return {};

	HANDLE vout = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (vout != INVALID_HANDLE_VALUE)
	{
		DWORD vw = 0;
		WriteFile(vout, kRaidFoodVersion, static_cast<DWORD>(std::strlen(kRaidFoodVersion)), &vw, nullptr);
		CloseHandle(vout);
	}

	return PathToFileUrl(path);
}
