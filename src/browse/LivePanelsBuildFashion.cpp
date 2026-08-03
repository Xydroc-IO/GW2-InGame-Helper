#include "LivePanelsBuildShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
struct InfusionRow { const char* name; const char* url; };

const InfusionRow kInfusions[] = {
	{ "Arcane Flow Infusion", "https://wiki.guildwars2.com/wiki/Arcane_Flow_Infusion" },
	{ "Forest Wisp Infusion", "https://wiki.guildwars2.com/wiki/Forest_Wisp_Infusion" },
	{ "Mystic Infusion", "https://wiki.guildwars2.com/wiki/Mystic_Infusion" },
	{ "Queen Bee Infusion", "https://wiki.guildwars2.com/wiki/Queen_Bee_Infusion" },
	{ "Liquid Aurillium Infusion", "https://wiki.guildwars2.com/wiki/Liquid_Aurillium_Infusion" },
	{ "Chak Infusion", "https://wiki.guildwars2.com/wiki/Chak_Infusion" },
	{ "Festive Confetti Infusion", "https://wiki.guildwars2.com/wiki/Festive_Confetti_Infusion" },
	{ "Crystal Infusion", "https://wiki.guildwars2.com/wiki/Crystal_Infusion" },
	{ "Heart of the Khan-Ur", "https://wiki.guildwars2.com/wiki/Heart_of_the_Khan-Ur" },
	{ "Frost Legion Infusion", "https://wiki.guildwars2.com/wiki/Frost_Legion_Infusion" },
	{ "Jormag Left Eye Infusion", "https://wiki.guildwars2.com/wiki/Jormag_Left_Eye_Infusion" },
	{ "Jormag Right Eye Infusion", "https://wiki.guildwars2.com/wiki/Jormag_Right_Eye_Infusion" },
	{ "Echo of the Dragonvoid", "https://wiki.guildwars2.com/wiki/Echo_of_the_Dragonvoid" },
	{ "Chromatic Bubbles", "https://wiki.guildwars2.com/wiki/Chromatic_Bubbles" },
	{ "Seer Transcendence", "https://wiki.guildwars2.com/wiki/Seer_Transcendence" },
	{ "Ethereal Sea-Life Infusion", "https://wiki.guildwars2.com/wiki/Ethereal_Sea-Life_Infusion" },
	{ "Celestial Infusion (Blue)", "https://wiki.guildwars2.com/wiki/Celestial_Infusion_(Blue)" },
	{ "Celestial Infusion (Red)", "https://wiki.guildwars2.com/wiki/Celestial_Infusion_(Red)" },
	{ "Abyssal Infusion", "https://wiki.guildwars2.com/wiki/Abyssal_Infusion" },
	{ "Ghostly Infusion", "https://wiki.guildwars2.com/wiki/Ghostly_Infusion" },
	{ "Peerless Infusion", "https://wiki.guildwars2.com/wiki/Peerless_Infusion" },
	{ "Imperial Everbloom", "https://wiki.guildwars2.com/wiki/Imperial_Everbloom" },
	{ "Clockwork Infusion", "https://wiki.guildwars2.com/wiki/Clockwork_Infusion" },
	{ "Demonic Infusion", "https://wiki.guildwars2.com/wiki/Demonic_Infusion" },
	{ "Deldrimor Stoneskin Infusion", "https://wiki.guildwars2.com/wiki/Deldrimor_Stoneskin_Infusion" },
	{ "Ember Infusion", "https://wiki.guildwars2.com/wiki/Ember_Infusion" },
	{ "Winter's Heart Infusion", "https://wiki.guildwars2.com/wiki/Winter%27s_Heart_Infusion" },
	{ "Snow Diamond Infusion", "https://wiki.guildwars2.com/wiki/Snow_Diamond_Infusion" },
	{ "Mistwalker Infusion", "https://wiki.guildwars2.com/wiki/Mistwalker_Infusion" },
	{ "Heat Core Infusion", "https://wiki.guildwars2.com/wiki/Heat_Core_Infusion" },
};

std::string EnsureColorsJson(const std::wstring& addonDir)
{
	const std::wstring path = StemPath(addonDir, "live-colors", L".json");
	if (FileFresh(path, kColorsTtlSec))
	{
		std::string cached = ReadUtf8File(path);
		if (!cached.empty())
			return cached;
	}
	/* Cached to disk for a week — keep budget modest so a cold miss cannot
	   pin a Live worker for tens of seconds. */
	auto r = Gw2Http::Api("/v2/colors?ids=all", nullptr, 6000);
	if (r.ok && r.body.size() > 10)
	{
		WriteUtf8File(path, r.body);
		return r.body;
	}
	std::string stale = ReadUtf8File(path);
	return stale;
}

