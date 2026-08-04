#include "UnlocksDataInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace UnlocksDetail
{
	bool WriteUtf8File(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		CloseHandle(h);
		return ok && written == data.size();
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 16 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok || read != out.size())
			return {};
		return out;
	}

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos)
			return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos)
			return {};
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t'))
			++k;
		if (k >= json.size() || json[k] != '"')
			return {};
		++k;
		std::string out;
		while (k < json.size())
		{
			const char c = json[k++];
			if (c == '\\' && k < json.size())
			{
				out.push_back(json[k++]);
				continue;
			}
			if (c == '"')
				break;
			out.push_back(c);
		}
		return out;
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos)
			return -1;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos)
			return -1;
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t'))
			++k;
		return std::atoll(json.c_str() + k);
	}

	void ParseIdArray(const std::string& body, std::unordered_set<int>& out)
	{
		out.clear();
		for (size_t i = 0; i < body.size(); ++i)
		{
			if (body[i] < '0' || body[i] > '9')
				continue;
			const int id = std::atoi(body.c_str() + i);
			if (id > 0)
				out.insert(id);
			while (i < body.size() && body[i] >= '0' && body[i] <= '9')
				++i;
		}
	}

	/* Finishers arrive as [{"id":N,"permanent":true},…] — collect id fields. */
	void ParseFinisherIds(const std::string& body, std::unordered_set<int>& out)
	{
		out.clear();
		size_t p = 0;
		while (p < body.size())
		{
			const size_t k = body.find("\"id\"", p);
			if (k == std::string::npos)
				break;
			const long long id = JsonIntAfterKey(body, "id", k);
			if (id > 0)
				out.insert(static_cast<int>(id));
			p = k + 4;
		}
		if (out.empty())
			ParseIdArray(body, out);
	}

	void ParseNameObjects(const std::string& body, std::unordered_map<int, std::string>& names)
	{
		size_t p = 0;
		while (p < body.size())
		{
			const size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			int depth = 0;
			bool inStr = false, esc = false;
			size_t end = brace;
			for (; end < body.size(); ++end)
			{
				const char c = body[end];
				if (inStr)
				{
					if (esc) esc = false;
					else if (c == '\\') esc = true;
					else if (c == '"') inStr = false;
					continue;
				}
				if (c == '"') { inStr = true; continue; }
				if (c == '{') ++depth;
				else if (c == '}')
				{
					--depth;
					if (depth == 0)
					{
						++end;
						break;
					}
				}
			}
			const long long id = JsonIntAfterKey(body, "id", brace);
			std::string name = JsonStringAfterKey(body, "name", brace);
			if (id > 0 && !name.empty())
				names[static_cast<int>(id)] = std::move(name);
			p = end;
		}
	}

	bool LoadCache(UnlocksData::Kind k, std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names)
	{
		const std::wstring path = CachePathW(k);
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
			return false;
		FILETIME nowFt{};
		GetSystemTimeAsFileTime(&nowFt);
		ULARGE_INTEGER now{}, then{};
		now.LowPart = nowFt.dwLowDateTime;
		now.HighPart = nowFt.dwHighDateTime;
		then.LowPart = fad.ftLastWriteTime.dwLowDateTime;
		then.HighPart = fad.ftLastWriteTime.dwHighDateTime;
		if ((now.QuadPart - then.QuadPart) / 10000ULL > kCacheTtlMs)
			return false;

		const std::string raw = ReadUtf8File(path);
		if (raw.rfind("#gw2igh-unlocks v1", 0) != 0)
			return false;
		ids.clear();
		names.clear();
		size_t p = raw.find('\n');
		if (p == std::string::npos)
			return false;
		++p;
		while (p < raw.size())
		{
			size_t end = raw.find('\n', p);
			if (end == std::string::npos)
				end = raw.size();
			std::string line = raw.substr(p, end - p);
			p = end + 1;
			if (line.empty() || line[0] == '#')
				continue;
			const size_t tab = line.find('\t');
			const int id = std::atoi(line.c_str());
			if (id <= 0)
				continue;
			ids.insert(id);
			if (tab != std::string::npos && tab + 1 < line.size())
				names[id] = line.substr(tab + 1);
		}
		return !ids.empty();
	}

	void SaveCache(UnlocksData::Kind k, const std::unordered_set<int>& ids,
		const std::unordered_map<int, std::string>& names)
	{
		std::string out = "#gw2igh-unlocks v1\n";
		std::vector<int> sorted(ids.begin(), ids.end());
		std::sort(sorted.begin(), sorted.end());
		for (int id : sorted)
		{
			out += std::to_string(id);
			out += '\t';
			auto it = names.find(id);
			if (it != names.end())
			{
				for (char c : it->second)
					out.push_back((c == '\t' || c == '\n') ? ' ' : c);
			}
			out += '\n';
		}
		CreateDirectoryW(AddonPaths::DataDir().c_str(), nullptr);
		WriteUtf8File(CachePathW(k), out);
	}

	void ResolveNames(UnlocksData::Kind k, const std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names, const char* key)
	{
		std::vector<int> missing;
		missing.reserve(ids.size());
		for (int id : ids)
		{
			if (names.find(id) == names.end())
				missing.push_back(id);
		}
		std::sort(missing.begin(), missing.end());
		for (size_t i = 0; i < missing.size(); )
		{
			std::string path = kPublicPaths[KindIndex(k)];
			path += "?ids=";
			const size_t end = (std::min)(i + static_cast<size_t>(kNameBatch), missing.size());
			for (size_t j = i; j < end; ++j)
			{
				if (j > i)
					path += ",";
				path += std::to_string(missing[j]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (r.ok)
				ParseNameObjects(r.body, names);
			i = end;
		}
		(void)key;
	}

	DWORD WINAPI LoadProc(void* p)
	{
		LoadArg* arg = static_cast<LoadArg*>(p);
		const UnlocksData::Kind kind = arg->kind;
		const bool force = arg->force;
		delete arg;

		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		KindState& st = gKinds[KindIndex(kind)];
		std::unordered_set<int> ids;
		std::unordered_map<int, std::string> names;
		std::string status;

		if (!force && LoadCache(kind, ids, names))
		{
			status = "Loaded " + std::to_string(ids.size()) + " (cache).";
		}
		else if (!G::Gw2ApiKey[0])
		{
			status = "API key required (unlocks scope).";
		}
		else
		{
			status = "Downloading…";
			{
				std::lock_guard<std::mutex> lock(gMu);
				st.pendingStatus = status;
			}
			auto r = Gw2Http::Api(kAccountPaths[KindIndex(kind)], G::Gw2ApiKey, kHttpTimeoutMs);
			if (!r.ok)
			{
				status = r.status == 403
					? "Missing unlocks scope on API key."
					: ("Download failed: " + r.error);
			}
			else
			{
				if (kind == UnlocksData::Kind::Finishers)
					ParseFinisherIds(r.body, ids);
				else
					ParseIdArray(r.body, ids);
				ResolveNames(kind, ids, names, G::Gw2ApiKey);
				SaveCache(kind, ids, names);
				status = "Indexed " + std::to_string(ids.size()) + " " +
					kLabels[KindIndex(kind)] + ".";
			}
		}

		{
			std::lock_guard<std::mutex> lock(gMu);
			st.pendingIds = std::move(ids);
			st.pendingNames = std::move(names);
			st.pendingStatus = status;
			st.pending = true;
			st.busy = false;
		}
		return 0;
	}
} // namespace UnlocksDetail
