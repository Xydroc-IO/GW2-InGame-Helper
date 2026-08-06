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
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
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
			/* ["item", ...] string ids or item names? Actually returns string slugs like "glob_of_ectoplasm" OR ids?
			   API: array of strings - daily crafting recipe ids as strings matching /v2/dailycrafting */
			std::vector<std::string> slugs;
			size_t i = 0;
			while (i < r.body.size() && slugs.size() < 32)
			{
				while (i < r.body.size() && r.body[i] != '"') ++i;
				if (i >= r.body.size()) break;
				++i;
				std::string val;
				while (i < r.body.size() && r.body[i] != '"')
				{
					if (r.body[i] == '\\' && i + 1 < r.body.size()) { val.push_back(r.body[i + 1]); i += 2; continue; }
					val.push_back(r.body[i++]);
				}
				if (i < r.body.size()) ++i;
				if (!val.empty()) slugs.push_back(val);
			}
			/* Resolve names via /v2/dailycrafting?ids=slug1,slug2 */
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
						row.name = JsonStringAfterKey(det.body, "id", brace);
						/* dailycrafting objects: { "id": "slug" } - need item via separate mapping.
						   Actually schema is just { "id": "charged_quartz_crystal" }.
						   Convert slug to display name. */
						if (!row.name.empty())
						{
							std::string pretty = row.name;
							for (char& c : pretty)
								if (c == '_') c = ' ';
							if (!pretty.empty())
								pretty[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(pretty[0])));
							row.name = pretty;
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
						row.name = s;
						for (char& c : row.name)
							if (c == '_') c = ' ';
						rows.push_back(row);
					}
				}
			}
			status = rows.empty() ? "No daily crafts listed." : "Daily crafting (UTC reset).";
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
