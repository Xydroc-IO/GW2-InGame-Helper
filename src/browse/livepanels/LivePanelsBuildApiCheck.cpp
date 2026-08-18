#include "LivePanelsBuildShared.h"

#include "AddonPaths.h"
#include "Gw2Catalog.h"
#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
namespace
{
	constexpr int kProbeTimeoutMs = 4000;

	struct Probe
	{
		const char* label = nullptr;
		const char* apiPath = nullptr; /* /v2/... when url is null */
		const char* url = nullptr;     /* full URL when not ArenaNet */
		bool needsAuth = false;
		bool expectJson = true;
		const char* group = nullptr;
	};

	struct ProbeRow
	{
		const Probe* probe = nullptr;
		unsigned status = 0;
		bool online = false;
		bool schemaOk = false;
		DWORD ms = 0;
		std::string detail;
		bool skipped = false;
	};

	struct TimedJob
	{
		const Probe* probe = nullptr;
		const char* bearer = nullptr;
		Gw2Http::Result result;
		DWORD ms = 0;
	};

	bool LooksLikeJson(const std::string& body)
	{
		size_t i = 0;
		while (i < body.size() && (body[i] == ' ' || body[i] == '\t' ||
			body[i] == '\r' || body[i] == '\n'))
			++i;
		if (i >= body.size())
			return false;
		return body[i] == '{' || body[i] == '[';
	}

	bool LooksLikeBody(const std::string& body)
	{
		size_t i = 0;
		while (i < body.size() && (body[i] == ' ' || body[i] == '\t' ||
			body[i] == '\r' || body[i] == '\n'))
			++i;
		if (i >= body.size())
			return false;
		if (LooksLikeJson(body))
			return true;
		return body[i] == '<' || body.size() > 32;
	}

	ProbeRow RowFromJob(const TimedJob& j)
	{
		ProbeRow row;
		row.probe = j.probe;
		row.ms = j.ms;
		row.status = j.result.status;
		row.online = j.result.ok && j.result.status >= 200 && j.result.status < 300;
		row.schemaOk = row.online && j.probe && (j.probe->expectJson
			? LooksLikeJson(j.result.body) : LooksLikeBody(j.result.body));
		if (!j.result.ok)
			row.detail = j.result.error.empty() ? "request failed" : j.result.error;
		else if (!row.online)
		{
			char buf[48];
			std::snprintf(buf, sizeof(buf), "HTTP %u", j.result.status);
			row.detail = buf;
		}
		else if (!row.schemaOk)
			row.detail = (j.probe && !j.probe->expectJson)
				? "empty or unexpected body" : "non-JSON body";
		else
			row.detail = "ok";
		return row;
	}

	const char* StatusClass(const ProbeRow& row)
	{
		if (row.skipped)
			return "skip";
		if (row.online && row.schemaOk)
			return "ok";
		if (row.online)
			return "warn";
		return "bad";
	}

	const char* StatusLabel(const ProbeRow& row)
	{
		if (row.skipped)
			return "No API key";
		if (row.online && row.schemaOk)
			return "Online";
		if (row.online)
			return "Online · schema?";
		if (row.status == 429)
			return "Rate limited";
		if (row.status == 503 || row.status == 504)
			return "Down / timeout";
		if (row.status >= 500)
			return "Server error";
		if (row.status == 403)
			return "Forbidden";
		if (row.status == 401)
			return "Unauthorized";
		return "Fail";
	}
}

