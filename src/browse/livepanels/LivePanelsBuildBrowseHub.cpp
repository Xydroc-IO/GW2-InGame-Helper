#include "LivePanelsBuildBrowseHubInternal.h"

#include "HelperThemeCss.h"
#include "Sites.h"

#include <cstring>
#include <string>

namespace LivePanelsBuild
{
std::string BuildBrowseHubHtml(const std::wstring& /*addonDir*/, const char* /*apiKey*/)
{
	std::string html;
	html.reserve(12000);
	html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
		"<title>Browse</title><style>";
	html += HubCss();
	html += "</style></head><body><div class=\"wrap\">"
		"<header class=\"hero\">";
	AppendBrowseHeroArt(html);
	html += "<div class=\"hero-copy\">"
		"<p class=\"eyebrow\">GW2 In-Game Helper</p>"
		"<h1>Browse</h1>"
		"<p class=\"tag\">Pick a category — it opens in this tab. "
		"Use <strong>+</strong> or <strong>Ctrl+T</strong> when you want a new tab. "
		"Star the address bar to bookmark the current page.</p>"
		"<input class=\"search\" id=\"q\" type=\"search\" placeholder=\"Filter categories…\" "
		"autocomplete=\"off\"/>"
		"</div></header>";

	/* Categories — two rows of three: Builds Guides Tools / Help Search Discord */
	html += "<section class=\"sec\" data-sec=\"1\"><h2>Categories</h2>"
		"<p class=\"sec-hint\">Bookmarks are on the bar above — star any page or ☆ a site below.</p>"
		"<div class=\"grid grid-cats\">";
	static const char* kHubCats[] = {
		"Builds", "Guides", "Tools", "Help", "Search", "Discord",
	};
	int shown = 0;
	for (const char* cat : kHubCats)
	{
		if (!BrowseHubShowsCategory(cat))
			continue;
		const std::string slug = BrowseCategorySlug(cat);
		if (slug.empty())
			continue;
		const int count = Sites::CountInCategory(cat);
		if (count <= 0)
			continue;
		std::string q = ToLower(std::string(cat) + " " + slug);
		html += "<a class=\"tile tile-cat\" data-q=\"";
		html += Esc(q);
		html += "\" href=\"?gw2igh-about=browse-cat-";
		html += Esc(slug);
		html += "\"><span class=\"name\">";
		html += Esc(cat);
		html += "</span><span class=\"blurb\">";
		html += std::to_string(count);
		html += " sites</span></a>";
		++shown;
	}
	html += "</div>";
	if (shown == 0)
		html += "<p class=\"empty\">No categories found.</p>";
	html += "</section>";

	html += "<p class=\"foot\">Wiki and Cheat Sheets have their own side-rail buttons. "
		"Tab bar <strong>+</strong> still opens the quick picker.</p>"
		"<section class=\"block troubleshoot\">"
		"<div class=\"head\"><h2>Troubleshooting</h2></div>"
		"<div class=\"body\">"
		"<p><strong>Site login, video, or bot check fails in-game</strong></p>"
		"<p>This overlay is a windowless Chromium helper — not a full Chrome install. "
		"Many logins, Cloudflare checks, and video players will not work here. "
		"Rebuilding Chromium would still leave most of those sites unreliable. "
		"Use <strong>Open Ext</strong> in the toolbar (that session is separate from in-game tabs).</p>"
		"<p class=\"trouble-report\">Report any issues here — Discord — "
		"<a href=\"?gw2igh-newtab=https%3A%2F%2Fdiscord.gg%2FkA8PvbuymS\">"
		"Raidcore</a> — Channel — "
		"<a href=\"?gw2igh-newtab=https%3A%2F%2Fdiscord.com%2Fchannels%2F"
		"410828272679518241%2F1531031243196727407\">"
		"GW2-InGame-Helper</a></p>"
		"</div></section>"
		"<p class=\"credit\">Created By Xydroc</p>"
		"<p class=\"credit-donate\">If you would like to donate or support GW2-InGame-Helper, "
		"you can do so here — "
		"<a href=\"?gw2igh-newtab=https%3A%2F%2Fko-fi.com%2Fxydroc\">"
		"ko-fi.com/xydroc</a></p>"
		"</div><script>";
	html += HubJs();
	html += "</script>";
	html += HelperThemeCss::ViewportSyncJs();
	html += "</body></html>";
	return html;
}

} // namespace LivePanelsBuild
