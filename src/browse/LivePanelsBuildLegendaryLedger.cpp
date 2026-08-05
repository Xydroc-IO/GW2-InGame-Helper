#include "LivePanelsBuildShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

/* Embedded compact legendaries catalog (ld -r -b binary). */
extern "C" {
	extern const unsigned char _binary_build_legendaries_catalog_json_start[];
	extern const unsigned char _binary_build_legendaries_catalog_json_end[];
}

namespace LivePanelsBuild
{
namespace
{
	std::string EmbeddedCatalogJson()
	{
		const unsigned char* begin = _binary_build_legendaries_catalog_json_start;
		const unsigned char* end = _binary_build_legendaries_catalog_json_end;
		if (!begin || !end || end <= begin)
			return {};
		return std::string(reinterpret_cast<const char*>(begin),
			static_cast<size_t>(end - begin));
	}

	/* Extract the "items":[...] array as a JSON substring for CEF. */
	std::string ExtractItemsArray(const std::string& catalog)
	{
		const size_t key = catalog.find("\"items\"");
		if (key == std::string::npos)
			return "[]";
		size_t br = catalog.find('[', key);
		if (br == std::string::npos)
			return "[]";
		int depth = 0;
		for (size_t i = br; i < catalog.size(); ++i)
		{
			if (catalog[i] == '[')
				++depth;
			else if (catalog[i] == ']')
			{
				--depth;
				if (depth == 0)
					return catalog.substr(br, i - br + 1);
			}
		}
		return "[]";
	}

	void ParseOwnedArmory(const std::string& body, std::unordered_map<int, int>& owned)
	{
		owned.clear();
		size_t p = 0;
		while (p < body.size())
		{
			size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos)
				break;
			long long id = JsonIntAfterKey(body, "id", brace);
			long long cnt = JsonIntAfterKey(body, "count", brace);
			if (id > 0)
				owned[static_cast<int>(id)] = cnt > 0 ? static_cast<int>(cnt) : 0;
			p = end + 1;
		}
	}

	std::string OwnedJson(const std::unordered_map<int, int>& owned)
	{
		std::string out = "{";
		bool first = true;
		for (const auto& kv : owned)
		{
			if (!first)
				out += ',';
			first = false;
			char buf[48];
			std::snprintf(buf, sizeof(buf), "\"%d\":%d", kv.first, kv.second);
			out += buf;
		}
		out += '}';
		return out;
	}

