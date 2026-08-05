#include "LivePanelsBuildShared.h"

#include "LivePanels_Html.h"

#include <string>

namespace LivePanelsBuild
{
std::string BuildPage(const char* title, const char* eyebrow, const char* heading,
	const char* tagline, const char* toc, const std::string& body, const std::string& extraHead)
{
	std::string out;
	out.reserve(body.size() + 4000);
	out += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\"/>\n";
	out += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n<title>";
	out += title;
	out += "</title>\n<style>\n";
	out += LivePanelsHtml::SharedCss();
	out += "\n</style>\n";
	out += extraHead;
	out += "</head>\n<body>\n<div class=\"wrap\">\n<header class=\"hero\">\n";
	out += "<p class=\"eyebrow\">";
	out += eyebrow;
	out += "</p>\n<h1>";
	out += heading;
	out += "</h1>\n<p class=\"tagline\">";
	out += tagline;
	out += "</p>\n<p class=\"meta\">Updated ";
	out += HtmlEscape(NowLocalStamp());
	out += " · Reload tab for fresh data · Read-only API</p>\n</header>\n";
	if (toc && toc[0])
	{
		out += "<nav class=\"toc\" aria-label=\"Sections\">\n";
		out += toc;
		out += "\n</nav>\n";
	}
	out += body;
	out += "\n</div>\n</body>\n</html>\n";
	return out;
}

void AppendChecklistSection(std::string& body, const char* sectionId, const char* title,
	const char* blurb, const std::string& jsonArray)
{
	body += "<section class=\"block\" id=\"";
	body += sectionId;
	body += "\"><div class=\"head\"><h2>";
	body += title;
	body += "</h2><p>";
	body += blurb;
	body += "</p></div><div class=\"body\"><ul class=\"checks\">";
	int n = 0;
	size_t i = 0;
	while (n < 80 && i < jsonArray.size())
	{
		size_t q = jsonArray.find('"', i);
		if (q == std::string::npos)
			break;
		size_t after = q;
		std::string val = ReadJsonQuoted(jsonArray, q, &after);
		i = after;
		if (val.empty() || val == "id")
			continue;
		body += "<li><label class=\"check\"><input type=\"checkbox\"/>"
			"<span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\"><strong>";
		body += HtmlEscape(HumanizeApiId(val));
		body += "</strong></span></label></li>";
		++n;
	}
	body += "</ul>";
	if (n == 0)
		body += "<p class=\"note\">No data — hit Reload.</p>";
	else
		body += "<p class=\"meta\">Tick items off as you finish them (saved in this browser).</p>";
	body += "</div></section>\n";
}

void AppendVaultObjectives(std::string& body, const char* sectionId, const char* title,
	const std::string& json, bool accountScoped, const char* trackFilter,
	int maxItems)
{
	body += "<section class=\"block\" id=\"";
	body += sectionId;
	body += "\"><div class=\"head\"><h2>";
	body += title;
	body += "</h2><p>";
	body += accountScoped
		? "Live account progress from your API key — tick extras locally if you want."
		: "Catalog checklist (add an API key in Settings for live personal progress).";
	body += "</p></div><div class=\"body\">";

	size_t pos = json.find('[');
	if (pos == std::string::npos)
	{
		body += "<p class=\"note\">No objectives in response.";
		if (accountScoped)
			body += " Check API key scopes (<strong>account</strong> + <strong>progression</strong>) in Settings.";
		body += "</p></div></section>\n";
		return;
	}

	body += "<ul class=\"checks\">";
	int count = 0;
	size_t i = pos;
	while (count < maxItems)
	{
		size_t obj = json.find('{', i);
		if (obj == std::string::npos)
			break;
		size_t end = JsonObjectEnd(json, obj);
		if (end == std::string::npos)
			break;
		const std::string chunk = json.substr(obj, end - obj + 1);
		std::string titleStr = JsonStringAfterKey(chunk, "title");
		if (titleStr.empty())
			titleStr = JsonStringAfterKey(chunk, "name");
		std::string track = JsonStringAfterKey(chunk, "track");
		if (titleStr.empty())
		{
			i = end + 1;
			continue;
		}
		if (trackFilter && trackFilter[0] && track != trackFilter)
		{
			i = end + 1;
			continue;
		}
		long long cur = JsonIntAfterKey(chunk, "progress_current");
		long long need = JsonIntAfterKey(chunk, "progress_complete");
		long long acclaim = JsonIntAfterKey(chunk, "acclaim");
		bool claimed = JsonBoolAfterKey(chunk, "claimed");
		bool done = claimed || (need > 0 && cur >= need);

		body += "<li><label class=\"check\"><input type=\"checkbox\"";
		if (done)
			body += " checked";
		body += "/><span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\">";
		if (done)
			body += "<span class=\"badge\">Done</span>";
		if (!track.empty())
		{
			body += "<span class=\"badge\">";
			body += HtmlEscape(track);
			body += "</span>";
		}
		if (acclaim > 0 && acclaim <= 10)
			body += "<span class=\"badge\">Easy</span>";
		body += "<strong>";
		body += HtmlEscape(titleStr);
		body += "</strong>";
		if (acclaim > 0 || need > 0)
		{
			body += "<br/><span class=\"s\">";
			if (acclaim > 0)
			{
				body += std::to_string(acclaim);
				body += " acclaim";
			}
			if (need > 0)
			{
				if (acclaim > 0) body += " · ";
				body += "progress ";
				body += std::to_string(cur < 0 ? 0 : cur);
				body += " / ";
				body += std::to_string(need);
			}
			body += "</span>";
		}
		if (need > 0)
		{
			int pct = static_cast<int>((100.0 * (cur < 0 ? 0 : cur)) / need);
			if (pct > 100) pct = 100;
			if (pct < 0) pct = 0;
			body += "<div class=\"bar\"><i style=\"width:";
			body += std::to_string(pct);
			body += "%\"></i></div>";
		}
		body += "</span></label></li>";
		++count;
		i = end + 1;
	}
	body += "</ul>";
	if (count == 0)
		body += "<p class=\"note\">No parseable objectives.</p>";
	body += "</div></section>\n";
}

/* CEF page is informational only — add/remove/prices live in ImGui TpWatchPad
   (CEF file:// clicks and shared Live workers were unreliable under Wine). */
std::string BuildTpHtml(const char* /*tpWatchIds*/, bool /*fetchApi*/)
{
	std::string body;
	body += "<section class=\"block\"><div class=\"head\"><h2>TP Watchlist window</h2>"
		"<p>Use the ImGui panel — it is instant and reliable</p></div><div class=\"body\">";
	body += "<p class=\"note\">Your watchlist is managed in the <strong>TP Watchlist</strong> window "
		"(opens automatically from Browse → Live → TP Watchlist, or use the <strong>TP</strong> "
		"toolbar button).</p>";
	body += "<ul class=\"rows\">";
	body += "<li><span class=\"t\">Add any item</span><span class=\"s\">"
		"In GW2: Shift+click an item → copy <code>[&…]</code> → paste in the TP window → Add. "
		"Numeric IDs also work.</span></li>";
	body += "<li><span class=\"t\">Prices</span><span class=\"s\">"
		"Buy / sell / spread refresh in that window (read-only official API, ~2s).</span></li>";
	body += "<li><span class=\"t\">Delivery box</span><span class=\"s\">"
		"Same window shows coins/items waiting to claim via <code>/v2/commerce/delivery</code> "
		"(API key with <strong>tradingpost</strong>). Reminder only — claim in-game.</span></li>";
	body += "</ul>";
	body += "<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://www.gw2bltc.com/\">GW2BLTC</a>"
		" · <a class=\"link\" href=\"https://wiki.guildwars2.com/\">Wiki</a></p>";
	body += "</div></section>\n";
	return BuildPage(
		"Live — Trading Post Watchlist",
		"GW2 In-Game Helper · Live",
		"TP Watchlist",
		"Manage items in the TP Watchlist window beside the helper.",
		nullptr,
		body);
}

} // namespace LivePanelsBuild
