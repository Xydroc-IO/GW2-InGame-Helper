#include "LivePanelsBuildProgressInternal.h"

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

} // namespace LivePanelsBuild
