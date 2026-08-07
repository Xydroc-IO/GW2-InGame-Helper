#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "InventoryData.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	static void ParseSlugArray(const std::string& body, std::vector<std::string>& out)
	{
		size_t i = 0;
		while (i < body.size() && out.size() < 64)
		{
			while (i < body.size() && body[i] != '"') ++i;
			if (i >= body.size()) break;
			++i;
			std::string val;
			while (i < body.size() && body[i] != '"')
			{
				if (body[i] == '\\' && i + 1 < body.size())
				{
					val.push_back(body[i + 1]);
					i += 2;
					continue;
				}
				val.push_back(body[i++]);
			}
			if (i < body.size()) ++i;
			if (!val.empty()) out.push_back(val);
		}
	}

	static std::string PrettySlug(std::string slug)
	{
		for (char& c : slug)
			if (c == '_') c = ' ';
		if (!slug.empty())
			slug[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(slug[0])));
		return slug;
	}

	DWORD WINAPI DailyProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		std::vector<DailyRow> rows;
		std::string status;
		auto r = Gw2Http::Api("/v2/dailycrafting", nullptr, kHttpTimeoutMs);
		if (!r.ok)
		{
			status = "Could not load daily crafting list.";
		}
		else
		{
			std::vector<std::string> slugs;
			ParseSlugArray(r.body, slugs);
			if (!slugs.empty())
			{
				std::string path = "/v2/dailycrafting?ids=";
				for (size_t si = 0; si < slugs.size(); ++si)
				{
					if (si) path += ',';
					path += slugs[si];
				}
				auto det = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
				if (det.ok)
				{
					size_t p = 0;
					while (p < det.body.size())
					{
						size_t brace = det.body.find('{', p);
						if (brace == std::string::npos) break;
						size_t end = JsonObjectEnd(det.body, brace);
						if (end == std::string::npos) break;
						DailyRow row;
						row.slug = JsonStringAfterKey(det.body, "id", brace);
						if (!row.slug.empty())
						{
							row.name = PrettySlug(row.slug);
							rows.push_back(row);
						}
						p = end + 1;
					}
				}
				if (rows.empty())
				{
					for (const std::string& s : slugs)
					{
						DailyRow row;
						row.slug = s;
						row.name = PrettySlug(s);
						rows.push_back(row);
					}
				}
			}

			std::unordered_set<std::string> completed;
			if (G::Gw2ApiKey[0])
			{
				auto acc = Gw2Http::Api("/v2/account/dailycrafting", G::Gw2ApiKey, kHttpTimeoutMs);
				if (acc.ok)
				{
					std::vector<std::string> doneSlugs;
					ParseSlugArray(acc.body, doneSlugs);
					for (const std::string& s : doneSlugs)
						completed.insert(s);
				}
			}

			int doneCount = 0;
			for (DailyRow& row : rows)
			{
				row.done = !row.slug.empty() && completed.count(row.slug) > 0;
				if (row.done) ++doneCount;
			}

			if (rows.empty())
				status = "No daily crafts listed.";
			else if (!G::Gw2ApiKey[0])
				status = "Daily crafting (UTC reset). Add an API key to track done.";
			else
			{
				char buf[96];
				std::snprintf(buf, sizeof(buf),
					"Daily crafting — %d of %d done (UTC reset).",
					doneCount, static_cast<int>(rows.size()));
				status = buf;
			}
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPendingDailies = std::move(rows);
			gDailyStatus = status;
			gDailyReady = true;
			gDailyBusy = false;
			gDailyFetchedAt = GetTickCount();
		}
		return 0;
	}

	void StartDailies(bool force)
	{
		if (!force && gDailyFetchedAt != 0 &&
			(GetTickCount() - gDailyFetchedAt) < kDailyTtlMs)
			return;
		if (gDailyBusy.exchange(true)) return;
		if (gDailyThread)
		{
			WaitForSingleObject(gDailyThread, 0);
			CloseHandle(gDailyThread);
			gDailyThread = nullptr;
		}
		gDailyThread = CreateThread(nullptr, 0, DailyProc, nullptr, 0, nullptr);
		if (!gDailyThread) gDailyBusy = false;
	}

} // namespace CraftingDetail

using namespace CraftingDetail;

void CraftingData::RefreshDailiesIfNeeded(bool force)
{
	StartDailies(force);
}
