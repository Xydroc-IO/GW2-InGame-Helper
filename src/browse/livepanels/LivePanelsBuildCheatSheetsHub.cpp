#include "LivePanelsBuildShared.h"

#include "AddonPaths.h"
#include "HelperThemeCss.h"
#include "Sites.h"
#include "UiChrome.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace LivePanelsBuild
{
namespace
{
	struct HubEntry
	{
		std::string id;
		std::string label;
		std::string title;
		std::string section;
	};

	const char* HubCss()
	{
		static std::string s;
		static const char* out = nullptr;
		if (!out)
		{
			s = HelperThemeCss::RootVars();
			s += HelperThemeCss::ImmersiveShell();
			{
				const std::string fill = UiChrome::FillFileUrl(AddonPaths::DataDir());
				s += HelperThemeCss::FillBackgroundCss(fill.c_str());
			}
			s += R"CSS(
.wrap{max-width:960px;margin:0 auto;padding:28px 22px 72px}
.hero{
  margin-bottom:22px;padding:1.1rem 1.15rem 1.2rem;
  background:linear-gradient(165deg,rgba(48,38,22,.4),transparent 55%),var(--panel-inset);
  border:1px solid var(--border);
  box-shadow:inset 0 1px 0 rgba(255,230,160,.1),0 10px 32px rgba(0,0,0,.4);
}
h1{margin:0 0 8px;font-size:2.1rem}
.tag{margin:0;color:var(--muted);font-size:.98rem;max-width:38rem}
.search{
  width:100%;max-width:28rem;margin:18px 0 0;height:2.65rem;
  border:1px solid var(--border);background:var(--accent);color:var(--text);
  padding:0 .95rem;font-size:.92rem;box-shadow:inset 0 1px 3px rgba(0,0,0,.45);
}
.search:focus{outline:1px solid var(--gold-dim);border-color:var(--gold)}
.sec{margin-top:1.85rem}
.sec h2{
  margin:0 0 .85rem;font-size:.92rem;letter-spacing:.12em;text-transform:uppercase;
  color:var(--gold-bright);border-left:3px solid var(--gold);padding-left:.7rem;
}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:.85rem}
a.tile{
  display:flex;flex-direction:column;justify-content:center;gap:.35rem;
  min-height:5.4rem;padding:1.05rem 1.1rem;text-decoration:none;color:var(--text);
  border-left:3px solid var(--gold-dim);
  transition:border-color .15s,transform .12s,box-shadow .15s;
}
a.tile:hover{
  border-color:var(--gold);border-left-color:var(--gold-bright);
  transform:translateY(-2px);
  box-shadow:inset 0 1px 0 rgba(255,230,160,.16),0 12px 28px rgba(0,0,0,.5);
}
a.tile .name{font-size:1.05rem;font-weight:650;color:var(--gold-bright);font-family:var(--font-display)}
a.tile .blurb{font-size:.8rem;color:var(--muted);line-height:1.35}
.foot{margin-top:2.5rem;font-size:.78rem;color:var(--muted)}
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
  if(!q)return;
  q.addEventListener("input",function(){
    var needle=(q.value||"").trim().toLowerCase();
    document.querySelectorAll("[data-sec]").forEach(function(sec){
      var any=false;
      sec.querySelectorAll("a.tile").forEach(function(tile){
        var hay=(tile.getAttribute("data-q")||"");
        var show=!needle||hay.indexOf(needle)>=0;
        tile.classList.toggle("hidden",!show);
        if(show)any=true;
      });
      sec.classList.toggle("hidden",!any);
    });
  });
})();
)JS";
	}

	std::string Esc(const std::string& s) { return HtmlEscape(s); }
} // namespace

std::string BuildCheatSheetsHubHtml(const std::wstring& /*addonDir*/, const char* /*apiKey*/)
{
	size_t n = 0;
	const SiteDef* sites = Sites::All(&n);
	std::vector<HubEntry> entries;
	entries.reserve(32);
	for (size_t i = 0; i < n; ++i)
	{
		const SiteDef& s = sites[i];
		if (!s.category || std::strcmp(s.category, "Cheat Sheets") != 0)
			continue;
		/* Legendary Ledger has its own side-rail button. */
		if (s.id && std::strcmp(s.id, "legvault") == 0)
			continue;
		if (s.homeUrl && std::strcmp(s.homeUrl, "about:legendary-vault") == 0)
			continue;
		if (!s.id || !s.id[0] || !s.label || !s.label[0])
			continue;
		HubEntry e;
		e.id = s.id;
		e.label = s.label;
		e.title = (s.title && s.title[0]) ? s.title : s.label;
		if (s.browsePath && s.browsePath[0] && s.browsePath[0][0])
			e.section = s.browsePath[0];
		else
			e.section = "General";
		entries.push_back(std::move(e));
	}
	std::sort(entries.begin(), entries.end(),
		[](const HubEntry& a, const HubEntry& b) {
			if (a.section != b.section)
				return a.section < b.section;
			return a.label < b.label;
		});

	std::string html;
	html.reserve(16000);
	html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
		"<title>Cheat Sheets</title><style>";
	html += HubCss();
	html += "</style></head><body><div class=\"wrap\">"
		"<header class=\"hero\">"
		"<p class=\"eyebrow\">GW2 In-Game Helper</p>"
		"<h1>Cheat Sheets</h1>"
		"<p class=\"tag\">Offline reference only — food, fractals, legendaries, daily/weekly checklist, "
		"squad tools, and more. Live Vault / Today board use your API key elsewhere. Each sheet opens in a new tab.</p>"
		"<input class=\"search\" id=\"q\" type=\"search\" placeholder=\"Filter cheat sheets…\" "
		"autocomplete=\"off\"/>"
		"</header>";

	if (entries.empty())
	{
		html += "<p class=\"empty\">No cheat sheets found in the catalog.</p>";
	}
	else
	{
		std::string curSec;
		bool open = false;
		for (const HubEntry& e : entries)
		{
			if (e.section != curSec)
			{
				if (open)
					html += "</div></section>";
				curSec = e.section;
				html += "<section class=\"sec\" data-sec=\"1\"><h2>";
				html += Esc(curSec);
				html += "</h2><div class=\"grid\">";
				open = true;
			}
			std::string q = e.label + " " + e.title + " " + e.section + " " + e.id;
			for (char& c : q)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			html += "<a class=\"tile\" href=\"?gw2igh-open-site=";
			html += Esc(e.id);
			html += "\" data-q=\"";
			html += Esc(q);
			html += "\"><span class=\"name\">";
			html += Esc(e.label);
			html += "</span><span class=\"blurb\">";
			html += Esc(e.title);
			html += "</span></a>";
		}
		if (open)
			html += "</div></section>";
	}

	html += "<p class=\"foot\">Legendary Ledger is on the side rail under Notes. "
		"Sheets open in a <strong>new helper tab</strong>.</p>"
		"</div><script>";
	html += HubJs();
	html += "</script></body></html>";
	return html;
}

} // namespace LivePanelsBuild
