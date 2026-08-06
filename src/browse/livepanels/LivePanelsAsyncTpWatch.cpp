#include "LivePanelsInternal.h"

#include "LivePanels.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsDetail
{
namespace
{
	void ParseIdList(const char* csv, std::vector<int>& out, size_t maxN)
	{
		if (!csv || !csv[0])
			return;
		const char* p = csv;
		while (*p && out.size() < maxN)
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t')
				++p;
			if (!*p)
				break;
			int v = 0;
			bool any = false;
			while (*p >= '0' && *p <= '9')
			{
				any = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (any && v > 0)
			{
				bool dup = false;
				for (int x : out)
				{
					if (x == v) { dup = true; break; }
				}
				if (!dup)
					out.push_back(v);
			}
			while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t')
				++p;
		}
	}

	bool WatchlistContains(const std::vector<int>& ids, int id)
	{
		for (int x : ids)
			if (x == id) return true;
		return false;
	}

	void SerializeTpWatchIds(const std::vector<int>& ids)
	{
		std::string s;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) s += ',';
			s += std::to_string(ids[i]);
		}
		if (s.size() >= sizeof(G::TpWatchIds))
			s.resize(sizeof(G::TpWatchIds) - 1);
		std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", s.c_str());
	}
} // namespace

bool MutateTpWatchlist(const char* op, int id)
{
	if (id <= 0 || !op || !op[0])
		return false;
	std::vector<int> ids;
	ParseIdList(G::TpWatchIds, ids, 120);
	if (std::strcmp(op, "add") == 0)
	{
		if (WatchlistContains(ids, id))
			return false;
		if (ids.size() >= 120)
			return false;
		ids.push_back(id);
	}
	else if (std::strcmp(op, "remove") == 0)
	{
		bool found = false;
		std::vector<int> next;
		next.reserve(ids.size());
		for (int x : ids)
		{
			if (x == id) { found = true; continue; }
			next.push_back(x);
		}
		if (!found)
			return false;
		ids.swap(next);
	}
	else
		return false;
	SerializeTpWatchIds(ids);
	Settings::SetDirty();
	LivePanels::InvalidateTpCache(AddonPaths::DataDir());
	return true;
}

bool ProcessTpWatchCmdFile(const std::wstring& addonDir)
{
	const std::wstring path = AddonPaths::EnsureUnder(addonDir, L"cmds") + L"\\live-tp-cmd.txt";
	const std::string raw = ReadUtf8File(path);
	if (raw.empty())
		return false;
	DeleteFileW(path.c_str());
	bool changed = false;
	size_t i = 0;
	while (i < raw.size())
	{
		while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\r' || raw[i] == '\n' || raw[i] == '\t'))
			++i;
		if (i >= raw.size())
			break;
		const size_t lineStart = i;
		while (i < raw.size() && raw[i] != '\n' && raw[i] != '\r')
			++i;
		std::string line = raw.substr(lineStart, i - lineStart);
		const char* op = nullptr;
		const char* num = nullptr;
		if (line.rfind("add ", 0) == 0)
		{
			op = "add";
			num = line.c_str() + 4;
		}
		else if (line.rfind("remove ", 0) == 0)
		{
			op = "remove";
			num = line.c_str() + 7;
		}
		if (!op || !num)
			continue;
		int id = 0;
		for (const char* p = num; *p >= '0' && *p <= '9'; ++p)
			id = id * 10 + (*p - '0');
		if (MutateTpWatchlist(op, id))
			changed = true;
	}
	return changed;
}

bool ParseTpWatchMutateUrl(const std::string& url, const char** opOut, int* idOut)
{
	if (url.rfind("about:live-tp-add-", 0) == 0)
	{
		*opOut = "add";
		*idOut = 0;
		for (const char* p = url.c_str() + 18; *p >= '0' && *p <= '9'; ++p)
			*idOut = *idOut * 10 + (*p - '0');
		return *idOut > 0;
	}
	if (url.rfind("about:live-tp-remove-", 0) == 0)
	{
		*opOut = "remove";
		*idOut = 0;
		for (const char* p = url.c_str() + 21; *p >= '0' && *p <= '9'; ++p)
			*idOut = *idOut * 10 + (*p - '0');
		return *idOut > 0;
	}
	return false;
}

} // namespace LivePanelsDetail
