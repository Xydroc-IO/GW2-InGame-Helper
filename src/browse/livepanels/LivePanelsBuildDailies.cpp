#include "LivePanelsBuildShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
std::string BuildDailiesHtml(const std::wstring& addonDir, const char* apiKey)
{
	const bool hasKey = apiKey && apiKey[0];
	std::string body;

	if (hasKey)
	{
		body += "<div class=\"keybox ok\" id=\"apikey\"><h3>API key connected</h3>"
			"<p>Showing <strong>live</strong> Wizard’s Vault progress for your account "
			"(same read-only endpoints Discord bots use). "
			"Change or clear the key in <strong>Settings</strong> (helper side rail), then Reload.</p></div>";
	}
	else
	{
		body += "<div class=\"keybox warn\" id=\"apikey\"><h3>Add your GW2 API key for live Vault</h3>"
			"<p>1. Open <strong>Settings</strong> from the helper side rail<br/>"
			"2. Paste a key from <strong>account.arena.net/applications</strong> with "
			"<strong>account</strong> + <strong>progression</strong><br/>"
			"3. Come back here and hit <strong>Reload</strong><br/>"
			"The key stays in this addon’s local settings.ini only — never sent to third parties.</p></div>";
	}

	/* Cache-first, then one parallel round for misses (typically 0–6 GETs). */
	Gw2Http::Result season, craft, bosses, vd, vw, vs, pub;
	const bool hitSeason = TryCacheHit(addonDir, "live-season", kSeasonTtlSec, season);
	const bool hitCraft = TryCacheHit(addonDir, "live-craft", kPublicTtlSec, craft);
	const bool hitBosses = TryCacheHit(addonDir, "live-bosses", kPublicTtlSec, bosses);
	bool hitVd = false, hitVw = false, hitVs = false, hitPub = false;
	if (hasKey)
	{
		hitVd = TryCacheHit(addonDir, "live-vault-daily", kAccountTtlSec, vd);
		hitVw = TryCacheHit(addonDir, "live-vault-weekly", kAccountTtlSec, vw);
		hitVs = TryCacheHit(addonDir, "live-vault-special", kAccountTtlSec, vs);
	}
	else
		hitPub = TryCacheHit(addonDir, "live-vault-obj", kPublicTtlSec, pub);

	ParallelApiJob jobs[8];
	size_t nJobs = 0;
	auto enqueue = [&](const char* path, const char* bearer, int timeoutMs, Gw2Http::Result* out) {
		if (nJobs >= 8 || !path || !out)
			return;
		jobs[nJobs].path = path;
		jobs[nJobs].bearer = bearer;
		jobs[nJobs].timeoutMs = timeoutMs;
		jobs[nJobs].out = out;
		++nJobs;
	};
	if (!hitSeason)
		enqueue("/v2/wizardsvault", nullptr, kLiveHttpTimeoutMs, &season);
	if (!hitCraft)
		enqueue("/v2/dailycrafting", nullptr, kLiveHttpTimeoutMs, &craft);
	if (!hitBosses)
		enqueue("/v2/worldbosses", nullptr, kLiveHttpTimeoutMs, &bosses);
	if (hasKey)
	{
		if (!hitVd)
			enqueue("/v2/account/wizardsvault/daily", apiKey, kLiveHttpTimeoutMs, &vd);
		if (!hitVw)
			enqueue("/v2/account/wizardsvault/weekly", apiKey, kLiveHttpTimeoutMs, &vw);
		if (!hitVs)
			enqueue("/v2/account/wizardsvault/special", apiKey, kLiveHttpTimeoutMs, &vs);
	}
	else if (!hitPub)
		enqueue("/v2/wizardsvault/objectives?ids=all", nullptr, kLiveBulkTimeoutMs, &pub);

	RunParallelApis(jobs, nJobs);

	if (!hitSeason) { StoreCache(addonDir, "live-season", season); PreferStaleCache(addonDir, "live-season", season); }
	if (!hitCraft) { StoreCache(addonDir, "live-craft", craft); PreferStaleCache(addonDir, "live-craft", craft); }
	if (!hitBosses) { StoreCache(addonDir, "live-bosses", bosses); PreferStaleCache(addonDir, "live-bosses", bosses); }
	if (hasKey)
	{
		if (!hitVd) { StoreCache(addonDir, "live-vault-daily", vd); PreferStaleCache(addonDir, "live-vault-daily", vd); }
		if (!hitVw) { StoreCache(addonDir, "live-vault-weekly", vw); PreferStaleCache(addonDir, "live-vault-weekly", vw); }
		if (!hitVs) { StoreCache(addonDir, "live-vault-special", vs); PreferStaleCache(addonDir, "live-vault-special", vs); }
	}
	else if (!hitPub)
	{
		StoreCache(addonDir, "live-vault-obj", pub);
		PreferStaleCache(addonDir, "live-vault-obj", pub);
	}

	body += "<section class=\"block\" id=\"season\"><div class=\"head\"><h2>Current season</h2>"
		"<p>/v2/wizardsvault</p></div><div class=\"body\">";
	if (season.ok)
	{
		std::string title = JsonStringAfterKey(season.body, "title");
		std::string start = JsonStringAfterKey(season.body, "start");
		std::string end = JsonStringAfterKey(season.body, "end");
		body += "<p class=\"t\" style=\"font-size:1.2rem;font-weight:700;color:var(--gold)\">";
		body += HtmlEscape(title.empty() ? "Wizard’s Vault" : title);
		body += "</p>";
		body += "<p class=\"s\">";
		body += SeasonDateBlurb(start, end);
		body += "</p>";
	}
	else
		body += "<p class=\"note\">Season fetch failed: " + HtmlEscape(season.error) + "</p>";
	body += "</div></section>\n";

	if (hasKey)
	{
		if (vd.ok)
			AppendVaultObjectives(body, "vault-daily", "Your daily Vault (live)", vd.body, true);
		else
			body += "<section class=\"block\" id=\"vault-daily\"><div class=\"head\"><h2>Your daily Vault (live)</h2></div>"
				"<div class=\"body\"><p class=\"note\">Failed: " +
				HtmlEscape(vd.error) + " (HTTP " + std::to_string(vd.status) +
				"). Confirm scopes <strong>account</strong> + <strong>progression</strong>, then Reload.</p></div></section>\n";
		if (vw.ok)
			AppendVaultObjectives(body, "vault-weekly", "Your weekly Vault (live)", vw.body, true);
		else
			body += "<section class=\"block\" id=\"vault-weekly\"><div class=\"head\"><h2>Your weekly Vault (live)</h2></div>"
				"<div class=\"body\"><p class=\"note\">Failed: " + HtmlEscape(vw.error) + "</p></div></section>\n";
		if (vs.ok)
			AppendVaultObjectives(body, "vault-special", "Your special Vault (live)", vs.body, true);
	}
	else
	{
		if (pub.ok && pub.body.find("\"title\"") != std::string::npos)
		{
			std::string easyOnly = "[";
			size_t i = 0;
			int easyN = 0;
			bool first = true;
			while (easyN < 60)
			{
				size_t obj = pub.body.find('{', i);
				if (obj == std::string::npos) break;
				size_t end = JsonObjectEnd(pub.body, obj);
				if (end == std::string::npos) break;
				std::string chunk = pub.body.substr(obj, end - obj + 1);
				long long ac = JsonIntAfterKey(chunk, "acclaim");
				if (ac > 0 && ac <= 10)
				{
					if (!first) easyOnly += ',';
					easyOnly += chunk;
					first = false;
					++easyN;
				}
				i = end + 1;
			}
			easyOnly += ']';
			AppendVaultObjectives(body, "vault-easy", "Easy Vault objectives (preview)", easyOnly, false);
		}
		else
		{
			body += "<section class=\"block\" id=\"vault-easy\"><div class=\"head\"><h2>Wizard’s Vault</h2></div>"
				"<div class=\"body\"><p class=\"note\">Add an API key above for live personal objectives, or Reload.</p></div></section>\n";
		}
	}

	if (craft.ok)
		AppendChecklistSection(body, "craft", "Daily crafting",
			"Today’s time-gated crafts — tick them off as you go.", craft.body);
	else
		body += "<section class=\"block\" id=\"craft\"><div class=\"head\"><h2>Daily crafting</h2></div>"
			"<div class=\"body\"><p class=\"note\">Could not load (" + HtmlEscape(craft.error) + ").</p></div></section>\n";

	if (bosses.ok)
		AppendChecklistSection(body, "bosses", "World bosses",
			"World-boss checklist for today — tick as you finish. Use GW2Timer for countdowns.", bosses.body);
	else
		body += "<section class=\"block\" id=\"bosses\"><div class=\"head\"><h2>World bosses</h2></div>"
			"<div class=\"body\"><p class=\"note\">Could not load (" + HtmlEscape(bosses.error) + ").</p></div></section>\n";

	body += "<section class=\"block\" id=\"links\"><div class=\"head\"><h2>Related</h2></div><div class=\"body\">"
		"<ul class=\"rows\">"
		"<li><a class=\"link\" href=\"about:daily-weekly\">Offline Daily / Weekly checklist</a></li>"
		"<li><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Wizard%27s_Vault/Easy_objectives\">Wiki — Easy Vault objectives</a></li>"
		"<li><a class=\"link\" href=\"https://gw2timer.com/\">GW2Timer — live schedules</a></li>"
		"<li><a class=\"link\" href=\"https://account.arena.net/applications\">Create / manage API keys</a></li>"
		"</ul></div></section>\n";

	return BuildPage(
		"Live — Dailies &amp; Vault",
		"GW2 In-Game Helper · Live",
		"Dailies &amp; Wizard’s Vault",
		hasKey
			? "Live Vault progress from your API key, plus crafting and world-boss checklists."
			: "Add your API key in Settings (helper side rail) for live Vault — crafting and bosses work now.",
		"<a href=\"#apikey\">API key</a>\n<a href=\"#vault-daily\">Vault</a>\n"
		"<a href=\"#craft\">Crafting</a>\n<a href=\"#bosses\">Bosses</a>\n<a href=\"#links\">Links</a>",
		body);
}

} // namespace LivePanelsBuild
