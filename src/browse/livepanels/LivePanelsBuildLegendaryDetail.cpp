#include "LivePanelsBuildShared.h"

#include "AddonPaths.h"
#include "CraftingPlanSnapshot.h"
#include "Gw2Http.h"
#include "HelperThemeCss.h"
#include "UiChrome.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#include <windows.h>

namespace LivePanelsBuild
{
namespace
{
	std::string Esc(const std::string& s)
	{
		return HtmlEscape(s);
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

	/* Match helper gold/dark chrome (Browse / Cheat Sheets hubs). */
	const char* LedgerDetailCss()
	{
		return R"CSS(
:root{--bg:#120e0a;--text:#f5eddb;--muted:#b8ad94;--muted-2:#9a8f78;--zinc-100:#f5eddb;--zinc-200:#ebe3d2;--zinc-500:#b8ad94;--zinc-600:#9a8f78;--purple-200:#ffe68c;--purple-300:#f0c761;--purple:#c29438;--border:rgba(240,199,97,.22);--panel:rgba(28,23,17,.94);--ok:#85b86b;--miss:#e88878;--part:#eb8c47}
*{box-sizing:border-box}html,body{margin:0;min-height:var(--app-h,100vh)}body{font-family:"Segoe UI",Tahoma,sans-serif;background:var(--bg);color:var(--text);line-height:1.5}
.glow{pointer-events:none;position:fixed;inset:0;background:radial-gradient(ellipse 80% 50% at 50% -10%,rgba(240,199,97,.16),transparent 55%);z-index:0}
.shell{position:relative;z-index:1;width:100%;box-sizing:border-box;min-height:var(--app-h,100vh);display:flex;flex-direction:column;justify-content:center;justify-content:safe center}
main{max-width:56rem;margin:0 auto;padding-left:1.25rem;padding-right:1.25rem}
main{padding-top:1.5rem;padding-bottom:5rem}
footer.credit{max-width:56rem;margin:0 auto;padding:0 1.25rem 2.5rem;font-size:.75rem;color:var(--zinc-600)}footer.credit strong{color:var(--zinc-500)}
.back{display:inline-flex;align-items:center;gap:.35rem;color:var(--purple-300);text-decoration:none;font-size:.875rem;margin-bottom:1rem}
.detail-head{display:flex;gap:1rem;align-items:center;margin-bottom:1.25rem}
.avatar{width:3rem;height:3rem;border-radius:.375rem;background:rgba(240,199,97,.15);color:var(--purple-300);display:flex;align-items:center;justify-content:center;font-weight:600;font-size:1.25rem;flex-shrink:0;overflow:hidden;padding:0}
.avatar img{width:100%;height:100%;object-fit:contain;display:block}
h1{margin:0;font-size:1.35rem;font-weight:600;letter-spacing:-.025em;color:#f0c65a}
.detail-sub{margin:.25rem 0 0;color:var(--zinc-500);font-size:.875rem}
.detail-card{border:1px solid var(--border);border-radius:.5rem;background:var(--panel);padding:1rem;margin-bottom:1rem}
.field{margin-bottom:.85rem}.lbl{margin:0 0 .25rem;font-size:.7rem;letter-spacing:.06em;text-transform:uppercase;color:var(--zinc-500)}.val{margin:0;color:var(--zinc-200);font-size:.875rem}
.badge{display:inline-block;font-size:.65rem;font-weight:700;letter-spacing:.04em;text-transform:uppercase;padding:.2rem .45rem;border-radius:.25rem}
.badge.owned{background:rgba(74,222,128,.15);color:var(--ok)}.badge.missing{background:rgba(248,113,113,.12);color:var(--miss)}
.bar{margin:.75rem 0;height:.5rem;background:rgba(255,255,255,.06);border-radius:999px;overflow:hidden}
.bar>i{display:block;height:100%;background:linear-gradient(90deg,var(--purple),var(--ok));border-radius:999px}
.pct{font-size:1.15rem;font-weight:700;color:var(--purple-200);margin:0}
.note{margin:0 0 1rem;font-size:.8rem;color:var(--zinc-500)}
.cta{display:inline-block;margin-top:.5rem;margin-right:.5rem;padding:.55rem 1rem;border-radius:.375rem;background:rgba(240,199,97,.18);border:1px solid rgba(240,199,97,.45);color:var(--purple-200);text-decoration:none;font-size:.875rem;font-weight:600}
.cta:hover{background:rgba(240,199,97,.28)}.cta.ghost{background:transparent;border-color:var(--border);color:var(--zinc-200)}.cta.ghost:hover{border-color:rgba(240,199,97,.5);color:var(--purple-200)}
.cta-row{display:flex;flex-wrap:wrap;gap:.5rem;margin:0 0 1.25rem}
.card-title{margin:0 0 .75rem;font-size:.7rem;letter-spacing:.06em;text-transform:uppercase;color:var(--zinc-500)}
ul.craft-tree,ul.craft-tree ul{list-style:none;margin:.35rem 0 0;padding-left:1rem}
ul.craft-tree{padding-left:0}
li .row{display:flex;justify-content:space-between;gap:1rem;padding:.28rem 0;border-bottom:1px solid var(--border);font-size:.84rem}
li.done .nm{color:var(--ok)}li.mat .nm{color:var(--zinc-200)}li.craft .nm{color:var(--purple-200)}
.qty{color:var(--muted);font-variant-numeric:tabular-nums;white-space:nowrap}
.pulse{display:inline-block;width:.55rem;height:.55rem;border-radius:999px;background:var(--purple);margin-right:.45rem;animation:p 1.1s ease-in-out infinite}
@keyframes p{0%,100%{opacity:.35}50%{opacity:1}}
.load-box{border:1px solid rgba(240,199,97,.35);border-radius:.5rem;background:rgba(240,199,97,.08);padding:1.25rem}
.load-box h2{margin:0 0 .5rem;font-size:1rem;color:var(--purple-200)}
)CSS";
	}

	std::string LedgerChromeOpen(const char* title)
	{
		std::string html;
		html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/>"
			"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
			"<title>";
		html += Esc(title);
		html += "</title><style>";
		html += LedgerDetailCss();
		{
			const std::wstring dir = AddonPaths::DataDir();
			const std::string fill = UiChrome::FillFileUrl(dir);
			html += HelperThemeCss::FillBackgroundCss(fill.c_str());
			html += UiChrome::DecorCss(dir);
			HelperThemeCss::AppendUserRoot(html);
		}
		html += "</style></head><body><div class=\"glow\" aria-hidden=\"true\"></div>"
			"<div class=\"shell\"><main>";
		return html;
	}

	std::string WikiNewTabHref(const std::string& name)
	{
		if (name.empty())
			return {};
		std::string wiki = "https://wiki.guildwars2.com/wiki/";
		for (unsigned char c : name)
		{
			if (c == ' ')
				wiki += '_';
			else if (c == '\'')
				wiki += "%27";
			else
				wiki.push_back(static_cast<char>(c));
		}
		std::string href = "?gw2igh-newtab=";
		for (unsigned char c : wiki)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~')
				href.push_back(static_cast<char>(c));
			else
			{
				char buf[8];
				std::snprintf(buf, sizeof(buf), "%%%02X", c);
				href += buf;
			}
		}
		return href;
	}

	std::string LedgerChromeClose()
	{
		std::string s = "</main><footer class=\"credit\">Idea credit: <strong>Dark Sorcerer.6420</strong> · "
			"Armory + craft sync via API key</footer></div>";
		s += HelperThemeCss::ViewportSyncJs();
		s += "</body></html>";
		return s;
	}
} // namespace

std::string BuildLegendaryDetailShellHtml(int itemId)
{
	char title[64];
	std::snprintf(title, sizeof(title), "Craft #%d", itemId > 0 ? itemId : 0);
	std::string html = LedgerChromeOpen(title);
	/* Auto-refresh so finishing the worker reloads even if CEF ignores same-URL Navigate. */
	html.insert(html.find("</title>") + 8,
		"<meta http-equiv=\"refresh\" content=\"2\"/>");
	html += "<a class=\"back\" href=\"?gw2igh-leg-vault=1\">"
		"<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" "
		"width=\"16\" height=\"16\"><path d=\"m15 18-6-6 6-6\"/></svg> All legendaries</a>";
	html += "<div class=\"detail-head\"><div class=\"avatar\">…</div><div><h1>";
	html += Esc(title);
	html += "</h1><p class=\"detail-sub\">Building craft tree in the background</p></div></div>";
	html += "<div class=\"load-box\"><h2><span class=\"pulse\" aria-hidden=\"true\"></span>"
		"Building craft tree</h2>"
		"<p class=\"note\" style=\"margin:0\">Looking up mystic forge gifts → mats against "
		"materials, bank, and shared inventory. This page refreshes when ready.</p></div>";
	html += LedgerChromeClose();
	return html;
}

std::string BuildLegendaryDetailHtml(const std::wstring& addonDir, const char* apiKey, int itemId)
{
	const bool hasKey = apiKey && apiKey[0];
	int armoryHave = -1;
	int armoryMax = 1;

	if (hasKey)
	{
		Gw2Http::Result acc = Gw2Http::Api("/v2/account/legendaryarmory", apiKey, kLiveHttpTimeoutMs);
		StoreCache(addonDir, "live-acc-armory", acc);
		PreferStaleCache(addonDir, "live-acc-armory", acc);
		if (acc.ok)
		{
			std::unordered_map<int, int> owned;
			ParseOwnedArmory(acc.body, owned);
			auto it = owned.find(itemId);
			armoryHave = (it != owned.end()) ? it->second : 0;
		}
	}

	CraftingPlanSnapshot::Progress snap;
	if (hasKey)
		snap = CraftingPlanSnapshot::Build(itemId);
	else
	{
		snap.outputId = itemId;
		snap.status = "Add a GW2 API key in Settings (inventories + unlocks) to sync mats.";
	}

	if (snap.ok)
	{
		char stem[64];
		std::snprintf(stem, sizeof(stem), "live-leg-craft-%d", itemId);
		std::string cache = "{\"pct\":";
		cache += std::to_string(snap.pct);
		cache += ",\"have\":";
		cache += std::to_string(snap.leafHave);
		cache += ",\"need\":";
		cache += std::to_string(snap.leafNeed);
		cache += ",\"name\":\"";
		for (char c : snap.outputName)
		{
			if (c == '"' || c == '\\')
				cache += '\\';
			cache += c;
		}
		cache += "\"}";
		WriteUtf8File(StemPath(addonDir, stem, L".json"), cache);
	}

	const std::string title = snap.outputName.empty()
		? ("Legendary #" + std::to_string(itemId))
		: snap.outputName;
	const char initial = title.empty() ? '?' : title[0];

	std::string iconUrl;
	{
		const std::string cached = ReadUtf8File(StemPath(addonDir, "live-leg-icons", L".json"));
		if (!cached.empty())
		{
			const std::string key = "\"" + std::to_string(itemId) + "\":\"";
			size_t pos = cached.find(key);
			if (pos != std::string::npos)
			{
				pos += key.size();
				size_t end = pos;
				while (end < cached.size() && cached[end] != '"')
				{
					if (cached[end] == '\\' && end + 1 < cached.size())
						end += 2;
					else
						++end;
				}
				if (end > pos)
					iconUrl = cached.substr(pos, end - pos);
			}
		}
		if (iconUrl.empty())
		{
			const std::string path = "/v2/items/" + std::to_string(itemId);
			Gw2Http::Result item = Gw2Http::Api(path.c_str(), nullptr, kLiveHttpTimeoutMs);
			if (item.ok)
			{
				iconUrl = JsonStringAfterKey(item.body, "icon", 0);
				if (iconUrl.rfind("https://", 0) != 0)
					iconUrl.clear();
			}
		}
	}

	std::string html = LedgerChromeOpen(title.c_str());
	html += "<a class=\"back\" href=\"?gw2igh-leg-vault=1\">"
		"<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" "
		"width=\"16\" height=\"16\"><path d=\"m15 18-6-6 6-6\"/></svg> All legendaries</a>";
	html += "<div class=\"detail-head\"><div class=\"avatar\">";
	if (!iconUrl.empty())
	{
		html += "<img src=\"";
		html += Esc(iconUrl);
		html += "\" alt=\"\"/>";
	}
	else
		html += Esc(std::string(1, initial));
	html += "</div><div><h1>";
	html += Esc(title);
	html += "</h1><p class=\"detail-sub\">#";
	html += std::to_string(itemId);
	if (!snap.recipeSource.empty())
	{
		html += " · ";
		html += Esc(snap.recipeSource);
	}
	html += "</p></div></div>";

	html += "<div class=\"cta-row\">";
	const std::string wikiHref = WikiNewTabHref(title);
	if (!wikiHref.empty())
	{
		html += "<a class=\"cta\" href=\"";
		html += wikiHref;
		html += "\">Open in Wiki</a>";
	}
	html += "<a class=\"cta\" href=\"?gw2igh-craft-plan=";
	html += std::to_string(itemId);
	html += "\">Open in Crafting</a></div>";

	html += "<div class=\"detail-card\">";

	if (armoryHave >= 0)
	{
		html += "<div class=\"field\"><p class=\"lbl\">Armory</p><p class=\"val\">";
		html += armoryHave > 0
			? "<span class=\"badge owned\">Owned ×"
			: "<span class=\"badge missing\">Missing";
		if (armoryHave > 0)
			html += std::to_string(armoryHave);
		html += "</span> · ";
		html += Esc(title);
		html += " ×";
		html += std::to_string(armoryHave);
		html += " (max ";
		html += std::to_string(armoryMax);
		html += ")</p></div>";
	}

	if (snap.ok)
	{
		html += "<div class=\"field\"><p class=\"lbl\">Craft progress</p>"
			"<p class=\"pct\">";
		html += std::to_string(snap.pct);
		html += "% complete</p><div class=\"bar\"><i style=\"width:";
		html += std::to_string(snap.pct);
		html += "%\"></i></div><p class=\"note\">Leaf mats/gifts: ";
		html += std::to_string(snap.leafHave);
		html += " / ";
		html += std::to_string(snap.leafNeed);
		html += " (materials, bank, shared inventory).</p></div>";
	}
	else
	{
		html += "<p class=\"note\">";
		html += Esc(snap.status);
		html += "</p>";
	}
	html += "</div>";

	html += "<div class=\"detail-card\"><p class=\"card-title\">Craft tree (have / need)</p>";
	if (snap.ok && !snap.treeHtml.empty())
		html += snap.treeHtml;
	else
		html += "<p class=\"note\">No craft tree yet — reopen this legendary to retry the wiki forge lookup.</p>";
	html += "</div>";
	html += LedgerChromeClose();
	return html;
}

} // namespace LivePanelsBuild
