#include "LivePanelsBuildShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
std::string IdsQuery(const std::vector<int>& ids, size_t from, size_t count)
{
	std::string q;
	for (size_t i = 0; i < count && from + i < ids.size(); ++i)
	{
		if (i) q += ',';
		q += std::to_string(ids[from + i]);
	}
	return q;
}

struct PriceRow
{
	int id = 0;
	std::string name;
	long long buy = 0;
	long long sell = 0;
	bool custom = false;
};

void FetchItemNames(const std::vector<int>& ids, std::vector<PriceRow>& rows, int timeoutMs)
{
	if (ids.empty())
		return;
	const size_t batches = (ids.size() + 199) / 200;
	std::vector<std::string> paths(batches);
	std::vector<Gw2Http::Result> results(batches);
	std::vector<ParallelApiJob> jobs(batches);
	for (size_t bi = 0; bi < batches; ++bi)
	{
		const size_t off = bi * 200;
		const size_t n = (ids.size() - off > 200) ? 200 : (ids.size() - off);
		paths[bi] = "/v2/items?ids=";
		paths[bi] += IdsQuery(ids, off, n);
		jobs[bi].path = paths[bi].c_str();
		jobs[bi].bearer = nullptr;
		jobs[bi].timeoutMs = timeoutMs;
		jobs[bi].out = &results[bi];
	}
	RunParallelApis(jobs.data(), jobs.size());

	for (size_t bi = 0; bi < batches; ++bi)
	{
		const Gw2Http::Result& r = results[bi];
		if (!r.ok)
			continue;
		size_t p = 0;
		while (p < r.body.size())
		{
			size_t brace = r.body.find('{', p);
			if (brace == std::string::npos)
				break;
			size_t end = JsonObjectEnd(r.body, brace);
			if (end == std::string::npos)
				break;
			long long id = JsonIntAfterKey(r.body, "id", brace);
			std::string name = JsonStringAfterKey(r.body, "name", brace);
			if (id > 0 && !name.empty())
			{
				for (PriceRow& row : rows)
				{
					if (row.id == static_cast<int>(id))
					{
						row.name = name;
						break;
					}
				}
			}
			p = end + 1;
		}
	}
}

void ApplyNamesFromJson(const std::string& json, std::vector<PriceRow>& rows)
{
	size_t p = 0;
	while (p < json.size())
	{
		size_t brace = json.find('{', p);
		if (brace == std::string::npos)
			break;
		size_t end = JsonObjectEnd(json, brace);
		if (end == std::string::npos)
			break;
		long long id = JsonIntAfterKey(json, "id", brace);
		std::string name = JsonStringAfterKey(json, "name", brace);
		if (id > 0 && !name.empty())
		{
			for (PriceRow& row : rows)
			{
				if (row.id == static_cast<int>(id))
				{
					row.name = name;
					break;
				}
			}
		}
		p = end + 1;
	}
}

void EnsureArmoryNames(const std::wstring& addonDir,
	const std::vector<int>& ids, std::vector<PriceRow>& rows)
{
	if (ids.empty())
		return;
	const std::wstring path = StemPath(addonDir, "live-armory-names", L".json");
	if (FileFresh(path, kArmoryTtlSec))
	{
		std::string cached = ReadUtf8File(path);
		if (!cached.empty())
		{
			ApplyNamesFromJson(cached, rows);
			int named = 0;
			for (const PriceRow& r : rows)
				if (!r.name.empty()) ++named;
			if (named > static_cast<int>(ids.size()) / 2)
				return;
		}
	}
	FetchItemNames(ids, rows, kLiveBulkTimeoutMs);
	std::string out = "[";
	bool first = true;
	for (const PriceRow& r : rows)
	{
		if (r.name.empty())
			continue;
		if (!first) out += ',';
		first = false;
		out += "{\"id\":";
		out += std::to_string(r.id);
		out += ",\"name\":\"";
		for (char c : r.name)
		{
			if (c == '"' || c == '\\') out += '\\';
			out += c;
		}
		out += "\"}";
	}
	out += ']';
	if (!first)
		WriteUtf8File(path, out);
}

std::string UrlEncodePathSegment(const std::string& s)
{
	std::string o;
	o.reserve(s.size() * 3);
	for (unsigned char c : s)
	{
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~')
			o.push_back(static_cast<char>(c));
		else if (c == ' ')
			o += "%20";
		else
		{
			char buf[8];
			std::snprintf(buf, sizeof(buf), "%%%02X", c);
			o += buf;
		}
	}
	return o;
}

