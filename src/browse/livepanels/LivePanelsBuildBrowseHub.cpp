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
	html.reserve(24000);
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
		"<p class=\"tag\">Pick a category, or open a favorite in a new tab. "
		"Star sites to pin them, create folders with <strong>+ Folder</strong>, "
		"tap <strong>⇄</strong> to move a favorite, or <strong>Delete</strong> on a folder "
		"header to remove a mistaken folder (sites return to Unfiled).</p>"
		"<input class=\"search\" id=\"q\" type=\"search\" placeholder=\"Filter favorites &amp; categories…\" "
		"autocomplete=\"off\"/>"
		"</div></header>";

	/* Favorites (folder sections) — + Folder creates via helper IPC. */
	html += "<section class=\"sec\" data-sec=\"1\" data-keep=\"1\">"
		"<div class=\"sec-head\">"
		"<h2>Favorites</h2>"
		"<button type=\"button\" class=\"btn-plus\" id=\"fav-add-folder\" "
		"title=\"Create a folder to organize favorites\">+ Folder</button>"
		"</div>";
	const int favCount = Sites::FavoriteCount();
	const int folderN = Sites::FavoriteFolderCount();
	if (favCount <= 0 && folderN <= 0)
	{
		html += "<p class=\"empty\">No favorites yet — open a category and tap ☆ on a site, "
			"or create a folder first with <strong>+ Folder</strong>.</p>";
	}
	else
	{
		size_t n = 0;
		const SiteDef* sites = Sites::All(&n);
		auto AppendFolder = [&](int folderId) {
			const int count = Sites::FavoriteCountInFolder(folderId);
			/* Always show user folders (even empty) so + Folder is useful immediately. */
			if (count <= 0 && folderId == 0)
				return;
			const char* fname = Sites::FavoriteFolderName(folderId);
			html += "<div class=\"fav-fold-head\"><h3>";
			html += Esc(fname ? fname : "Folder");
			html += " (";
			html += std::to_string(count);
			html += ")</h3>";
			if (folderId != 0)
			{
				html += "<a class=\"fold-del\" href=\"?gw2igh-fav-folder-delete=";
				html += std::to_string(folderId);
				html += "\" title=\"Delete folder — favorites in it return to Unfiled\" "
					"onclick=\"return confirm('Delete this folder? Favorites inside move to Unfiled.');\">"
					"Delete</a>";
			}
			html += "</div><div class=\"grid\">";
			for (int i = 0; i < count; ++i)
			{
				const int idx = Sites::FavoriteSiteIndexInFolder(folderId, i);
				if (idx < 0 || idx >= static_cast<int>(n))
					continue;
				const SiteDef& s = sites[idx];
				std::string path;
				if (s.browsePath && s.browsePathCount > 0)
				{
					for (int p = 0; p < s.browsePathCount; ++p)
					{
						if (p)
							path += " / ";
						if (s.browsePath[p])
							path += s.browsePath[p];
					}
				}
				AppendTile(html, s, path, true, folderId);
			}
			if (count <= 0)
				html += "<p class=\"empty\">Empty — open <strong>⇄</strong> on a starred site "
					"and pick this folder.</p>";
			html += "</div>";
		};
		AppendFolder(0);
		for (int fi = 0; fi < folderN; ++fi)
			AppendFolder(Sites::FavoriteFolderIdAt(fi));
	}
	html += "</section>"
		"<div id=\"fav-folder-modal\" class=\"modal-backdrop hidden\" role=\"dialog\" "
		"aria-labelledby=\"fav-folder-title\">"
		"<div class=\"modal\">"
		"<h3 id=\"fav-folder-title\">New favorites folder</h3>"
		"<p class=\"hint\">After creating a folder, tap <strong>⇄</strong> on any favorite "
		"and choose the folder name.</p>"
		"<input id=\"fav-folder-name\" type=\"text\" maxlength=\"47\" "
		"placeholder=\"Folder name…\" autocomplete=\"off\"/>"
		"<div class=\"modal-actions\">"
		"<button type=\"button\" id=\"fav-folder-cancel\">Cancel</button>"
		"<button type=\"button\" class=\"primary\" id=\"fav-folder-create\">Create</button>"
		"</div></div></div>";

	/* Categories — two rows of three: Builds Guides Tools / Help Search Discord */
	html += "<section class=\"sec\" data-sec=\"1\"><h2>Categories</h2><div class=\"grid grid-cats\">";
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
		"<p class=\"credit\">Created By Xydroc</p>"
		"<p class=\"credit-issue\">Report any issues here — Discord — "
		"<a href=\"?gw2igh-newtab=https%3A%2F%2Fdiscord.gg%2FkA8PvbuymS\">"
		"Raidcore</a> — Channel — "
		"<a href=\"?gw2igh-newtab=https%3A%2F%2Fdiscord.com%2Fchannels%2F"
		"410828272679518241%2F1531031243196727407\">"
		"GW2-InGame-Helper</a></p>"
		"</div><script>";
	html += HubJs();
	html += "</script>";
	html += HelperThemeCss::ViewportSyncJs();
	html += "</body></html>";
	return html;
}

} // namespace LivePanelsBuild