std::string BuildFashionHtml(const std::wstring& addonDir)
{
	std::string colors = EnsureColorsJson(addonDir);
	std::string body;
	body += "<p class=\"note\">Checklist is local (browser storage) — wishlist / planning only. "
		"No wardrobe or game-memory reads. Tick dyes and infusions you want; Reload refreshes the dye catalog.</p>";

	body += "<section class=\"block\" id=\"dyes\"><div class=\"head\"><h2>Dye wishlist</h2>"
		"<p>From official /v2/colors — filter and tick</p></div><div class=\"body\">";
	body += "<input id=\"dyeFilter\" type=\"search\" placeholder=\"Filter dyes…\" autocomplete=\"off\"/>";
	body += "<ul class=\"checks\" id=\"dyeList\">";

	int dyeCount = 0;
	if (!colors.empty())
	{
		size_t p = 0;
		while (dyeCount < 2000)
		{
			size_t idKey = colors.find("\"id\"", p);
			if (idKey == std::string::npos)
				break;
			long long id = JsonIntAfterKey(colors, "id", idKey);
			size_t nameKey = colors.find("\"name\"", idKey);
			size_t nextId = colors.find("\"id\"", idKey + 4);
			if (id < 0 || nameKey == std::string::npos ||
				(nextId != std::string::npos && nameKey > nextId))
			{
				p = idKey + 4;
				continue;
			}
			std::string name = JsonStringAfterKey(colors, "name", nameKey);
			if (name.empty())
			{
				p = idKey + 4;
				continue;
			}

			char rgb[32] = "888888";
			auto tryParseRgbArray = [&](size_t fromKey, size_t limit) -> bool {
				if (fromKey == std::string::npos || fromKey >= limit)
					return false;
				size_t br = colors.find('[', fromKey);
				if (br == std::string::npos || br >= limit)
					return false;
				int rv = 0, gv = 0, bv = 0;
				/* Pretty-printed arrays: [\\n 54,\\n 130,\\n 160\\n] — %d skips whitespace. */
				if (std::sscanf(colors.c_str() + br, "[%d,%d,%d]", &rv, &gv, &bv) != 3)
					return false;
				if (rv < 0) rv = 0;
				if (rv > 255) rv = 255;
				if (gv < 0) gv = 0;
				if (gv > 255) gv = 255;
				if (bv < 0) bv = 0;
				if (bv > 255) bv = 255;
				std::snprintf(rgb, sizeof(rgb), "%02x%02x%02x", rv, gv, bv);
				return true;
			};

			/* base_rgb is often a shared red placeholder; cloth/leather/metal hold the real swatch. */
			const size_t colorEnd = (nextId != std::string::npos) ? nextId : colors.size();
			size_t cloth = colors.find("\"cloth\"", idKey);
			size_t leather = colors.find("\"leather\"", idKey);
			size_t metal = colors.find("\"metal\"", idKey);
			size_t clothRgb = (cloth != std::string::npos && cloth < colorEnd)
				? colors.find("\"rgb\"", cloth) : std::string::npos;
			size_t leatherRgb = (leather != std::string::npos && leather < colorEnd)
				? colors.find("\"rgb\"", leather) : std::string::npos;
			size_t metalRgb = (metal != std::string::npos && metal < colorEnd)
				? colors.find("\"rgb\"", metal) : std::string::npos;
			size_t baseRgb = colors.find("\"base_rgb\"", idKey);
			if (baseRgb != std::string::npos && baseRgb >= colorEnd)
				baseRgb = std::string::npos;

			if (!tryParseRgbArray(clothRgb, colorEnd) &&
				!tryParseRgbArray(leatherRgb, colorEnd) &&
				!tryParseRgbArray(metalRgb, colorEnd))
				tryParseRgbArray(baseRgb, colorEnd);

			std::string lower = name;
			for (char& c : lower)
				if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');

			body += "<li class=\"dye\" data-name=\"";
			body += HtmlEscape(lower);
			body += "\"><label class=\"check\"><input type=\"checkbox\"/>";
			body += "<span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\">";
			body += "<span class=\"swatch\" style=\"background:#";
			body += rgb;
			body += "\"></span><strong>";
			body += HtmlEscape(name);
			body += "</strong> <span class=\"muted\">#";
			body += std::to_string(id);
			body += "</span></span></label></li>";
			++dyeCount;
			p = (nextId != std::string::npos) ? nextId : colors.size();
		}
	}
	body += "</ul>";
	if (dyeCount == 0)
		body += "<p class=\"note\">Dye catalog unavailable (network). Infusions below still work.</p>";
	else
	{
		body += "<p class=\"meta\">";
		body += std::to_string(dyeCount);
		body += " dyes loaded.</p>";
	}
	body += "</div></section>\n";

	body += "<section class=\"block\" id=\"infusions\"><div class=\"head\"><h2>Infusion wishlist</h2>"
		"<p>Curated wiki links — tick what you want</p></div><div class=\"body\"><ul class=\"checks\">";
	for (const InfusionRow& row : kInfusions)
	{
		body += "<li><label class=\"check\"><input type=\"checkbox\"/>"
			"<span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\"><strong>";
		body += HtmlEscape(row.name);
		body += "</strong> — <a class=\"link\" href=\"";
		body += row.url;
		body += "\">wiki</a></span></label></li>";
	}
	body += "</ul>"
		"<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Cosmetic_infusion\">"
		"All cosmetic infusions (wiki)</a></p></div></section>\n";

	const char* filterJs =
		"<script>\n"
		"(function(){var f=document.getElementById('dyeFilter');var list=document.getElementById('dyeList');"
		"if(!f||!list)return;f.addEventListener('input',function(){var q=(f.value||'').toLowerCase();"
		"var items=list.querySelectorAll('li.dye');for(var i=0;i<items.length;i++){"
		"var n=items[i].getAttribute('data-name')||'';items[i].style.display=!q||n.indexOf(q)>=0?'':'none';}});})();\n"
		"</script>\n";

	return BuildPage(
		"Live — Fashion Wishlist",
		"GW2 In-Game Helper · Live",
		"Fashion Wishlist",
		"Dyes and cosmetic infusions — plan looks without leaving the game.",
		"<a href=\"#dyes\">Dyes</a>\n<a href=\"#infusions\">Infusions</a>",
		body,
		filterJs);
}

} // namespace LivePanelsBuild