void ParseArmoryCatalog(const std::string& body,
	std::vector<int>& armoryIds, std::vector<int>& maxCounts)
{
	armoryIds.clear();
	maxCounts.clear();
	if (body.empty())
		return;

	/* Prefer objects from ?ids=all: [{"id":1,"max_count":2}, ...] */
	size_t p = 0;
	while (p < body.size() && armoryIds.size() < 400)
	{
		size_t brace = body.find('{', p);
		if (brace == std::string::npos)
			break;
		size_t end = JsonObjectEnd(body, brace);
		if (end == std::string::npos)
			break;
		long long id = JsonIntAfterKey(body, "id", brace);
		long long mx = JsonIntAfterKey(body, "max_count", brace);
		if (id > 0)
		{
			armoryIds.push_back(static_cast<int>(id));
			maxCounts.push_back(mx > 0 ? static_cast<int>(mx) : 1);
		}
		p = end + 1;
	}
	if (!armoryIds.empty())
		return;

	/* Root /v2/legendaryarmory is a bare id list: [105317, 83162, ...] */
	p = 0;
	while (p < body.size() && armoryIds.size() < 400)
	{
		while (p < body.size() && (body[p] < '0' || body[p] > '9'))
			++p;
		if (p >= body.size())
			break;
		int v = 0;
		bool any = false;
		while (p < body.size() && body[p] >= '0' && body[p] <= '9')
		{
			any = true;
			v = v * 10 + (body[p] - '0');
			++p;
		}
		if (any && v > 0)
		{
			armoryIds.push_back(v);
			maxCounts.push_back(1);
		}
	}
}

std::string EnsureArmoryCatalogJson(const std::wstring& addonDir)
{
	const std::wstring path = StemPath(addonDir, "live-armory", L".json");
	if (FileFresh(path, kArmoryTtlSec))
	{
		std::string cached = ReadUtf8File(path);
		if (!cached.empty() && cached.find('{') != std::string::npos)
			return cached;
	}
	/* Bare /v2/legendaryarmory returns ids only — need ids=all for max_count. */
	auto r = Gw2Http::Api("/v2/legendaryarmory?ids=all", nullptr, kLiveBulkTimeoutMs);
	if (r.ok && r.body.find("\"id\"") != std::string::npos)
	{
		WriteUtf8File(path, r.body);
		return r.body;
	}
	std::string stale = ReadUtf8File(path);
	if (!stale.empty())
		return stale;
	return r.ok ? r.body : std::string{};
}

