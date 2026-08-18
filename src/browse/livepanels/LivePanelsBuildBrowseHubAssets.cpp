#include "LivePanelsBuildBrowseHubInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "HelperThemeCss.h"
#include "LivePanelsInternal.h"
#include "Sites.h"
#include "UiChrome.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace LivePanelsBuild
{
std::string Esc(const std::string& s) { return HtmlEscape(s); }

void AppendBrowseHeroArt(std::string& html)
{
	const std::string art = UiChrome::NamedFileUrl(AddonPaths::DataDir(), "browse-hero.png");
	if (art.empty())
		return;
	html += "<img class=\"hero-art\" alt=\"\" src=\"";
	html += art;
	html += "\"/>";
}

std::string ToLower(std::string s)
{
	for (char& c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

const char* HubCss()
{
	static std::string s;
	static char sTheme[64]{};
	static char sVer[16]{};
	if (s.empty() || std::strcmp(sTheme, G::ThemeId) != 0 ||
		std::strcmp(sVer, LivePanelsDetail::kPanelVer) != 0)
	{
		std::snprintf(sTheme, sizeof(sTheme), "%s", G::ThemeId);
		std::snprintf(sVer, sizeof(sVer), "%s", LivePanelsDetail::kPanelVer);
		s = HelperThemeCss::RootVars();
		HelperThemeCss::AppendUserRoot(s);
		s += HelperThemeCss::ImmersiveShell();
		{
			const std::wstring dir = AddonPaths::DataDir();
			const std::string fill = UiChrome::FillFileUrl(dir);
			s += HelperThemeCss::FillBackgroundCss(fill.c_str());
			s += UiChrome::DecorCss(dir);
		}
		s += R"CSS(
.wrap{max-width:1100px;margin-left:auto;margin-right:auto;padding:28px 22px 96px}
.hero{
  position:relative;overflow:hidden;isolation:isolate;
  margin-bottom:22px;padding:1.1rem 1.15rem 1.2rem;
  min-height:11.5rem;
  background:linear-gradient(165deg,rgba(48,38,22,.4),transparent 55%),var(--panel-inset);
  border:1px solid var(--border);
  box-shadow:inset 0 1px 0 rgba(255,230,160,.1),0 10px 32px rgba(0,0,0,.4);
}
.hero-copy{position:relative;z-index:1;max-width:min(36rem,calc(100% - 11rem))}
.hero-art{
  position:absolute;right:0;bottom:0;top:0;
  width:min(46%,260px);height:100%;margin:0;padding:0;
  pointer-events:none;object-fit:contain;object-position:right bottom;
  opacity:.92;
  -webkit-mask-image:linear-gradient(90deg,transparent 0%,#000 22%);
  mask-image:linear-gradient(90deg,transparent 0%,#000 22%);
}
.hero .eyebrow{margin:0 0 8px}
h1{margin:0 0 8px;font-size:2.15rem}
.tag{margin:0;color:var(--muted);font-size:.98rem;max-width:42rem;font-family:var(--font-ui)}
.back{
  display:inline-block;margin:0 0 12px;color:var(--gold-dim);text-decoration:none;font-size:.9rem;
}
.back:hover{color:var(--gold-bright)}
.search{
  width:100%;max-width:28rem;margin:18px 0 0;height:2.65rem;
  border:1px solid var(--border);background:var(--accent);color:var(--text);
  padding:0 .95rem;font-size:.92rem;
  box-shadow:inset 0 1px 3px rgba(0,0,0,.45);
}
.search:focus{outline:1px solid var(--gold-dim);border-color:var(--gold)}
)CSS";
		/* Rest of hub styles (toc onward) — keep prior block. */
		s += R"CSS(
.toc{
  display:flex;flex-wrap:wrap;gap:.45rem;margin:16px 0 0;padding:0;
  position:sticky;top:0;z-index:5;
  background:transparent;
  border-bottom:none;
  width:fit-content;max-width:100%;
}
a.jump{
  display:inline-block;padding:.38rem .75rem;font-size:.76rem;font-weight:650;
  letter-spacing:.04em;text-decoration:none;text-transform:uppercase;
  color:var(--gold-dim);border:1px solid var(--border-deep);background:var(--accent);
  background-image:none;
}
a.jump:hover{color:var(--gold-bright);border-color:var(--gold);background-image:none;
  box-shadow:inset 0 0 0 1px rgba(232,196,112,.22)}
.sec{margin-top:1.85rem;scroll-margin-top:3.25rem}
.sec-head{display:flex;align-items:center;gap:.75rem;flex-wrap:wrap;margin:0 0 .85rem}
.sec-head h2,.sec h2{
  margin:0;font-size:.92rem;letter-spacing:.12em;text-transform:uppercase;
  color:var(--gold-bright);border-left:3px solid var(--gold);padding-left:.7rem;
  font-family:var(--font-ui);
}
.sec h3{
  /* Nested browsePath folders (Legendary Armor under Armory, Food attrs, …).
     Same language as h2 so they do not look like a leftover ImGui header. */
  margin:1.15rem 0 .7rem;font-size:.82rem;letter-spacing:.1em;
  text-transform:uppercase;color:var(--gold-dim);font-weight:650;
  border-left:2px solid var(--gold-dim);padding-left:.55rem;
  font-family:var(--font-ui);
}
.fav-fold-head{display:flex;align-items:baseline;gap:.65rem;flex-wrap:wrap;margin:0}
details.fav-fold{margin:1.1rem 0 0;border:none}
details.fav-fold>summary.fav-fold-head{
  cursor:pointer;list-style:none;margin:0 0 .45rem;
}
details.fav-fold>summary.fav-fold-head::-webkit-details-marker{display:none}
details.fav-fold>summary.fav-fold-head::before{
  content:"▾";flex:0 0 auto;width:1rem;color:var(--gold-dim);font-size:.85rem;
  transition:transform .15s ease;
}
details.fav-fold:not([open])>summary.fav-fold-head::before{transform:rotate(-90deg)}
.fold-title{
  flex:1;min-width:8rem;font-size:.86rem;letter-spacing:.04em;
  color:var(--muted);font-weight:600;
}
a.fold-del{
  font-size:.72rem;color:var(--muted);text-decoration:none;
  padding:.15rem .4rem;border:1px solid transparent;
}
a.fold-del:hover{color:#e8a0a0;border-color:rgba(180,80,80,.45);background:rgba(80,20,20,.25)}
button.btn-plus{
  appearance:none;cursor:pointer;margin:0;padding:.3rem .75rem;
  font:inherit;font-size:.76rem;font-weight:650;letter-spacing:.04em;text-transform:uppercase;
  color:var(--gold-bright);background:var(--accent);border:1px solid var(--border);
}
button.btn-plus:hover{border-color:var(--gold);color:var(--gold);
  box-shadow:0 0 12px rgba(232,196,112,.12)}
.modal-backdrop{
  position:fixed;inset:0;z-index:40;display:flex;align-items:center;justify-content:center;
  background:rgba(0,0,0,.62);
}
.modal{
  width:min(22rem,92vw);padding:1.2rem 1.25rem 1.1rem;
}
.modal h3{margin:0 0 .65rem;color:var(--gold-bright);font-size:1.05rem;font-family:var(--font-display)}
.modal .hint{margin:0 0 .85rem;font-size:.82rem;color:var(--muted)}
.modal input{
  width:100%;height:2.4rem;margin:0 0 1rem;padding:0 .75rem;
  border:1px solid var(--border);background:var(--accent);color:var(--text);
  font:inherit;font-size:.92rem;
}
.modal input:focus{outline:1px solid var(--gold-dim)}
.modal-actions{display:flex;gap:.55rem;justify-content:flex-end}
.modal-actions button{
  appearance:none;cursor:pointer;min-width:5.5rem;height:2.25rem;
  padding:0 .85rem;font:inherit;font-size:.85rem;font-weight:600;
  border:1px solid var(--border);color:var(--text);background:var(--accent);
}
.modal-actions button.primary{
  color:#1a1208;background:linear-gradient(180deg,var(--gold-bright),var(--gold-dim));
  border-color:var(--gold);
}
.modal-actions button:hover{border-color:var(--gold)}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:.85rem;align-items:stretch}
.grid-cats{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:.85rem;align-items:stretch}
.tile-wrap{position:relative;display:flex;flex-direction:column;min-height:6.1rem;height:100%}
a.star{
  position:absolute;top:.5rem;right:.55rem;z-index:2;
  width:1.6rem;height:1.6rem;line-height:1.55rem;text-align:center;
  text-decoration:none;font-size:1.05rem;
  color:var(--muted);background:rgba(0,0,0,.4);border:1px solid transparent;
}
a.star.on{color:var(--gold-bright);border-color:var(--border-soft)}
a.star:hover{color:var(--gold);background:rgba(0,0,0,.55)}
details.move{position:absolute;top:.5rem;left:.55rem;z-index:2}
details.move>summary{
  list-style:none;cursor:pointer;width:1.6rem;height:1.6rem;
  line-height:1.55rem;text-align:center;font-size:.85rem;font-weight:700;
  color:var(--muted);background:rgba(0,0,0,.4);border:1px solid transparent;
}
details.move>summary::-webkit-details-marker{display:none}
details.move>summary:hover,details.move[open]>summary{
  color:var(--gold-bright);border-color:var(--border);background:rgba(0,0,0,.55);
}
details.move .move-menu{
  position:absolute;left:0;top:1.85rem;min-width:9.5rem;padding:.35rem;
  background:var(--panel-solid);border:1px solid var(--border);
  box-shadow:0 10px 28px rgba(0,0,0,.5);
}
details.move .move-menu a{
  display:block;padding:.35rem .55rem;text-decoration:none;
  color:var(--text);font-size:.78rem;white-space:nowrap;
}
details.move .move-menu a:hover{background:rgba(240,199,97,.12);color:var(--gold-bright)}
details.move .move-menu a.cur{color:var(--gold-dim);pointer-events:none}
details.move .move-menu .lbl{
  display:block;padding:.2rem .55rem .35rem;font-size:.68rem;
  letter-spacing:.06em;text-transform:uppercase;color:var(--muted);
}
a.tile{
  /* Centered stack on flat grey of card-fill (ragged ink eats outer edges).
     Side padding clears ☆ / ⇄ overlays on favorites + category site tiles. */
  display:flex;flex-direction:column;align-items:center;justify-content:center;
  text-align:center;gap:.35rem;
  flex:1;height:100%;min-height:6.1rem;padding:1.45rem 1.9rem 1.6rem;
  box-sizing:border-box;
  text-decoration:none;color:var(--text);
  border-left:1px solid var(--gold-dim);
  transition:border-color .15s,transform .12s,box-shadow .15s;
}
a.tile:hover{
  border-color:var(--gold);border-left-color:var(--gold-bright);
  transform:translateY(-2px);
  box-shadow:inset 0 1px 0 rgba(255,230,160,.16),0 12px 28px rgba(0,0,0,.5);
}
a.tile > *{position:relative;z-index:1}
a.tile .name{
  font-size:1.22rem;font-weight:700;color:var(--gold-bright);
  font-family:var(--font-display);letter-spacing:.02em;line-height:1.2;
  overflow-wrap:normal;word-break:normal;hyphens:none;max-width:100%;
}
a.tile .blurb{
  font-size:.88rem;color:var(--muted);line-height:1.35;max-width:100%;
  display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;
}
a.tile .meta{
  font-size:.78rem;color:var(--gold-dim);margin-top:.15rem;letter-spacing:.03em;
  /* Stay in the content stack — never absolute-bottom on the dark fringe. */
}
/* Browse Categories — slightly larger title-only tiles. */
a.tile.tile-cat{
  min-height:5.9rem;padding:1.45rem 1.15rem 1.6rem;
}
a.tile.tile-cat .name{
  font-size:1.28rem;
}
a.tile.tile-cat .blurb{
  font-size:.9rem;
}
.foot{margin-top:2.5rem;font-size:.78rem;color:var(--muted)}
.credit{
  margin:1.75rem 0 0;padding:1rem 0 0;
  border-top:1px solid var(--border-soft);
  font-size:.78rem;color:var(--gold-dim);
  letter-spacing:.06em;text-align:center;text-transform:uppercase;
}
.credit-issue{
  margin:.4rem 0 0;padding:0;
  font-size:.78rem;color:var(--muted);
  letter-spacing:.02em;text-align:center;line-height:1.45;
}
.credit-donate{
  margin:.35rem 0 0;padding:0 0 3.5rem;
  font-size:.78rem;color:var(--muted);
  letter-spacing:.02em;text-align:center;line-height:1.45;
}
.credit-issue a,.credit-donate a{color:var(--gold-dim);text-decoration:underline;word-break:break-all}
.credit-issue a:hover,.credit-donate a:hover{color:var(--gold)}
.empty{margin:2rem 0;color:var(--muted)}
.hidden{display:none!important}
)CSS";
	}
	return s.c_str();
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
        document.querySelectorAll("details.fav-fold").forEach(function(fold){
          var anySub=false;
          fold.querySelectorAll("[data-q]").forEach(function(el){
            if(!el.classList.contains("hidden"))anySub=true;
          });
          var titleEl=fold.querySelector(".fold-title");
          var title=(titleEl&&titleEl.textContent)?titleEl.textContent.toLowerCase():"";
          var show=anySub||!needle||title.indexOf(needle)>=0;
          fold.classList.toggle("hidden",!show);
          if(anySub&&needle)fold.setAttribute("open","");
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

  function closeMoveMenus(except){
    document.querySelectorAll("details.move[open]").forEach(function(d){
      if(except&&d===except)return;
      d.removeAttribute("open");
    });
  }
  /* Accidental ⇄ open: click outside or Esc dismisses the move menu. */
  document.addEventListener("click",function(e){
    var t=e.target;
    var open=null;
    if(t&&t.closest)open=t.closest("details.move");
    closeMoveMenus(open);
  });
  document.addEventListener("keydown",function(e){
    if(e.key==="Escape")closeMoveMenus(null);
  });

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
    closeMoveMenus(null);
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

  document.querySelectorAll("details.fav-fold").forEach(function(fold){
    var id=fold.getAttribute("data-fold");
    if(!id)return;
    var key="gw2igh-fav-fold-"+id;
    try{
      if(localStorage.getItem(key)==="0")fold.removeAttribute("open");
      fold.addEventListener("toggle",function(){
        localStorage.setItem(key,fold.open?"1":"0");
      });
    }catch(e){}
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

bool BrowseHubShowsCategory(const char* cat)
{
	if (!cat || !cat[0])
		return false;
	static const char* kOrder[] = {
		"Builds", "Guides", "Tools", "Help", "Search", "Discord",
	};
	for (const char* want : kOrder)
	{
		if (std::strcmp(cat, want) == 0)
			return true;
	}
	return false;
}

} // namespace LivePanelsBuild