std::string BuildApiCheckHtml(const char* apiKey)
{
	/* Canary only — not every /v2 route. One GET per helper host besides ArenaNet. */
	static const Probe kProbes[] = {
		{ "/v2/build", "/v2/build", nullptr, false, true, "Public" },
		{ "/v2/items?ids=19721", "/v2/items?ids=19721", nullptr, false, true, "Public" },
		{ "/v2/commerce/prices?ids=19721", "/v2/commerce/prices?ids=19721", nullptr, false, true, "Commerce" },
		{ "/v2/tokeninfo", "/v2/tokeninfo", nullptr, true, true, "Account" },
		{ "/v2/account", "/v2/account", nullptr, true, true, "Account" },
		{ "catalog.manifest", nullptr, Gw2Catalog::kManifestUrl, false, true, "Other" },
		{ "wiki api.php", nullptr,
			"https://wiki.guildwars2.com/api.php?action=query&meta=siteinfo&format=json",
			false, true, "Other" },
		{ "news feed", nullptr, "https://www.guildwars2.com/en/feed/", false, false, "Other" },
		{ "killproof.me", nullptr, "https://killproof.me/", false, false, "Other" },
	};
	constexpr size_t kCount = sizeof(kProbes) / sizeof(kProbes[0]);

	std::vector<ProbeRow> rows(kCount);
	std::vector<TimedJob> jobs;
	jobs.reserve(kCount);

	const ULONGLONG wall0 = GetTickCount64();
	for (size_t i = 0; i < kCount; ++i)
	{
		rows[i].probe = &kProbes[i];
		if (kProbes[i].needsAuth && (!apiKey || !apiKey[0]))
		{
			rows[i].skipped = true;
			rows[i].detail = "Paste a key in Settings (side rail), then Reload this tab.";
			continue;
		}
		TimedJob job;
		job.probe = &kProbes[i];
		job.bearer = kProbes[i].needsAuth ? apiKey : nullptr;
		jobs.push_back(job);
	}
	if (!jobs.empty())
	{
		for (TimedJob& job : jobs)
		{
			if (!job.probe)
				continue;
			const ULONGLONG t0 = GetTickCount64();
			if (job.probe->url && job.probe->url[0])
				job.result = Gw2Http::Get(job.probe->url, job.bearer, kProbeTimeoutMs);
			else if (job.probe->apiPath && job.probe->apiPath[0])
				job.result = Gw2Http::Api(job.probe->apiPath, job.bearer, kProbeTimeoutMs);
			job.ms = static_cast<DWORD>(GetTickCount64() - t0);
		}
	}

	size_t ji = 0;
	for (size_t i = 0; i < kCount; ++i)
	{
		if (rows[i].skipped)
			continue;
		if (ji < jobs.size())
			rows[i] = RowFromJob(jobs[ji++]);
	}
	{
		const std::wstring pack = AddonPaths::CacheDir() + L"\\gw2-helper-catalog.igh";
		const bool onDisk = GetFileAttributesW(pack.c_str()) != INVALID_FILE_ATTRIBUTES;
		for (ProbeRow& r : rows)
		{
			if (!r.probe || !r.probe->url || !std::strstr(r.probe->url, "catalog.manifest"))
				continue;
			if (onDisk)
			{
				if (r.detail == "ok")
					r.detail = "ok · names pack on disk";
			}
			else if (r.detail == "ok")
				r.detail = "ok · names pack not in cache yet";
			else
				r.detail += " · names pack not in cache";
		}
	}
	const DWORD wallMs = static_cast<DWORD>(GetTickCount64() - wall0);

	int okN = 0, failN = 0, skipN = 0, warnN = 0;
	for (const ProbeRow& r : rows)
	{
		if (r.skipped)
			++skipN;
		else if (r.online && r.schemaOk)
			++okN;
		else if (r.online)
			++warnN;
		else
			++failN;
	}

	std::string summary = "<div class=\"keybox ";
	summary += (failN == 0 && warnN == 0) ? "ok" : (okN > 0 ? "warn" : "warn");
	summary += "\"><h3>";
	if (failN == 0 && warnN == 0 && skipN == 0)
		summary += "All probed endpoints online";
	else if (failN == 0 && warnN == 0)
		summary += "Public endpoints online · account checks need a key";
	else
	{
		char line[160];
		std::snprintf(line, sizeof(line),
			"%d online · %d warn · %d fail · %d skipped", okN, warnN, failN, skipN);
		summary += line;
	}
	summary += "</h3><p>Checked <strong>";
	summary += std::to_string(rows.size());
	summary += "</strong> hosts/endpoints in <strong>";
	summary += std::to_string(wallMs);
	summary += " ms</strong>. ArenaNet canaries plus one GET per other helper host. "
		"Reload the tab to re-run.</p></div>\n";

	auto appendGroup = [&](std::string& body, const char* group, const char* host) {
		body += "<section class=\"block\" id=\"";
		body += group;
		body += "\"><div class=\"head\"><h2>";
		body += group;
		body += "</h2><p>";
		body += host;
		body += "</p></div><div class=\"body\">";
		body += "<table class=\"api\"><thead><tr>"
			"<th>Endpoint</th><th>Status</th><th>HTTP</th><th>Schema</th><th>Time</th>"
			"</tr></thead><tbody>";
		int n = 0;
		for (const ProbeRow& r : rows)
		{
			if (!r.probe || std::strcmp(r.probe->group, group) != 0)
				continue;
			++n;
			body += "<tr class=\"";
			body += StatusClass(r);
			body += "\"><td><code>";
			body += HtmlEscape(r.probe->label ? r.probe->label : "");
			body += "</code></td><td>";
			body += StatusLabel(r);
			body += "</td><td>";
			if (r.skipped)
				body += "—";
			else
				body += std::to_string(r.status ? r.status : 0);
			body += "</td><td>";
			if (r.skipped)
				body += "—";
			else
				body += r.schemaOk ? "OK" : "—";
			body += "</td><td>";
			if (r.skipped)
				body += "—";
			else
			{
				body += std::to_string(r.ms);
				body += " ms";
			}
			body += "</td></tr>";
			if (!r.detail.empty() && r.detail != "ok")
			{
				body += "<tr class=\"detail ";
				body += StatusClass(r);
				body += "\"><td colspan=\"5\"><span class=\"s\">";
				body += HtmlEscape(r.detail);
				body += "</span></td></tr>";
			}
		}
		if (n == 0)
			body += "<tr><td colspan=\"5\">No probes in this group.</td></tr>";
		body += "</tbody></table></div></section>\n";
	};

	std::string body = summary;
	appendGroup(body, "Public", "api.guildwars2.com");
	appendGroup(body, "Commerce", "api.guildwars2.com");
	appendGroup(body, "Account", "api.guildwars2.com");
	appendGroup(body, "Other", "github.com · wiki · news · killproof.me");

	body += "<section class=\"block\" id=\"about\"><div class=\"head\"><h2>About</h2>"
		"<p>What this page is</p></div><div class=\"body\">"
		"<p class=\"note\">Canaries only (not every <code>/v2</code> route). ArenaNet: build, "
		"a public item, commerce, tokeninfo, account. Other hosts we actually GET: "
		"GitHub catalog manifest, wiki MediaWiki API, official news feed, killproof.me. "
		"Account rows use your Settings key when present. The names pack "
		"<code>gw2-helper-catalog.igh</code> downloads in the background after helper open.</p>"
		"<p class=\"meta\">Tip: if ArenaNet rows fail together, that API or your network is "
		"degraded. If only Other rows fail, that host is down independently.</p>"
		"</div></section>\n";

	const char* toc =
		"<a href=\"#Public\">Public</a>"
		"<a href=\"#Commerce\">Commerce</a>"
		"<a href=\"#Account\">Account</a>"
		"<a href=\"#Other\">Other</a>"
		"<a href=\"#about\">About</a>";

	const char* extraCss =
		"<style>\n"
		"table.api{width:100%;border-collapse:collapse;font-size:0.88rem}\n"
		"table.api th,table.api td{padding:8px 10px;text-align:left;border-bottom:1px solid var(--border-soft);vertical-align:top}\n"
		"table.api th{color:var(--gold-dim);font-size:0.75rem;letter-spacing:0.06em;text-transform:uppercase}\n"
		"table.api tr.ok td:nth-child(2){color:var(--ok);font-weight:650}\n"
		"table.api tr.warn td:nth-child(2){color:var(--warn);font-weight:650}\n"
		"table.api tr.bad td:nth-child(2){color:#d07070;font-weight:650}\n"
		"table.api tr.skip td:nth-child(2){color:var(--muted)}\n"
		"table.api tr.detail td{padding-top:0;border-bottom-color:transparent}\n"
		"table.api code{font-size:0.84rem;word-break:break-all}\n"
		".wrap{max-width:980px}\n"
		"</style>\n";

	return BuildPage("GW2 API Check", "GW2 In-Game Helper · Diagnostics",
		"GW2 API Check",
		"Live health check of ArenaNet plus the other hosts this addon GETs.",
		toc, body, extraCss);
}

} // namespace LivePanelsBuild
