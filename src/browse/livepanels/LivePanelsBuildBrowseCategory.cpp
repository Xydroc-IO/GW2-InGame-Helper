#include "LivePanelsBuildBrowseHubInternal.h"

#include "Sites.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace LivePanelsBuild
{
std::string BrowseCategorySlug(const char* category)
{
	if (!category || !category[0])
		return {};
	std::string out;
	out.reserve(std::strlen(category) + 4);
	bool dash = false;
	for (const char* p = category; *p; ++p)
	{
		const unsigned char c = static_cast<unsigned char>(*p);
		if (std::isalnum(c))
		{
			out.push_back(static_cast<char>(std::tolower(c)));
			dash = false;
		}
		else if (!out.empty() && !dash)
		{
			out.push_back('-');
			dash = true;
		}
	}
	while (!out.empty() && out.back() == '-')
		out.pop_back();
	return out;
}

const char* BrowseCategoryFromSlug(const char* slug)
{
	if (!slug || !slug[0])
		return nullptr;
	size_t n = 0;
	const char* const* cats = Sites::Categories(&n);
	for (size_t i = 0; i < n; ++i)
	{
		if (!cats[i])
			continue;
		if (BrowseCategorySlug(cats[i]) == slug)
			return cats[i];
	}
	return nullptr;
}

std::string BuildBrowseCategoryShellHtml(const char* category)
{
	const char* cat = (category && category[0]) ? category : "Browse";
	std::string html;
	html.reserve(2048);
	html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
		"<title>";
	html += Esc(cat);
	html += "</title><style>";
	html += HubCss();
	html += "</style></head><body><div class=\"wrap\">"
		"<a class=\"back\" href=\"?gw2igh-about=browse-hub\">← All categories</a>"
		"<header class=\"hero\">";
	AppendBrowseHeroArt(html);
	html += "<div class=\"hero-copy\">"
		"<p class=\"eyebrow\">Browse</p>"
		"<h1>";
	html += Esc(cat);
	html += "</h1>"
		"<p class=\"tag\">Building site list…</p>"
		"</div></header>"
		"<p class=\"empty\">Loading sections in the background — this page refreshes when ready.</p>"
		"</div></body></html>";
	return html;
}

std::string BuildBrowseCategoryHtml(const std::wstring& /*addonDir*/, const char* category)
{
	const char* cat = (category && category[0]) ? category : "";
	std::string html;
	html.reserve(256000);
	html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
		"<title>";
	html += Esc(cat);
	html += "</title><style>";
	html += HubCss();
	html += "</style></head><body><div class=\"wrap\">"
		"<a class=\"back\" href=\"?gw2igh-about=browse-hub\">← All categories</a>"
		"<header class=\"hero\">";
	AppendBrowseHeroArt(html);
	html += "<div class=\"hero-copy\">"
		"<p class=\"eyebrow\">Browse</p>"
		"<h1>";
	html += Esc(cat);
	html += "</h1>"
		"<p class=\"tag\">Tap a site to open it in a new helper tab. "
		"Use ☆ to add favorites on the Browse hub.</p>"
		"<input class=\"search\" id=\"q\" type=\"search\" placeholder=\"Filter sites…\" "
		"autocomplete=\"off\"/>"
		"</div></header>";

	if (!cat[0] || std::strcmp(cat, "Cheat Sheets") == 0)
	{
		html += "<p class=\"empty\">Unknown category.</p></div><script>";
		html += HubJs();
		html += "</script></body></html>";
		return html;
	}

	size_t n = 0;
	const SiteDef* sites = Sites::All(&n);
	std::vector<int> indices;
	indices.reserve(static_cast<size_t>(Sites::CountInCategory(cat)) + 8u);
	for (size_t i = 0; i < n; ++i)
	{
		if (sites[i].category && std::strcmp(sites[i].category, cat) == 0)
			indices.push_back(static_cast<int>(i));
	}

	/* Section order from browseSections; unknown sections after. */
	size_t secCount = 0;
	const char* const* ordered = Sites::BrowseSections(cat, &secCount);
	std::vector<std::string> sectionOrder;
	sectionOrder.reserve(secCount + 8);
	std::map<std::string, std::map<std::string, std::vector<int>>> buckets;
	for (size_t i = 0; i < secCount; ++i)
	{
		if (ordered[i] && ordered[i][0])
			sectionOrder.push_back(ordered[i]);
	}

	for (int idx : indices)
	{
		const SiteDef& s = sites[idx];
		std::string section = "General";
		std::string sub;
		if (s.browsePath && s.browsePathCount > 0 && s.browsePath[0] && s.browsePath[0][0])
			section = s.browsePath[0];
		if (s.browsePath && s.browsePathCount > 1)
		{
			for (int p = 1; p < s.browsePathCount; ++p)
			{
				if (!s.browsePath[p] || !s.browsePath[p][0])
					continue;
				if (!sub.empty())
					sub += " / ";
				sub += s.browsePath[p];
			}
		}
		if (buckets.find(section) == buckets.end())
		{
			bool known = false;
			for (const std::string& o : sectionOrder)
			{
				if (o == section)
				{
					known = true;
					break;
				}
			}
			if (!known)
				sectionOrder.push_back(section);
		}
		buckets[section][sub].push_back(idx);
	}

	if (indices.empty())
	{
		html += "<p class=\"empty\">No sites in this category.</p>";
	}
	else
	{
		/* Collect sections that will actually render (for jump buttons). */
		std::vector<std::pair<std::string, std::string>> jumps; /* id, label */
		jumps.reserve(sectionOrder.size());
		int secIndex = 0;
		for (const std::string& section : sectionOrder)
		{
			auto sit = buckets.find(section);
			if (sit == buckets.end() || sit->second.empty())
				continue;
			jumps.emplace_back(SectionAnchorId(section, secIndex), section);
			++secIndex;
		}
		if (jumps.size() > 1)
		{
			html += "<nav class=\"toc\" aria-label=\"Sections\">";
			for (const auto& j : jumps)
			{
				html += "<a class=\"jump\" href=\"#";
				html += Esc(j.first);
				html += "\">";
				html += Esc(j.second);
				html += "</a>";
			}
			html += "</nav>";
		}

		secIndex = 0;
		for (const std::string& section : sectionOrder)
		{
			auto sit = buckets.find(section);
			if (sit == buckets.end() || sit->second.empty())
				continue;
			const std::string anchor = SectionAnchorId(section, secIndex);
			++secIndex;
			html += "<section class=\"sec\" data-sec=\"1\" id=\"";
			html += Esc(anchor);
			html += "\"><h2>";
			html += Esc(section);
			html += "</h2>";
			/* Stable sub order: empty first, then alpha */
			std::vector<std::string> subs;
			subs.reserve(sit->second.size());
			for (const auto& kv : sit->second)
				subs.push_back(kv.first);
			std::sort(subs.begin(), subs.end(), [](const std::string& a, const std::string& b) {
				if (a.empty() != b.empty())
					return a.empty(); /* empty subsection first */
				return a < b;
			});
			for (const std::string& sub : subs)
			{
				auto& list = sit->second[sub];
				std::sort(list.begin(), list.end(), [&](int a, int b) {
					const char* la = sites[a].label ? sites[a].label : "";
					const char* lb = sites[b].label ? sites[b].label : "";
					return std::strcmp(la, lb) < 0;
				});
				html += "<div data-sub=\"1\">";
				if (!sub.empty())
				{
					html += "<h3>";
					html += Esc(sub);
					html += "</h3>";
				}
				html += "<div class=\"grid\">";
				const std::string pathBlurb = sub.empty() ? section : (section + " / " + sub);
				for (int idx : list)
					AppendTile(html, sites[idx], pathBlurb);
				html += "</div></div>";
			}
			html += "</section>";
		}
	}

	html += "<p class=\"foot\">Opens in a new helper tab (tab limit: 8).</p>"
		"</div><script>";
	html += HubJs();
	html += "</script></body></html>";
	return html;
}

} // namespace LivePanelsBuild
