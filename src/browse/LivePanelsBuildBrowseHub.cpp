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
.sec-head{
  display:flex;align-items:center;gap:.75rem;flex-wrap:wrap;
  margin:0 0 .75rem;
}
.sec-head h2{
  margin:0;font-size:1rem;letter-spacing:.08em;text-transform:uppercase;
  color:var(--gold-bright);border-left:3px solid var(--gold);padding-left:.65rem;
}
.sec h2{
  margin:0 0 .75rem;font-size:1rem;letter-spacing:.08em;text-transform:uppercase;
  color:var(--gold-bright);border-left:3px solid var(--gold);padding-left:.65rem;
}
.sec h3{
  margin:1rem 0 .55rem;font-size:.88rem;letter-spacing:.04em;
  color:var(--muted);font-weight:600;
}
button.btn-plus{
  appearance:none;cursor:pointer;margin:0;padding:.28rem .7rem;
  font:inherit;font-size:.78rem;font-weight:650;letter-spacing:.02em;
  color:var(--gold-bright);background:var(--accent);
  border:1px solid var(--border);border-radius:3px;
}
button.btn-plus:hover{border-color:var(--gold);color:var(--gold)}
.modal-backdrop{
  position:fixed;inset:0;z-index:40;
  display:flex;align-items:center;justify-content:center;
  background:rgba(0,0,0,.55);
}
.modal{
  width:min(22rem,92vw);padding:1.15rem 1.2rem 1.05rem;
  background:var(--panel);border:1px solid var(--border);
  box-shadow:0 12px 40px rgba(0,0,0,.45);
}
.modal h3{margin:0 0 .65rem;color:var(--gold-bright);font-size:1rem}
.modal .hint{margin:0 0 .85rem;font-size:.82rem;color:var(--muted)}
.modal input{
  width:100%;height:2.4rem;margin:0 0 1rem;padding:0 .75rem;
  border:1px solid var(--border);background:var(--accent);color:var(--text);
  font:inherit;font-size:.92rem;
}
.modal input:focus{outline:1px solid var(--gold-dim)}
.modal-actions{display:flex;gap:.55rem;justify-content:flex-end}
.modal-actions button{
  appearance:none;cursor:pointer;min-width:5.5rem;height:2.2rem;
  padding:0 .85rem;font:inherit;font-size:.85rem;font-weight:600;
  border-radius:3px;border:1px solid var(--border);
  color:var(--text);background:var(--accent);
}
.modal-actions button.primary{
  color:#1a1208;background:linear-gradient(180deg,var(--gold-bright),var(--gold-dim));
  border-color:var(--gold);
}
.modal-actions button:hover{border-color:var(--gold)}
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
details.move{
  position:absolute;top:.45rem;left:.5rem;z-index:2;
}
details.move>summary{
  list-style:none;cursor:pointer;width:1.6rem;height:1.6rem;
  line-height:1.55rem;text-align:center;font-size:.85rem;font-weight:700;
  border-radius:4px;color:var(--muted);background:rgba(0,0,0,.35);
  border:1px solid transparent;
}
details.move>summary::-webkit-details-marker{display:none}
details.move>summary:hover,details.move[open]>summary{
  color:var(--gold-bright);border-color:var(--border);background:rgba(0,0,0,.55);
}
details.move .move-menu{
  position:absolute;left:0;top:1.85rem;min-width:9.5rem;
  padding:.35rem;background:var(--panel);border:1px solid var(--border);
  box-shadow:0 8px 24px rgba(0,0,0,.45);
}
details.move .move-menu a{
  display:block;padding:.35rem .55rem;text-decoration:none;
  color:var(--text);font-size:.78rem;border-radius:2px;white-space:nowrap;
}
details.move .move-menu a:hover{background:rgba(235,192,71,.12);color:var(--gold-bright)}
details.move .move-menu a.cur{color:var(--gold-dim);pointer-events:none}
details.move .move-menu .lbl{
  display:block;padding:.2rem .55rem .35rem;font-size:.68rem;
  letter-spacing:.06em;text-transform:uppercase;color:var(--muted);
}
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
  if(q){
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
        /* Favorites keeps the + Folder header visible while filtering tiles. */
        if(sec.getAttribute("data-keep")!=="1")
          sec.classList.toggle("hidden",!any);
        var id=sec.getAttribute("id");
        if(id){
          var jump=document.querySelector('a.jump[href="#'+id+'"]');
          if(jump)jump.classList.toggle("hidden",!any);
        }
      });
    });
  }

  var modal=document.getElementById("fav-folder-modal");
  var openBtn=document.getElementById("fav-add-folder");
  var nameInput=document.getElementById("fav-folder-name");
  var cancelBtn=document.getElementById("fav-folder-cancel");
  var createBtn=document.getElementById("fav-folder-create");
  if(!modal||!openBtn||!nameInput||!createBtn)return;

  function closeModal(){
    modal.classList.add("hidden");
    nameInput.value="";
  }
  function submitFolder(){
    var name=(nameInput.value||"").trim();
    if(!name){nameInput.focus();return;}
    if(name.length>47)name=name.slice(0,47);
    location.search="?gw2igh-fav-folder-create="+encodeURIComponent(name);
  }
  openBtn.addEventListener("click",function(e){
    e.preventDefault();
    modal.classList.remove("hidden");
    setTimeout(function(){nameInput.focus();},0);
  });
  if(cancelBtn)cancelBtn.addEventListener("click",function(e){
    e.preventDefault();closeModal();
  });
  createBtn.addEventListener("click",function(e){
    e.preventDefault();submitFolder();
  });
  nameInput.addEventListener("keydown",function(e){
    if(e.key==="Enter"){e.preventDefault();submitFolder();}
    if(e.key==="Escape"){e.preventDefault();closeModal();}
  });
  modal.addEventListener("click",function(e){
    if(e.target===modal)closeModal();
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

	void AppendTile(std::string& html, const SiteDef& s, const std::string& pathBlurb,
		bool withFolderMove, int currentFolderId)
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
		html += "</a>";
		if (withFolderMove && Sites::FavoriteFolderCount() > 0)
		{
			html += "<details class=\"move\"><summary title=\"Move to folder\">⇄</summary>"
				"<div class=\"move-menu\"><span class=\"lbl\">Move to</span>";
			auto AppendMoveLink = [&](int folderId, const char* name) {
				html += "<a class=\"";
				if (folderId == currentFolderId)
					html += "cur";
				html += "\" href=\"?gw2igh-fav-folder-move=";
				html += Esc(id);
				html += "&amp;to=";
				html += std::to_string(folderId);
				html += "\">";
				html += Esc(name ? name : "Folder");
				html += "</a>";
			};
			AppendMoveLink(0, "Unfiled");
			const int folderN = Sites::FavoriteFolderCount();
			for (int fi = 0; fi < folderN; ++fi)
			{
				const int fid = Sites::FavoriteFolderIdAt(fi);
				AppendMoveLink(fid, Sites::FavoriteFolderName(fid));
			}
			html += "</div></details>";
		}
		html += "<a class=\"tile\" href=\"?gw2igh-open-site=";
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

	void AppendTile(std::string& html, const SiteDef& s, const std::string& pathBlurb)
	{
		AppendTile(html, s, pathBlurb, false, 0);
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
		"Star sites to pin them, create folders with <strong>+ Folder</strong>, "
		"then tap <strong>⇄</strong> on a favorite to move it.</p>"
		"<input class=\"search\" id=\"q\" type=\"search\" placeholder=\"Filter favorites &amp; categories…\" "
		"autocomplete=\"off\"/>"
		"</header>";

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
			html += "<h3>";
			html += Esc(fname ? fname : "Folder");
			html += " (";
			html += std::to_string(count);
			html += ")</h3><div class=\"grid\">";
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
