#include "LivePanelsBuildShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
namespace
{
	/* First itemId per legendary entry — used for list/detail icons. */
	void CollectPrimaryItemIds(const std::string& itemsJson, std::vector<int>& out)
	{
		out.clear();
		std::unordered_map<int, bool> seen;
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
			int id = 0;
			bool any = false;
			for (size_t i = br + 1; i < end; ++i)
			{
				if (itemsJson[i] < '0' || itemsJson[i] > '9')
				{
					if (any)
						break;
					continue;
				}
				any = true;
				id = id * 10 + (itemsJson[i] - '0');
			}
			if (any && id > 0 && !seen[id])
			{
				seen[id] = true;
				out.push_back(id);
			}
			search = end + 1;
		}
	}

	std::string IdsCsv(const std::vector<int>& ids, size_t from, size_t count)
	{
		std::string q;
		for (size_t i = 0; i < count && from + i < ids.size(); ++i)
		{
			if (i)
				q += ',';
			q += std::to_string(ids[from + i]);
		}
		return q;
	}

	void ParseIconsFromItemsBody(const std::string& body,
		std::unordered_map<int, std::string>& icons)
	{
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
			std::string icon = JsonStringAfterKey(body, "icon", brace);
			if (id > 0 && icon.rfind("https://", 0) == 0)
				icons[static_cast<int>(id)] = std::move(icon);
			p = end + 1;
		}
	}

	std::string IconsToJson(const std::unordered_map<int, std::string>& icons)
	{
		std::string out = "{";
		bool first = true;
		for (const auto& kv : icons)
		{
			if (!first)
				out += ',';
			first = false;
			out += '"';
			out += std::to_string(kv.first);
			out += "\":\"";
			for (char c : kv.second)
			{
				if (c == '"' || c == '\\')
					out += '\\';
				out += c;
			}
			out += '"';
		}
		out += '}';
		return out;
	}

	void ParseIconsJson(const std::string& json, std::unordered_map<int, std::string>& icons)
	{
		size_t p = 0;
		while (p < json.size())
		{
			while (p < json.size() && (json[p] < '0' || json[p] > '9'))
				++p;
			if (p >= json.size())
				break;
			int id = 0;
			while (p < json.size() && json[p] >= '0' && json[p] <= '9')
			{
				id = id * 10 + (json[p] - '0');
				++p;
			}
			size_t q = json.find('"', p);
			if (q == std::string::npos)
				break;
			size_t start = q + 1;
			size_t end = start;
			while (end < json.size() && json[end] != '"')
			{
				if (json[end] == '\\' && end + 1 < json.size())
					end += 2;
				else
					++end;
			}
			if (end >= json.size())
				break;
			std::string url = json.substr(start, end - start);
			if (id > 0 && url.rfind("https://", 0) == 0)
				icons[id] = std::move(url);
			p = end + 1;
		}
	}
} // namespace

/* Cached public item icons (render.guildwars2.com) for ledger avatars. */
std::string BuildItemIconsJson(const std::wstring& addonDir, const std::string& itemsJson)
{
	std::vector<int> ids;
	CollectPrimaryItemIds(itemsJson, ids);
	std::unordered_map<int, std::string> icons;
	const std::wstring cachePath = StemPath(addonDir, "live-leg-icons", L".json");
	if (FileFresh(cachePath, 7u * 24u * 60u * 60u))
	{
		const std::string cached = ReadUtf8File(cachePath);
		if (!cached.empty())
			ParseIconsJson(cached, icons);
	}
	std::vector<int> missing;
	missing.reserve(ids.size());
	for (int id : ids)
	{
		if (!icons.count(id))
			missing.push_back(id);
	}
	if (!missing.empty())
	{
		const size_t batches = (missing.size() + 199) / 200;
		std::vector<std::string> paths(batches);
		std::vector<Gw2Http::Result> results(batches);
		std::vector<ParallelApiJob> jobs(batches);
		for (size_t bi = 0; bi < batches; ++bi)
		{
			const size_t off = bi * 200;
			const size_t n = (missing.size() - off > 200) ? 200 : (missing.size() - off);
			paths[bi] = "/v2/items?ids=";
			paths[bi] += IdsCsv(missing, off, n);
			jobs[bi].path = paths[bi].c_str();
			jobs[bi].bearer = nullptr;
			jobs[bi].timeoutMs = kLiveBulkTimeoutMs;
			jobs[bi].out = &results[bi];
		}
		RunParallelApis(jobs.data(), jobs.size());
		for (size_t bi = 0; bi < batches; ++bi)
		{
			if (results[bi].ok)
				ParseIconsFromItemsBody(results[bi].body, icons);
		}
		if (!icons.empty())
			WriteUtf8File(cachePath, IconsToJson(icons));
	}
	return IconsToJson(icons);
}

} // namespace LivePanelsBuild
