#include "LivePanelsBuildBrowseHubInternal.h"

#include "AddonPaths.h"
#include "HelperThemeCss.h"
#include "Sites.h"
#include "UiChrome.h"

#include <cctype>
#include <string>

namespace LivePanelsBuild
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
	static std::string s;
	static const char* out = nullptr;
	if (!out)
	{
		s = HelperThemeCss::RootVars();
		s += HelperThemeCss::ImmersiveShell();
		s += HelperThemeCss::FillBackgroundCss(
			UiChrome::FillFileUrl(AddonPaths::DataDir()).c_str());
		s += R"CSS(
.wrap{max-width:1100px;margin:0 auto;padding:28px 22px 96px;min-height:100vh}
.hero{
  margin-bottom:22px;padding:1.1rem 1.15rem 1.2rem;
  background:linear-gradient(165deg,rgba(48,38,22,.4),transparent 55%),var(--panel-inset);
  border:1px solid var(--border);
  box-shadow:inset 0 1px 0 rgba(255,230,160,.1),0 10px 32px rgba(0,0,0,.4);
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
.toc{
  display:flex;flex-wrap:wrap;gap:.45rem;margin:16px 0 0;padding:12px 0 2px;
  position:sticky;top:0;z-index:5;
  background:linear-gradient(180deg,rgba(14,11,8,.98) 70%,rgba(14,11,8,.88));
  border-bottom:1px solid var(--border-soft);
}
a.jump{
  display:inline-block;padding:.38rem .75rem;font-size:.76rem;font-weight:650;
  letter-spacing:.04em;text-decoration:none;text-transform:uppercase;
  color:var(--gold-dim);border:1px solid var(--border-deep);background:var(--accent);
}
a.jump:hover{color:var(--gold-bright);border-color:var(--gold);
  box-shadow:inset 0 0 0 1px rgba(232,196,112,.15)}
.sec{margin-top:1.85rem;scroll-margin-top:3.25rem}
.sec-head{display:flex;align-items:center;gap:.75rem;flex-wrap:wrap;margin:0 0 .85rem}
.sec-head h2,.sec h2{
  margin:0;font-size:.92rem;letter-spacing:.12em;text-transform:uppercase;
  color:var(--gold-bright);border-left:3px solid var(--gold);padding-left:.7rem;
  font-family:var(--font-ui);
}
.sec h3{
  margin:1rem 0 .55rem;font-size:.86rem;letter-spacing:.05em;
  color:var(--muted);font-weight:600;
}
.fav-fold-head{display:flex;align-items:baseline;gap:.65rem;flex-wrap:wrap;margin:1.1rem 0 .45rem}
.fav-fold-head h3{
  margin:0;flex:1;min-width:8rem;font-size:.86rem;letter-spacing:.04em;
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
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:.85rem}
.tile-wrap{position:relative;display:flex;flex-direction:column;min-height:5.4rem}
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
  display:flex;flex-direction:column;justify-content:center;gap:.35rem;
  flex:1;min-height:5.4rem;padding:1.05rem 2.15rem 1.05rem 1.1rem;
  text-decoration:none;color:var(--text);
  border-left:3px solid var(--gold-dim);
  transition:border-color .15s,transform .12s,box-shadow .15s;
}
a.tile:hover{
  border-color:var(--gold);border-left-color:var(--gold-bright);
  transform:translateY(-2px);
  box-shadow:inset 0 1px 0 rgba(255,230,160,.16),0 12px 28px rgba(0,0,0,.5);
}
a.tile .name{
  font-size:1.05rem;font-weight:650;color:var(--gold-bright);
  font-family:var(--font-display);letter-spacing:.01em;
}
a.tile .blurb{font-size:.8rem;color:var(--muted);line-height:1.35}
a.tile .meta{font-size:.72rem;color:var(--gold-dim);margin-top:.15rem;letter-spacing:.03em}
.foot{margin-top:2.5rem;font-size:.78rem;color:var(--muted)}
.credit{
  margin:1.75rem 0 0;padding:1rem 0 3.5rem;
  border-top:1px solid var(--border-soft);
  font-size:.78rem;color:var(--gold-dim);
  letter-spacing:.06em;text-align:center;text-transform:uppercase;
}
.empty{margin:2rem 0;color:var(--muted)}
.hidden{display:none!important}
)CSS";
		out = s.c_str();
	}
	return out;
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

} // namespace LivePanelsBuild