std::string BuildProgressHtml(const std::wstring& addonDir, const char* apiKey)
{
	std::string body;
	const bool hasKey = apiKey && apiKey[0];

	std::vector<int> armoryIds;
	std::vector<int> maxCounts;
	const std::string catalogBody = EnsureArmoryCatalogJson(addonDir);
	ParseArmoryCatalog(catalogBody, armoryIds, maxCounts);
	std::string catalogErr;
	if (armoryIds.empty())
		catalogErr = catalogBody.empty() ? "empty response" : "parse failed";

	std::vector<PriceRow> nameRows;
	nameRows.reserve(armoryIds.size());
	for (int id : armoryIds)
	{
		PriceRow r;
		r.id = id;
		nameRows.push_back(r);
	}
	if (!armoryIds.empty())
		EnsureArmoryNames(addonDir, armoryIds, nameRows);

	std::vector<int> ownedCount(armoryIds.size(), -1); /* -1 = unknown */
	bool accountOk = false;
	bool accountDenied = false;
	Gw2Http::Result acc;
	Gw2Http::Result chars;
	bool hitAcc = false;
	bool hitChars = false;
	if (hasKey)
	{
		hitAcc = TryCacheHit(addonDir, "live-acc-armory", kAccountTtlSec, acc);
		hitChars = TryCacheHit(addonDir, "live-chars", kAccountTtlSec, chars);

		ParallelApiJob jobs[2];
		size_t nJobs = 0;
		if (!hitAcc)
		{
			jobs[nJobs] = {"/v2/account/legendaryarmory", apiKey, kLiveHttpTimeoutMs, &acc};
			++nJobs;
		}
		if (!hitChars)
		{
			jobs[nJobs] = {"/v2/characters", apiKey, kLiveHttpTimeoutMs, &chars};
			++nJobs;
		}
		RunParallelApis(jobs, nJobs);
		if (!hitAcc)
		{
			StoreCache(addonDir, "live-acc-armory", acc);
			PreferStaleCache(addonDir, "live-acc-armory", acc);
		}
		if (!hitChars)
		{
			StoreCache(addonDir, "live-chars", chars);
			PreferStaleCache(addonDir, "live-chars", chars);
		}

		if (acc.ok)
		{
			accountOk = true;
			for (int& c : ownedCount)
				c = 0;
			size_t p = 0;
			while (p < acc.body.size())
			{
				size_t brace = acc.body.find('{', p);
				if (brace == std::string::npos)
					break;
				size_t end = JsonObjectEnd(acc.body, brace);
				if (end == std::string::npos)
					break;
				long long id = JsonIntAfterKey(acc.body, "id", brace);
				long long cnt = JsonIntAfterKey(acc.body, "count", brace);
				if (id > 0)
				{
					for (size_t i = 0; i < armoryIds.size(); ++i)
					{
						if (armoryIds[i] == static_cast<int>(id))
						{
							ownedCount[i] = cnt > 0 ? static_cast<int>(cnt) : 0;
							break;
						}
					}
				}
				p = end + 1;
			}
		}
		else if (acc.status == 403 || acc.status == 401)
			accountDenied = true;
	}

	if (hasKey && accountOk)
	{
		body += "<div class=\"keybox ok\"><h3>Account legendary armory</h3>"
			"<p>Unlocked counts from your API key — read-only. "
			"Prefer Account → Progress in the helper for the ImGui tracker.</p></div>";
	}
	else if (hasKey && accountDenied)
	{
		body += "<div class=\"keybox warn\"><h3>Need more API scopes</h3>"
			"<p>Legendary progress needs <strong>account</strong> + <strong>inventories</strong> + "
			"<strong>unlocks</strong>. Characters need <strong>characters</strong>. "
			"Vault still uses <strong>progression</strong>. Create a key with those scopes in Options.</p></div>";
	}
	else
	{
		body += "<div class=\"keybox warn\"><h3>Public catalog</h3>"
			"<p>Showing the legendary armory list. Paste an API key in Nexus Options "
			"(scopes: account, inventories, unlocks, characters) to fill unlocks + roster.</p></div>";
	}

	body += "<section class=\"block\" id=\"armory\"><div class=\"head\"><h2>Legendary Armory</h2>"
		"<p>";
	body += accountOk ? "Your unlocks + public catalog" : "Public catalog — tick locally if you want";
	body += "</p></div><div class=\"body\"><ul class=\"checks\">";

	int listed = 0;
	int unlocked = 0;
	for (size_t i = 0; i < armoryIds.size(); ++i)
	{
		const std::string& name = nameRows[i].name;
		const bool have = ownedCount[i] > 0;
		if (have) ++unlocked;
		body += "<li><label class=\"check\"><input type=\"checkbox\"";
		if (have) body += " checked";
		body += "/><span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\"><strong>";
		body += HtmlEscape(name.empty() ? ("Item #" + std::to_string(armoryIds[i])) : name);
		body += "</strong>";
		if (accountOk)
		{
			body += " <span class=\"muted\">";
			body += std::to_string(ownedCount[i] < 0 ? 0 : ownedCount[i]);
			body += "/";
			body += std::to_string(maxCounts[i]);
			body += "</span>";
		}
		body += " — <a class=\"link\" href=\"";
		{
			std::string href;
			if (!name.empty())
			{
				href = "https://wiki.guildwars2.com/wiki/";
				for (char c : name)
				{
					if (c == ' ') href.push_back('_');
					else if (c == '\'') href += "%27";
					else href.push_back(c);
				}
			}
			else
			{
				href = "https://wiki.guildwars2.com/wiki/Special:Search?search=";
				href += std::to_string(armoryIds[i]);
			}
			body += HtmlEscape(href);
		}
		body += "\">wiki</a></span></label></li>";
		++listed;
	}
	body += "</ul>";
	if (listed == 0)
	{
		body += "<p class=\"note\">Could not load legendary armory catalog";
		if (!catalogErr.empty())
		{
			body += " (";
			body += HtmlEscape(catalogErr);
			body += ")";
		}
		body += ". Hit <strong>Reload</strong> — catalog is cached for a day after the first success.</p>";
	}
	else if (accountOk)
	{
		body += "<p class=\"meta\">";
		body += std::to_string(unlocked);
		body += " / ";
		body += std::to_string(listed);
		body += " unlocked in armory.</p>";
	}
	body += "<p style=\"margin-top:12px\">"
		"<a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Legendary_Armory\">Wiki — Legendary Armory</a>"
		"</p></div></section>\n";

	/* Characters — roster + one bulk details call (not N sequential). */
	body += "<section class=\"block\" id=\"chars\"><div class=\"head\"><h2>Characters</h2>"
		"<p>Roster from API (optional key)</p></div><div class=\"body\">";
	if (hasKey)
	{
		if (chars.ok)
		{
			std::vector<std::string> names;
			size_t i = 0;
			while (i < chars.body.size() && names.size() < 64)
			{
				size_t q = chars.body.find('"', i);
				if (q == std::string::npos)
					break;
				size_t after = q;
				std::string val = ReadJsonQuoted(chars.body, q, &after);
				i = after;
				if (val.empty())
					continue;
				names.push_back(val);
			}

			struct CharRow { std::string name; std::string profession; long long level = -1; };
			std::vector<CharRow> charRows;
			charRows.reserve(names.size());
			for (const std::string& nm : names)
			{
				CharRow cr;
				cr.name = nm;
				charRows.push_back(std::move(cr));
			}

			constexpr size_t kMaxCharDetails = 24;
			const size_t detailN = charRows.size() < kMaxCharDetails
				? charRows.size() : kMaxCharDetails;
			if (detailN > 0)
			{
				Gw2Http::Result detail;
				const bool hitDetail = TryCacheHit(addonDir, "live-chars-detail",
					kAccountTtlSec, detail);
				if (!hitDetail)
				{
					std::string path = "/v2/characters?ids=";
					for (size_t ci = 0; ci < detailN; ++ci)
					{
						if (ci) path += ',';
						path += UrlEncodePathSegment(charRows[ci].name);
					}
					detail = Gw2Http::Api(path.c_str(), apiKey, kLiveBulkTimeoutMs);
					StoreCache(addonDir, "live-chars-detail", detail);
					PreferStaleCache(addonDir, "live-chars-detail", detail);
				}
				if (detail.ok)
				{
					size_t p = 0;
					while (p < detail.body.size())
					{
						size_t brace = detail.body.find('{', p);
						if (brace == std::string::npos)
							break;
						size_t end = JsonObjectEnd(detail.body, brace);
						if (end == std::string::npos)
							break;
						std::string nm = JsonStringAfterKey(detail.body, "name", brace);
						std::string profession = JsonStringAfterKey(detail.body, "profession", brace);
						long long level = JsonIntAfterKey(detail.body, "level", brace);
						if (!nm.empty())
						{
							for (CharRow& cr : charRows)
							{
								if (cr.name == nm)
								{
									cr.profession = profession;
									cr.level = level;
									break;
								}
							}
						}
						p = end + 1;
					}
				}
			}

			body += "<ul class=\"rows\">";
			for (size_t ci = 0; ci < detailN; ++ci)
			{
				const CharRow& cr = charRows[ci];
				body += "<li><span class=\"t\">";
				body += HtmlEscape(cr.name);
				body += "</span><span class=\"s\">";
				if (cr.level >= 0)
				{
					body += "Level ";
					body += std::to_string(cr.level);
				}
				if (!cr.profession.empty())
				{
					if (cr.level >= 0) body += " · ";
					body += HtmlEscape(cr.profession);
				}
				body += "</span></li>";
			}
			body += "</ul>";
			if (names.size() > detailN)
			{
				body += "<p class=\"meta\">Showing ";
				body += std::to_string(detailN);
				body += " of ";
				body += std::to_string(names.size());
				body += " characters.</p>";
			}
		}
		else if (chars.status == 403 || chars.status == 401)
		{
			body += "<p class=\"note\">Character roster needs the <strong>characters</strong> scope on your API key.</p>";
		}
		else
		{
			body += "<p class=\"note\">Could not load characters";
			if (!chars.error.empty())
			{
				body += " (";
				body += HtmlEscape(chars.error);
				body += ")";
			}
			body += ".</p>";
		}
	}
	else
	{
		body += "<p class=\"note\">Add an API key with the <strong>characters</strong> scope to list your roster here.</p>";
	}
	body += "<p style=\"margin-top:12px\">"
		"<a class=\"link\" href=\"https://account.arena.net/applications\">Manage API keys</a>"
		" · Prefer <strong>Account → Progress</strong> in the helper toolbar."
		"</p></div></section>\n";

	return BuildPage(
		"Live — Legendaries &amp; Characters",
		"GW2 In-Game Helper · Live",
		"Legendaries &amp; Characters",
		"Armory progress and character roster — official API, no game memory. "
		"Primary UI is Account → Progress.",
		"<a href=\"#armory\">Armory</a>\n<a href=\"#chars\">Characters</a>",
		body);
}

} // namespace LivePanelsBuild