	const char* LedgerCss()
	{
		return R"CSS(
:root{--bg:#0b0a10;--text:#e4e4e7;--muted:#71717a;--muted-2:#52525b;--zinc-100:#f4f4f5;--zinc-200:#e4e4e7;--zinc-500:#71717a;--zinc-600:#52525b;--purple-200:#e9d5ff;--purple-300:#d8b4fe;--purple:#a855f7;--border:rgba(255,255,255,0.05);--panel:rgba(255,255,255,0.03);--ok:#4ade80;--miss:#f87171;--part:#fbbf24}
*{box-sizing:border-box}html,body{margin:0;min-height:100%}body{font-family:ui-sans-serif,system-ui,"Segoe UI",sans-serif;background:var(--bg);color:var(--text);line-height:1.5}
.glow{pointer-events:none;position:fixed;inset:0;background:radial-gradient(ellipse at top,rgba(139,92,246,.14),transparent 55%);z-index:0}
.shell{position:relative;z-index:1;min-height:100vh}header.top{position:sticky;top:0;z-index:20;border-bottom:1px solid var(--border);background:rgba(11,10,16,.8);backdrop-filter:blur(20px)}
.top-inner,main{max-width:56rem;margin:0 auto;padding-left:1.25rem;padding-right:1.25rem}
.top-inner{display:flex;align-items:center;gap:.625rem;padding-top:1rem;padding-bottom:1rem}
.brand{display:inline-flex;align-items:center;gap:.5rem;color:var(--zinc-100);text-decoration:none;font-size:.8125rem;font-weight:600;letter-spacing:.06em;text-transform:uppercase}
.brand:hover{color:var(--purple-200)}main{padding-top:1.5rem;padding-bottom:5rem}
footer.credit{max-width:56rem;margin:0 auto;padding:0 1.25rem 2.5rem;font-size:.75rem;color:var(--zinc-600)}footer.credit strong{color:var(--zinc-500)}
h1{margin:0;font-size:1.25rem;font-weight:600;letter-spacing:-.025em;color:var(--zinc-100)}
.search{width:100%;height:2.75rem;border:1px solid var(--border);border-radius:.375rem;background:var(--panel);padding:0 .75rem 0 2.5rem;color:var(--zinc-100);font-size:.875rem}
.search-wrap{position:relative}.search-wrap svg{position:absolute;left:.875rem;top:50%;width:1rem;height:1rem;transform:translateY(-50%);color:var(--zinc-500);pointer-events:none}
.cats,.letters{display:flex;flex-wrap:wrap;gap:.5rem}.cat,.letter{border:1px solid var(--border);background:var(--panel);color:var(--zinc-500);border-radius:.375rem;padding:.375rem .75rem;font-size:.75rem;cursor:pointer}
.cat.active,.letter:hover{color:var(--purple-200);border-color:rgba(168,85,247,.4)}.cat.active{background:rgba(168,85,247,.12)}
.alpha-bar{display:flex;align-items:center;justify-content:space-between;gap:1rem;flex-wrap:wrap}.count{font-size:.75rem;color:var(--zinc-500)}
.letter-sec{margin-top:2rem}.letter-head{display:flex;align-items:center;gap:.75rem;margin-bottom:.75rem}.letter-head h2{margin:0;font-size:1.125rem;color:var(--purple-300)}.rule{flex:1;height:1px;background:var(--border)}.letter-head .n{font-size:.75rem;color:var(--zinc-500)}
.grid{display:grid;gap:.5rem}a.card{display:flex;align-items:center;gap:.75rem;padding:.75rem;border:1px solid var(--border);border-radius:.5rem;background:var(--panel);text-decoration:none;color:inherit}
a.card:hover{border-color:rgba(168,85,247,.35)}.avatar{width:2.25rem;height:2.25rem;border-radius:.375rem;background:rgba(168,85,247,.15);color:var(--purple-300);display:flex;align-items:center;justify-content:center;font-weight:600;flex-shrink:0}
.avatar.lg{width:3rem;height:3rem;font-size:1.25rem}.meta{flex:1;min-width:0}.name{margin:0;font-size:.875rem;font-weight:600;color:var(--zinc-100)}.sub{margin:0;font-size:.75rem;color:var(--zinc-500)}
.badge{font-size:.65rem;font-weight:700;letter-spacing:.04em;text-transform:uppercase;padding:.2rem .45rem;border-radius:.25rem;flex-shrink:0}
.badge.owned{background:rgba(74,222,128,.15);color:var(--ok)}.badge.missing{background:rgba(248,113,113,.12);color:var(--miss)}.badge.partial{background:rgba(251,191,36,.12);color:var(--part)}.badge.unknown{background:rgba(113,113,122,.15);color:var(--zinc-500)}
.chev{width:1rem;height:1rem;color:var(--zinc-600);flex-shrink:0}.back{display:inline-flex;align-items:center;gap:.35rem;color:var(--purple-300);text-decoration:none;font-size:.875rem;margin-bottom:1rem}
.detail-head{display:flex;gap:1rem;align-items:center;margin-bottom:1.25rem}.detail-sub{margin:.25rem 0 0;color:var(--zinc-500);font-size:.875rem}
.detail-card{border:1px solid var(--border);border-radius:.5rem;background:var(--panel);padding:1rem}.field{margin-bottom:.85rem}.lbl{margin:0 0 .25rem;font-size:.7rem;letter-spacing:.06em;text-transform:uppercase;color:var(--zinc-500)}.val{margin:0;color:var(--zinc-200);font-size:.875rem}
.cta{display:inline-block;margin-top:.75rem;margin-right:.5rem;padding:.55rem 1rem;border-radius:.375rem;background:rgba(168,85,247,.2);border:1px solid rgba(168,85,247,.45);color:var(--purple-200);text-decoration:none;font-size:.875rem;font-weight:600}
.cta:hover{background:rgba(168,85,247,.3)}.cta.ghost{background:transparent;border-color:var(--border);color:var(--zinc-200)}.cta.ghost:hover{border-color:rgba(168,85,247,.4);color:var(--purple-200)}
.note{margin:0 0 1rem;font-size:.8rem;color:var(--zinc-500)}.empty{color:var(--zinc-500);padding:2rem 0;text-align:center}.hidden{display:none!important}
.space-y-3>*+*{margin-top:.75rem}.space-y-7>*+*{margin-top:1.75rem}.space-y-8>*+*{margin-top:2rem}
)CSS";
	}

