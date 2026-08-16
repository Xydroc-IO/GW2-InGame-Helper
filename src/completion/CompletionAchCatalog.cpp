#include "CompletionShared.h"
#include "CompletionInternal.h"

#include "AddonPaths.h"
#include "Gw2Http.h"
#include "JsonView.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace CompletionDetail
{
	namespace
	{
		constexpr int kHttpMs = 8000;
		constexpr DWORD kCatalogTtlSec = 7u * 24u * 60u * 60u;
		std::mutex gMu;
		std::vector<AchGroup> gGroups;
		std::unordered_map<int, AchCategory> gCats;
		std::unordered_map<int, AchDef> gDefs;
		std::vector<AchGroup> gPendGroups;
		std::unordered_map<int, AchCategory> gPendCats;
		std::unordered_map<int, AchDef> gPendDefs;
		std::atomic<bool> gCatTried{ false };
		std::atomic<bool> gCatBusy{ false };
		std::atomic<bool> gCatReady{ false };
		std::atomic<bool> gDefBusy{ false };
		std::atomic<bool> gDefReady{ false };
		HANDLE gCatTh = nullptr;
		HANDLE gDefTh = nullptr;

		std::wstring GroupsCachePath()
		{
			return AddonPaths::CacheDir() + L"\\ach-groups.cache";
		}
		std::wstring CatsCachePath()
		{
			return AddonPaths::CacheDir() + L"\\ach-categories.cache";
		}

		bool ReadUtf8(const std::wstring& path, std::string& out)
		{
			HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (h == INVALID_HANDLE_VALUE)
				return false;
			LARGE_INTEGER sz{};
			if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
			{
				CloseHandle(h);
				return false;
			}
			out.resize(static_cast<size_t>(sz.QuadPart));
			DWORD n = 0;
			const bool ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &n, nullptr) &&
				n == out.size();
			CloseHandle(h);
			if (!ok)
				out.clear();
			return ok;
		}

		bool WriteUtf8(const std::wstring& path, const std::string& data)
		{
			HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (h == INVALID_HANDLE_VALUE)
				return false;
			DWORD n = 0;
			const bool ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &n, nullptr);
			CloseHandle(h);
			return ok && n == data.size();
		}

		bool FileFresh(const std::wstring& path, DWORD ttlSec)
		{
			WIN32_FILE_ATTRIBUTE_DATA fad{};
			if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
				return false;
			FILETIME now{};
			GetSystemTimeAsFileTime(&now);
			ULARGE_INTEGER a, b;
			a.LowPart = fad.ftLastWriteTime.dwLowDateTime;
			a.HighPart = fad.ftLastWriteTime.dwHighDateTime;
			b.LowPart = now.dwLowDateTime;
			b.HighPart = now.dwHighDateTime;
			if (b.QuadPart < a.QuadPart)
				return true;
			const ULONGLONG ageSec = (b.QuadPart - a.QuadPart) / 10000000ull;
			return ageSec <= ttlSec;
		}

		void CollectQuotedIds(const std::string& body, std::vector<std::string>& out)
		{
			size_t p = 0;
			while (p < body.size())
			{
				const size_t q = body.find('"', p);
				if (q == std::string::npos)
					break;
				size_t after = 0;
				std::string id = JsonView::ReadQuoted(body, q, &after);
				if (!id.empty() && id.find('-') != std::string::npos)
					out.push_back(std::move(id));
				p = after > q ? after : q + 1;
			}
		}

		void CollectBareInts(const std::string& body, std::vector<int>& out)
		{
			std::unordered_map<int, char> seen;
			size_t i = 0;
			while (i < body.size())
			{
				if (body[i] < '0' || body[i] > '9')
				{
					++i;
					continue;
				}
				int id = 0;
				size_t after = i;
				if (JsonView::ParseInt32(JsonView::AsView(body), i, &id, &after) && id > 0 &&
					seen.emplace(id, 1).second)
					out.push_back(id);
				i = after > i ? after : i + 1;
			}
		}

		DWORD WINAPI CatalogWorker(LPVOID)
		{
			std::vector<AchGroup> groups;
			std::unordered_map<int, AchCategory> cats;
			const std::wstring gp = GroupsCachePath();
			const std::wstring cp = CatsCachePath();
			std::string gBody, cBody;
			if (FileFresh(gp, kCatalogTtlSec) && FileFresh(cp, kCatalogTtlSec) &&
				ReadUtf8(gp, gBody) && ReadUtf8(cp, cBody))
			{
				ParseAchGroups(gBody, groups);
				ParseAchCategories(cBody, cats);
			}
			else
			{
				auto listG = Gw2Http::Api("/v2/achievements/groups", nullptr, kHttpMs);
				std::vector<std::string> guids;
				if (listG.ok)
					CollectQuotedIds(listG.body, guids);
				std::string gJoin;
				std::string path;
				for (size_t i = 0; i < guids.size(); )
				{
					path = "/v2/achievements/groups?ids=";
					const size_t end = (std::min)(i + 12u, guids.size());
					for (size_t j = i; j < end; ++j)
					{
						if (j > i)
							path += ',';
						path += guids[j];
					}
					auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpMs);
					if (r.ok)
					{
						ParseAchGroups(r.body, groups);
						gJoin += r.body;
					}
					i = end;
				}
				auto listC = Gw2Http::Api("/v2/achievements/categories", nullptr, kHttpMs);
				std::vector<int> cids;
				if (listC.ok)
					CollectBareInts(listC.body, cids);
				std::string cJoin;
				path.clear();
				for (size_t i = 0; i < cids.size(); )
				{
					path = "/v2/achievements/categories?ids=";
					const size_t end = (std::min)(i + 180u, cids.size());
					for (size_t j = i; j < end; ++j)
					{
						if (j > i)
							path += ',';
						char buf[16];
						std::snprintf(buf, sizeof(buf), "%d", cids[j]);
						path += buf;
					}
					auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpMs);
					if (r.ok)
					{
						ParseAchCategories(r.body, cats);
						cJoin += r.body;
					}
					i = end;
				}
				if (!gJoin.empty())
					WriteUtf8(gp, gJoin);
				if (!cJoin.empty())
					WriteUtf8(cp, cJoin);
			}
			std::sort(groups.begin(), groups.end(),
				[](const AchGroup& a, const AchGroup& b) { return a.order < b.order; });
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPendGroups = std::move(groups);
				gPendCats = std::move(cats);
				gCatReady = true;
			}
			gCatTried = true;
			gCatBusy = false;
			return 0;
		}

		struct DefJob
		{
			int categoryId = 0;
			std::vector<int> ids;
		};

		DWORD WINAPI DefsWorker(LPVOID p)
		{
			auto* job = static_cast<DefJob*>(p);
			std::unordered_map<int, AchDef> local;
			if (job)
			{
				std::string path;
				for (size_t i = 0; i < job->ids.size(); )
				{
					path = "/v2/achievements?ids=";
					const size_t end = (std::min)(i + 180u, job->ids.size());
					for (size_t j = i; j < end; ++j)
					{
						if (j > i)
							path += ',';
						char buf[16];
						std::snprintf(buf, sizeof(buf), "%d", job->ids[j]);
						path += buf;
					}
					auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpMs);
					if (r.ok)
						ParseAchDefs(r.body, local);
					i = end;
				}
				std::lock_guard<std::mutex> lock(gMu);
				gPendDefs = std::move(local);
				gDefReady = true;
				delete job;
			}
			gDefBusy = false;
			return 0;
		}
	}

	void BeginAchCatalogRefresh(bool force)
	{
		if (!force && (gCatBusy.load() || gCatTried.load()))
			return;
		if (force)
			gCatTried = false;
		if (gCatBusy.exchange(true))
			return;
		if (gCatTh)
		{
			WaitForSingleObject(gCatTh, 0);
			CloseHandle(gCatTh);
			gCatTh = nullptr;
		}
		gCatTh = CreateThread(nullptr, 0, CatalogWorker, nullptr, 0, nullptr);
		if (!gCatTh)
			gCatBusy = false;
	}

	void ApplyAchCatalogResult()
	{
		if (!gCatReady.load())
			return;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gCatReady)
			return;
		gGroups = std::move(gPendGroups);
		gCats = std::move(gPendCats);
		gCatReady = false;
		if (gCatTh)
		{
			WaitForSingleObject(gCatTh, 0);
			CloseHandle(gCatTh);
			gCatTh = nullptr;
		}
	}

	bool AchCatalogBusy() { return gCatBusy.load(); }
	bool AchCatalogReady() { return !gGroups.empty(); }

	const std::vector<AchGroup>& AchGroups() { return gGroups; }

	const AchCategory* FindAchCategory(int id)
	{
		const auto it = gCats.find(id);
		return it == gCats.end() ? nullptr : &it->second;
	}

	int CategoryIdContainingAchievement(int achievementId)
	{
		if (achievementId <= 0)
			return 0;
		for (const auto& kv : gCats)
		{
			for (int id : kv.second.achievementIds)
			{
				if (id == achievementId)
					return kv.first;
			}
		}
		return 0;
	}

	void BeginAchDefsRefresh(int categoryId)
	{
		const AchCategory* c = FindAchCategory(categoryId);
		if (!c || c->achievementIds.empty())
			return;
		BeginAchDefsForIds(c->achievementIds);
	}

	void BeginAchDefsForIds(const std::vector<int>& ids)
	{
		if (ids.empty())
			return;
		if (gDefBusy.exchange(true))
			return;
		if (gDefTh)
		{
			WaitForSingleObject(gDefTh, 0);
			CloseHandle(gDefTh);
			gDefTh = nullptr;
		}
		auto* job = new DefJob();
		job->ids = ids;
		gDefTh = CreateThread(nullptr, 0, DefsWorker, job, 0, nullptr);
		if (!gDefTh)
		{
			delete job;
			gDefBusy = false;
		}
	}

	void ApplyAchDefsResult()
	{
		if (!gDefReady.load())
			return;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gDefReady)
			return;
		for (auto& kv : gPendDefs)
			gDefs[kv.first] = std::move(kv.second);
		gPendDefs.clear();
		gDefReady = false;
		if (gDefTh)
		{
			WaitForSingleObject(gDefTh, 0);
			CloseHandle(gDefTh);
			gDefTh = nullptr;
		}
	}

	bool AchDefsBusy() { return gDefBusy.load(); }

	const AchDef* FindAchDef(int id)
	{
		const auto it = gDefs.find(id);
		return it == gDefs.end() ? nullptr : &it->second;
	}
}
