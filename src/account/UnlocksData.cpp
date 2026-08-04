#include "UnlocksData.h"

#include "UnlocksDataInternal.h"

#include "AddonPaths.h"
#include "Globals.h"

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

namespace UnlocksDetail
{
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
} // namespace UnlocksDetail

using namespace UnlocksDetail;

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
