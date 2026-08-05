#include "LivePanelsBuildShared.h"

#include "Sites.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace LivePanelsBuild
{
namespace
{
	std::string Esc(const std::string& s) { return HtmlEscape(s); }

	std::string ToLower(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	const char* HubCss()
	{
		return R"CSS(
:root{
  --bg:#06070a;--panel:rgba(16,18,24,.94);--panel-2:#12141a;
  --border:#5a4a28;--border-soft:rgba(235,192,71,.22);
  --gold:#f0c65a;--gold-bright:#ffe08a;--gold-dim:#c9a227;
  --text:#f0f2f5;--muted:#a8aeb8;--accent:#1a1510;
}
*{box-sizing:border-box}
body{
  margin:0;min-height:100vh;
  font-family:"Segoe UI",Tahoma,sans-serif;
  background:
    radial-gradient(ellipse 80% 50% at 50% -10%,rgba(235,192,71,.16),transparent 55%),
    linear-gradient(180deg,#14161c 0%,var(--bg) 42%),var(--bg);
  color:var(--text);line-height:1.5;
}
.wrap{max-width:1100px;margin:0 auto;padding:28px 22px 72px}
.hero{margin-bottom:22px;padding-bottom:18px;border-bottom:1px solid var(--border-soft)}
.eyebrow{margin:0 0 8px;font-size:.75rem;letter-spacing:.16em;text-transform:uppercase;color:var(--gold-dim)}
h1{margin:0 0 8px;font-size:2rem;font-weight:700;color:var(--gold)}
.tag{margin:0;color:var(--muted);font-size:.98rem;max-width:42rem}
.back{
  display:inline-block;margin:0 0 12px;color:var(--gold-dim);text-decoration:none;font-size:.9rem;
}
.back:hover{color:var(--gold-bright)}
.search{
  width:100%;max-width:28rem;margin:18px 0 0;height:2.6rem;
  border:1px solid var(--border);background:var(--accent);color:var(--text);
  padding:0 .9rem;font-size:.92rem;
}
.search:focus{outline:1px solid var(--gold-dim)}
.toc{
  display:flex;flex-wrap:wrap;gap:.45rem;margin:16px 0 0;padding:12px 0 2px;
  position:sticky;top:0;z-index:5;
  background:linear-gradient(180deg,rgba(6,7,10,.97) 70%,rgba(6,7,10,.88));
  border-bottom:1px solid var(--border-soft);
}
a.jump{
  display:inline-block;padding:.35rem .7rem;font-size:.78rem;font-weight:600;
  letter-spacing:.02em;text-decoration:none;color:var(--gold-dim);
  border:1px solid var(--border);background:var(--accent);border-radius:3px;
}
a.jump:hover{color:var(--gold-bright);border-color:var(--gold)}
.sec{margin-top:1.75rem;scroll-margin-top:3.25rem}
.sec h2{
  margin:0 0 .75rem;font-size:1rem;letter-spacing:.08em;text-transform:uppercase;
  color:var(--gold-bright);border-left:3px solid var(--gold);padding-left:.65rem;
}
.sec h3{
  margin:1rem 0 .55rem;font-size:.88rem;letter-spacing:.04em;
  color:var(--muted);font-weight:600;
}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:.75rem}
.tile-wrap{
  position:relative;display:flex;flex-direction:column;
  min-height:5.25rem;
}
a.star{
  position:absolute;top:.45rem;right:.5rem;z-index:2;
  width:1.6rem;height:1.6rem;line-height:1.55rem;text-align:center;
  text-decoration:none;font-size:1.05rem;border-radius:4px;
  color:var(--muted);background:rgba(0,0,0,.35);
}
a.star.on{color:var(--gold-bright)}
a.star:hover{color:var(--gold);background:rgba(0,0,0,.55)}
a.tile{
  display:flex;flex-direction:column;justify-content:center;gap:.35rem;
  flex:1;min-height:5.25rem;padding:1rem 2.1rem 1rem 1.05rem;
  text-decoration:none;color:var(--text);
  background:linear-gradient(165deg,rgba(26,23,16,.95),var(--panel));
  border:1px solid var(--border);border-left:3px solid var(--gold-dim);
  transition:border-color .15s,transform .12s,background .15s;
}
a.tile:hover{
  border-color:var(--gold);border-left-color:var(--gold-bright);
  background:linear-gradient(165deg,#221c12,var(--panel-2));
  transform:translateY(-1px);
}
a.tile .name{font-size:1.02rem;font-weight:650;color:var(--gold-bright)}
a.tile .blurb{font-size:.8rem;color:var(--muted);line-height:1.35}
a.tile .meta{font-size:.72rem;color:var(--gold-dim);margin-top:.15rem}
.foot{margin-top:2.5rem;font-size:.78rem;color:var(--muted)}
.empty{margin:2rem 0;color:var(--muted)}
.hidden{display:none!important}
)CSS";
	}

	const char* HubJs()
	{
		return R"JS(
(function(){
  var q=document.getElementById("q");
  if(!q)return;
  q.addEventListener("input",function(){
    var needle=(q.value||"").trim().toLowerCase();
    document.querySelectorAll("[data-sec]").forEach(function(sec){
      var any=false;
      sec.querySelectorAll("[data-q]").forEach(function(el){
        var hay=(el.getAttribute("data-q")||"");
        var show=!needle||hay.indexOf(needle)>=0;
        el.classList.toggle("hidden",!show);
        if(show)any=true;
      });
      sec.querySelectorAll("[data-sub]").forEach(function(sub){
        var anySub=false;
        sub.querySelectorAll("[data-q]").forEach(function(el){
          if(!el.classList.contains("hidden"))anySub=true;
        });
        sub.classList.toggle("hidden",!anySub);
      });
      sec.classList.toggle("hidden",!any);
      var id=sec.getAttribute("id");
      if(id){
        var jump=document.querySelector('a.jump[href="#'+id+'"]');
        if(jump)jump.classList.toggle("hidden",!any);
      }
    });
  });
})();
)JS";
	}

	std::string SectionAnchorId(const std::string& section, int index)
	{
		std::string id = "sec-";
		id += std::to_string(index);
		id += "-";
		bool dash = false;
		for (unsigned char c : section)
		{
			if (std::isalnum(c))
			{
				id.push_back(static_cast<char>(std::tolower(c)));
				dash = false;
			}
			else if (!dash && id.size() > 4)
			{
				id.push_back('-');
				dash = true;
			}
		}
		while (!id.empty() && id.back() == '-')
			id.pop_back();
		return id;
	}

	void AppendTile(std::string& html, const SiteDef& s, const std::string& pathBlurb)
	{
		const char* id = (s.id && s.id[0]) ? s.id : "";
		if (!id[0] || !s.label || !s.label[0])
			return;
		const std::string label = s.label;
		const std::string title = (s.title && s.title[0]) ? s.title : label;
		std::string q = ToLower(std::string(id) + " " + label + " " + title + " " +
			(s.category ? s.category : "") + " " + pathBlurb);
		const bool fav = Sites::IsFavorite(id);
		html += "<div class=\"tile-wrap\" data-q=\"";
		html += Esc(q);
		html += "\"><a class=\"star";
		if (fav)
			html += " on";
		html += "\" href=\"?gw2igh-fav-toggle=";
		html += Esc(id);
		html += "\" title=\"";
		html += fav ? "Remove favorite" : "Add favorite";
		html += "\">";
		html += fav ? "★" : "☆";
		html += "</a><a class=\"tile\" href=\"?gw2igh-open-site=";
		html += Esc(id);
		html += "\"><span class=\"name\">";
		html += Esc(label);
		html += "</span><span class=\"blurb\">";
		html += Esc(title);
		html += "</span>";
		if (!pathBlurb.empty())
		{
			html += "<span class=\"meta\">";
			html += Esc(pathBlurb);
			html += "</span>";
		}
		html += "</a></div>";
	}
} // namespace

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

std::string BuildBrowseHubHtml(const std::wstring& /*addonDir*/, const char* /*apiKey*/)
{
	std::string html;
	html.reserve(24000);
	html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
		"<title>Browse</title><style>";
	html += HubCss();
	html += "</style></head><body><div class=\"wrap\">"
		"<header class=\"hero\">"
		"<p class=\"eyebrow\">GW2 In-Game Helper</p>"
		"<h1>Browse</h1>"
		"<p class=\"tag\">Pick a category, or open a favorite in a new tab. "
		"Use the star to pin sites here.</p>"
		"<input class=\"search\" id=\"q\" type=\"search\" placeholder=\"Filter favorites &amp; categories…\" "
		"autocomplete=\"off\"/>"
		"</header>";

	/* Favorites */
	html += "<section class=\"sec\" data-sec=\"1\"><h2>Favorites</h2>";
	const int favCount = Sites::FavoriteCount();
	if (favCount <= 0)
	{
		html += "<p class=\"empty\">No favorites yet — open a category and tap ☆ on a site.</p>";
	}
	else
	{
		html += "<div class=\"grid\">";
		size_t n = 0;
		const SiteDef* sites = Sites::All(&n);
		for (int i = 0; i < favCount; ++i)
		{
			const int idx = Sites::FavoriteSiteIndex(i);
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
			AppendTile(html, s, path);
		}
		html += "</div>";
	}
	html += "</section>";

	/* Categories */
	html += "<section class=\"sec\" data-sec=\"1\"><h2>Categories</h2><div class=\"grid\">";
	size_t catCount = 0;
	const char* const* cats = Sites::Categories(&catCount);
	int shown = 0;
	for (size_t i = 0; i < catCount; ++i)
	{
		const char* cat = cats[i] ? cats[i] : "";
		if (!cat[0] || std::strcmp(cat, "Cheat Sheets") == 0)
			continue;
		const std::string slug = BrowseCategorySlug(cat);
		if (slug.empty())
			continue;
		const int count = Sites::CountInCategory(cat);
		std::string q = ToLower(std::string(cat) + " " + slug);
		html += "<a class=\"tile\" data-q=\"";
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

	html += "<p class=\"foot\">Cheat Sheets have their own side-rail button. "
		"Tab bar <strong>+</strong> still opens the quick picker.</p>"
		"</div><script>";
	html += HubJs();
	html += "</script></body></html>";
	return html;
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
		"<header class=\"hero\">"
		"<p class=\"eyebrow\">Browse</p>"
		"<h1>";
	html += Esc(cat);
	html += "</h1>"
		"<p class=\"tag\">Building site list…</p>"
		"</header>"
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
		"<header class=\"hero\">"
		"<p class=\"eyebrow\">Browse</p>"
		"<h1>";
	html += Esc(cat);
	html += "</h1>"
		"<p class=\"tag\">Tap a site to open it in a new helper tab. "
		"Use ☆ to add favorites on the Browse hub.</p>"
		"<input class=\"search\" id=\"q\" type=\"search\" placeholder=\"Filter sites…\" "
		"autocomplete=\"off\"/>"
		"</header>";

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