	const char* LedgerJs()
	{
		return R"JS(
(function(){
var CATS=["All","Weapon","Armor","Trinket","Back Item","Sigil","Rune"];
var items=window.LEGENDARIES||[];
var owned=window.ARMORY_OWNED||{};
var hasKey=!!window.GW2IGH_HAS_KEY;
var state={q:"",cat:"All",missingOnly:false};
var elList=document.getElementById("view-list");
var elDetail=document.getElementById("view-detail");
function esc(s){return String(s==null?"":s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");}
function ownStatus(it){
  var ids=it.itemIds||[];
  if(!ids.length) return hasKey?"unknown":"unknown";
  if(!hasKey) return "unknown";
  var got=0,need=0;
  for(var i=0;i<ids.length;i++){
    var id=ids[i];
    var max=(it.maxCounts&&it.maxCounts[i])||1;
    need+=max;
    var c=owned[id];
    if(c==null) c=0;
    got+=Math.min(c,max);
  }
  if(got<=0) return "missing";
  if(got>=need) return "owned";
  return "partial";
}
function badgeHtml(st){
  if(st==="owned") return '<span class="badge owned">Owned</span>';
  if(st==="missing") return '<span class="badge missing">Missing</span>';
  if(st==="partial") return '<span class="badge partial">Partial</span>';
  return hasKey?'<span class="badge unknown">—</span>':'<span class="badge unknown">API</span>';
}
function subLine(it){var p=[];if(it.item_type)p.push(it.item_type);else if(it.category)p.push(it.category);if(it.generation&&it.generation!=="Other")p.push(it.generation);return p.join(" · ");}
function detailMeta(it){var p=[it.item_type,it.category];if(it.generation&&it.generation!=="Other")p.push(it.generation);return p.filter(Boolean).join(" · ");}
function filtered(){
  var q=state.q.trim().toLowerCase();
  return items.filter(function(it){
    if(state.cat!=="All"&&it.category!==state.cat)return false;
    if(state.missingOnly&&ownStatus(it)!=="missing"&&ownStatus(it)!=="partial")return false;
    if(!q)return true;
    return (it.name||"").toLowerCase().indexOf(q)>=0||(it.item_type||"").toLowerCase().indexOf(q)>=0;
  }).slice().sort(function(a,b){return a.name.localeCompare(b.name);});
}
function groupByLetter(list){
  var map={};list.forEach(function(it){var L=((it.name||"#")[0]||"#").toUpperCase();(map[L]=map[L]||[]).push(it);});
  return Object.keys(map).sort().map(function(L){return{letter:L,items:map[L]};});
}
function craftId(it){var ids=it.itemIds||[];return ids.length?ids[0]:0;}
function wikiNewTabHref(name){
  if(!name) return "";
  var path="";
  for(var i=0;i<name.length;i++){
    var c=name.charAt(i);
    if(c===" ") path+="_";
    else if(c==="'") path+="%27";
    else path+=c;
  }
  return "?gw2igh-newtab="+encodeURIComponent("https://wiki.guildwars2.com/wiki/"+path);
}
function craftPct(it){
  var ids=it.itemIds||[];
  var pcts=window.CRAFT_PCT||{};
  var best=-1;
  for(var i=0;i<ids.length;i++){
    var p=pcts[ids[i]];
    if(typeof p==="number"&&p>best)best=p;
  }
  return best;
}
function pctBadge(it){
  var p=craftPct(it);
  if(p<0) return "";
  return '<span class="badge '+(p>=100?"owned":(p>0?"partial":"missing"))+'">'+p+"%</span>";
}
function renderList(){
  var groups=groupByLetter(filtered());
  var total=groups.reduce(function(n,g){return n+g.items.length;},0);
  var catsHtml=CATS.map(function(c){return '<button type="button" class="cat'+(c===state.cat?" active":"")+'" data-cat="'+esc(c)+'">'+esc(c)+"</button>";}).join("");
  catsHtml+='<button type="button" class="cat'+(state.missingOnly?" active":"")+'" data-missing="1">Missing</button>';
  var lettersHtml=groups.map(function(g){return '<button type="button" class="letter" data-jump="'+esc(g.letter)+'">'+esc(g.letter)+"</button>";}).join("");
  var body;
  if(!items.length) body='<div class="empty">Catalog unavailable.</div>';
  else if(total===0) body='<div class="empty">No legendaries match your search.</div>';
  else{
    body='<div class="alpha-bar"><div class="letters">'+lettersHtml+'</div><span class="count">'+total+" items</span></div><div class=\"space-y-8\">"+
      groups.map(function(g){
        return '<section class="letter-sec" id="letter-'+esc(g.letter)+'"><div class="letter-head"><h2>'+esc(g.letter)+'</h2><div class="rule"></div><span class="n">'+g.items.length+'</span></div><div class="grid">'+
          g.items.map(function(it){
            var st=ownStatus(it);
            return '<a class="card" href="#/legendary/'+encodeURIComponent(it.id)+'"><div class="avatar">'+esc((it.name||"?")[0])+'</div><div class="meta"><p class="name">'+esc(it.name)+'</p><p class="sub">'+esc(subLine(it))+'</p></div>'+pctBadge(it)+badgeHtml(st)+'<svg class="chev" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="m9 18 6-6-6-6"/></svg></a>';
          }).join("")+"</div></section>";
      }).join("")+"</div>";
  }
  var keyNote=hasKey?'<p class="note">Owned / Missing refreshes automatically when you open the Ledger. Use <strong>Sync craft tree</strong> for gifts → mats have/need and %. <strong>Wiki</strong> opens the item page in a new helper tab.</p>':'<p class="note">Add a GW2 API key in Settings (unlocks + inventories) for Owned / Missing and craft have/need.</p>';
  elList.innerHTML="<div><h1>The Complete GW2 Legendary Collection</h1></div>"+keyNote+'<div class="space-y-3"><div class="search-wrap"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><path d="m21 21-4.3-4.3"/></svg><input class="search" id="q" type="search" placeholder="Search legendaries…" value="'+esc(state.q)+'"/></div><div class="cats">'+catsHtml+"</div></div>"+body;
  var q=document.getElementById("q");
  if(q){q.addEventListener("input",function(){state.q=q.value;renderList();var a=document.getElementById("q");if(a){a.focus();try{a.setSelectionRange(a.value.length,a.value.length);}catch(e){}}});}
  elList.querySelectorAll("[data-cat]").forEach(function(btn){btn.addEventListener("click",function(){state.cat=btn.getAttribute("data-cat");state.missingOnly=false;renderList();});});
  elList.querySelectorAll("[data-missing]").forEach(function(btn){btn.addEventListener("click",function(){state.missingOnly=!state.missingOnly;renderList();});});
  elList.querySelectorAll("[data-jump]").forEach(function(btn){btn.addEventListener("click",function(){var t=document.getElementById("letter-"+btn.getAttribute("data-jump"));if(t)t.scrollIntoView({behavior:"smooth",block:"start"});});});
}
function renderDetail(id){
  var it=null;for(var i=0;i<items.length;i++){if(items[i].id===id){it=items[i];break;}}
  if(!it){elDetail.innerHTML='<p class="empty">Legendary not found.</p>';return;}
  var st=ownStatus(it);
  var cid=craftId(it);
  var p=craftPct(it);
  var fields="";
  fields+='<div class="field"><p class="lbl">Armory</p><p class="val">'+badgeHtml(st);
  if((it.itemIds||[]).length){
    var bits=[];
    for(var j=0;j<it.itemIds.length;j++){
      var iid=it.itemIds[j],max=(it.maxCounts&&it.maxCounts[j])||1,c=owned[iid];
      if(c==null)c=hasKey?0:"?";
      bits.push("#"+iid+" "+c+"/"+max);
    }
    fields+=" · "+esc(bits.join(", "));
  }
  fields+="</p></div>";
  if(p>=0)fields+='<div class="field"><p class="lbl">Craft progress</p><p class="val">'+p+"% (from last Sync)</p></div>";
  if(it.acquisition)fields+='<div class="field"><p class="lbl">How to obtain</p><p class="val">'+esc(it.acquisition)+"</p></div>";
  if(it.notes)fields+='<div class="field"><p class="lbl">Notes</p><p class="val">'+esc(it.notes)+"</p></div>";
  var ctas="";
  var wiki=wikiNewTabHref(it.name);
  if(wiki) ctas+='<a class="cta ghost" href="'+wiki+'">Wiki</a>';
  if(cid){
    ctas+='<a class="cta" href="?gw2igh-leg-open='+cid+'">Sync craft tree</a>';
    ctas+='<a class="cta" href="?gw2igh-craft-plan='+cid+'">Open in Account Crafting</a>';
  }else{
    ctas+='<p class="note">No GW2 item id mapped — cannot sync craft tree yet.</p>';
  }
  elDetail.innerHTML='<a class="back" href="#/"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="16" height="16"><path d="m15 18-6-6 6-6"/></svg> All legendaries</a><div class="detail-head"><div class="avatar lg">'+esc((it.name||"?")[0])+'</div><div><h1>'+esc(it.name)+'</h1><p class="detail-sub">'+esc(detailMeta(it))+"</p></div></div><div class=\"detail-card\">"+fields+ctas+"</div>";
}
function route(){
  var hash=(location.hash||"#/").replace(/^#/,"");
  if(hash.charAt(0)!=="/")hash="/"+hash;
  var m=hash.match(/^\/legendary\/([^/?#]+)/);
  if(m){elList.classList.add("hidden");elDetail.classList.remove("hidden");renderDetail(decodeURIComponent(m[1]));window.scrollTo(0,0);}
  else{elDetail.classList.add("hidden");elList.classList.remove("hidden");renderList();}
}
window.addEventListener("hashchange",route);route();
})();
)JS";
	}
} // namespace

std::string BuildLegendaryLedgerHtml(const std::wstring& addonDir, const char* apiKey)
{
	const bool hasKey = apiKey && apiKey[0];
	std::unordered_map<int, int> owned;
	if (hasKey)
	{
		/* Always hit the live armory endpoint — Owned/Missing must not wait on Sync. */
		Gw2Http::Result acc = Gw2Http::Api("/v2/account/legendaryarmory", apiKey, kLiveHttpTimeoutMs);
		StoreCache(addonDir, "live-acc-armory", acc);
		PreferStaleCache(addonDir, "live-acc-armory", acc);
		if (acc.ok)
			ParseOwnedArmory(acc.body, owned);
	}

	const std::string catalog = EmbeddedCatalogJson();
	const std::string itemsJson = ExtractItemsArray(catalog);

	/* Load cached craft % from prior detail Syncs. */
	std::string craftPctJson = "{";
	bool firstPct = true;
	size_t search = 0;
	while (search < itemsJson.size())
	{
		size_t key = itemsJson.find("\"itemIds\"", search);
		if (key == std::string::npos)
			break;
		size_t br = itemsJson.find('[', key);
		if (br == std::string::npos)
			break;
		size_t end = itemsJson.find(']', br);
		if (end == std::string::npos)
			break;
		const std::string arr = itemsJson.substr(br + 1, end - br - 1);
		size_t i = 0;
		while (i < arr.size())
		{
			while (i < arr.size() && (arr[i] < '0' || arr[i] > '9'))
				++i;
			int id = 0;
			bool any = false;
			while (i < arr.size() && arr[i] >= '0' && arr[i] <= '9')
			{
				any = true;
				id = id * 10 + (arr[i] - '0');
				++i;
			}
			if (any && id > 0)
			{
				char stem[64];
				std::snprintf(stem, sizeof(stem), "live-leg-craft-%d", id);
				std::string cached = ReadUtf8File(StemPath(addonDir, stem, L".json"));
				if (!cached.empty())
				{
					long long pct = JsonIntAfterKey(cached, "pct", 0);
					if (!firstPct)
						craftPctJson += ',';
					firstPct = false;
					char buf[48];
					std::snprintf(buf, sizeof(buf), "\"%d\":%lld", id, pct);
					craftPctJson += buf;
				}
			}
		}
		search = end + 1;
	}
	craftPctJson += '}';

	std::string html;
	html.reserve(catalog.size() + 24000);
	html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
		"<title>The Complete GW2 Legendary Collection</title><style>";
	html += LedgerCss();
	html += "</style></head><body><div class=\"glow\" aria-hidden=\"true\"></div>"
		"<div class=\"shell\"><header class=\"top\"><div class=\"top-inner\">"
		"<a class=\"brand\" href=\"?gw2igh-leg-vault=1\" title=\"Refresh Legendary Ledger\">"
		"<span>GW2 Legendary Ledger</span></a></div></header>"
		"<main><div id=\"view-list\" class=\"space-y-7\"></div>"
		"<div id=\"view-detail\" class=\"hidden\"></div></main>"
		"<footer class=\"credit\">Idea credit: <strong>Dark Sorcerer.6420</strong> · "
		"Armory + craft sync via API key</footer></div><script>\n";
	html += "window.LEGENDARIES = ";
	html += itemsJson;
	html += ";\nwindow.ARMORY_OWNED = ";
	html += OwnedJson(owned);
	html += ";\nwindow.CRAFT_PCT = ";
	html += craftPctJson;
	html += ";\nwindow.GW2IGH_HAS_KEY = ";
	html += hasKey ? "true" : "false";
	html += ";\n";
	html += LedgerJs();
	html += "\n</script></body></html>";
	return html;
}

} // namespace LivePanelsBuild
