#include "UnlocksData.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kHttpTimeoutMs = 12000;
	constexpr int kNameBatch = 200;
	constexpr DWORD kCacheTtlMs = 6u * 60u * 60u * 1000u;

	struct KindState
	{
		std::unordered_set<int> ids;
		std::unordered_map<int, std::string> names;
		std::string status = "Not loaded.";
		std::atomic<bool> busy{false};
		std::atomic<bool> ready{false};
		std::atomic<bool> pending{false};
		std::unordered_set<int> pendingIds;
		std::unordered_map<int, std::string> pendingNames;
		std::string pendingStatus;
		HANDLE thread = nullptr;
		DWORD loadedAt = 0;
	};

	std::mutex gMu;
	KindState gKinds[static_cast<int>(UnlocksData::Kind::Count)];

	const char* kLabels[] = {
		"Skins", "Dyes", "Minis", "Finishers", "Outfits",
		"Gliders", "Mail carriers", "Novelties", "Titles"
	};
	const char* kAccountPaths[] = {
		"/v2/account/skins",
		"/v2/account/dyes",
		"/v2/account/minis",
		"/v2/account/finishers",
		"/v2/account/outfits",
		"/v2/account/gliders",
		"/v2/account/mailcarriers",
		"/v2/account/novelties",
		"/v2/account/titles"
	};
	const char* kPublicPaths[] = {
		"/v2/skins",
		"/v2/colors",
		"/v2/minis",
		"/v2/finishers",
		"/v2/outfits",
		"/v2/gliders",
		"/v2/mailcarriers",
		"/v2/novelties",
		"/v2/titles"
	};
	const char* kCacheNames[] = {
		"unlocks-skins.cache",
		"unlocks-dyes.cache",
		"unlocks-minis.cache",
		"unlocks-finishers.cache",
		"unlocks-outfits.cache",
		"unlocks-gliders.cache",
		"unlocks-mailcarriers.cache",
		"unlocks-novelties.cache",
		"unlocks-titles.cache"
	};

	int KindIndex(UnlocksData::Kind k)
	{
		return static_cast<int>(k);
	}

	bool ValidKind(UnlocksData::Kind k)
	{
		return KindIndex(k) >= 0 && KindIndex(k) < static_cast<int>(UnlocksData::Kind::Count);
	}

	std::wstring CachePathW(UnlocksData::Kind k)
	{
		wchar_t name[64]{};
		MultiByteToWideChar(CP_UTF8, 0, kCacheNames[KindIndex(k)], -1, name, 64);
		return AddonPaths::DataDir() + L"\\" + name;
	}

	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

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

	struct LoadArg
	{
		UnlocksData::Kind kind = UnlocksData::Kind::Skins;
		bool force = false;
	};

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
}

const char* UnlocksData::KindLabel(Kind k)
{
	return ValidKind(k) ? kLabels[KindIndex(k)] : "?";
}

const char* UnlocksData::KindApiPath(Kind k)
{
	return ValidKind(k) ? kAccountPaths[KindIndex(k)] : "";
}

void UnlocksData::EnsureLoaded(Kind k, bool force)
{
	if (!ValidKind(k))
		return;
	KindState& st = gKinds[KindIndex(k)];
	if (!force && (st.ready || st.busy))
		return;
	if (st.busy.exchange(true))
		return;
	if (st.thread)
	{
		WaitForSingleObject(st.thread, 0);
		CloseHandle(st.thread);
		st.thread = nullptr;
	}
	if (force)
		st.ready = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		st.status = "Loading…";
		st.pendingStatus = st.status;
	}
	auto* arg = new LoadArg{k, force};
	st.thread = CreateThread(nullptr, 0, LoadProc, arg, 0, nullptr);
	if (!st.thread)
	{
		delete arg;
		st.busy = false;
		std::lock_guard<std::mutex> lock(gMu);
		st.status = "Could not start unlock loader.";
	}
}

void UnlocksData::EnsureAll(bool force)
{
	for (int i = 0; i < static_cast<int>(Kind::Count); ++i)
		EnsureLoaded(static_cast<Kind>(i), force);
}

void UnlocksData::Tick()
{
	for (int i = 0; i < static_cast<int>(Kind::Count); ++i)
	{
		KindState& st = gKinds[i];
		if (!st.pending)
			continue;
		std::lock_guard<std::mutex> lock(gMu);
		if (!st.pending)
			continue;
		st.ids = std::move(st.pendingIds);
		st.names = std::move(st.pendingNames);
		st.status = std::move(st.pendingStatus);
		st.pendingIds.clear();
		st.pendingNames.clear();
		st.pendingStatus.clear();
		st.pending = false;
		st.ready = !st.ids.empty() || st.status.find("Indexed") != std::string::npos ||
			st.status.find("cache") != std::string::npos;
		/* Empty unlock set is still "ready". */
		if (st.status.find("Indexed") != std::string::npos ||
			st.status.find("cache") != std::string::npos ||
			st.status.find("API key") != std::string::npos ||
			st.status.find("failed") != std::string::npos ||
			st.status.find("Missing") != std::string::npos)
			st.ready = true;
		st.loadedAt = GetTickCount();
		if (st.thread)
		{
			WaitForSingleObject(st.thread, 0);
			CloseHandle(st.thread);
			st.thread = nullptr;
		}
	}
}

bool UnlocksData::Busy(Kind k)
{
	return ValidKind(k) && gKinds[KindIndex(k)].busy.load();
}

bool UnlocksData::BusyAny()
{
	for (int i = 0; i < static_cast<int>(Kind::Count); ++i)
		if (gKinds[i].busy.load())
			return true;
	return false;
}

bool UnlocksData::Ready(Kind k)
{
	return ValidKind(k) && gKinds[KindIndex(k)].ready.load();
}

const char* UnlocksData::Status(Kind k)
{
	static char buf[256];
	if (!ValidKind(k))
		return "";
	std::lock_guard<std::mutex> lock(gMu);
	KindState& st = gKinds[KindIndex(k)];
	const std::string& s = (st.busy.load() && !st.pendingStatus.empty()) ? st.pendingStatus : st.status;
	std::snprintf(buf, sizeof(buf), "%s", s.c_str());
	return buf;
}

size_t UnlocksData::Count(Kind k)
{
	if (!ValidKind(k))
		return 0;
	std::lock_guard<std::mutex> lock(gMu);
	return gKinds[KindIndex(k)].ids.size();
}

bool UnlocksData::Has(Kind k, int id)
{
	if (!ValidKind(k) || id <= 0)
		return false;
	std::lock_guard<std::mutex> lock(gMu);
	return gKinds[KindIndex(k)].ids.count(id) != 0;
}

void UnlocksData::Search(Kind k, const char* query, std::vector<Row>& out, size_t maxN)
{
	out.clear();
	if (!ValidKind(k) || maxN == 0)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	KindState& st = gKinds[KindIndex(k)];
	const std::string q = query ? ToLowerCopy(query) : std::string{};
	std::vector<int> ids(st.ids.begin(), st.ids.end());
	std::sort(ids.begin(), ids.end());
	for (int id : ids)
	{
		Row row;
		row.id = id;
		auto it = st.names.find(id);
		row.name = (it != st.names.end()) ? it->second : ("#" + std::to_string(id));
		if (!q.empty())
		{
			const std::string n = ToLowerCopy(row.name);
			if (n.find(q) == std::string::npos &&
				std::to_string(id).find(q) == std::string::npos)
				continue;
		}
		out.push_back(std::move(row));
		if (out.size() >= maxN)
			break;
	}
}
