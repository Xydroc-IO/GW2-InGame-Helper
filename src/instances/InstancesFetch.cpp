/* Instances sync: raids, fractal level, daily fractals, achievements, story quests. */
#include "InstancesShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

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

namespace InstancesDetail
{
	namespace
	{
		constexpr int kHttpTimeoutMs = 8000;

		long ParseIntAfterKey(const std::string& json, const char* key, size_t from)
		{
			const size_t k = json.find(key, from);
			if (k == std::string::npos)
				return 0;
			const size_t c = json.find(':', k);
			if (c == std::string::npos)
				return 0;
			return std::strtol(json.c_str() + c + 1, nullptr, 10);
		}
		std::mutex gMu;
		std::atomic<bool> gBusy{false};
		std::atomic<bool> gReady{false};
		HANDLE gThread = nullptr;
		std::vector<std::string> gPendRaidIds;
		std::vector<int> gPendAchIds;
		std::vector<int> gPendStoryIds;
		std::vector<std::string> gPendDailies;
		int gPendFractalLevel = 0;
		std::string gPendNote;
		DWORD gLastSyncMs = 0;
		constexpr DWORD kMinResyncMs = 90u * 1000u;

		void CollectQuotedIds(const std::string& body, std::vector<std::string>& out)
		{
			out.clear();
			std::unordered_set<std::string> seen;
			size_t p = 0;
			while (p < body.size())
			{
				size_t q1 = body.find('"', p);
				if (q1 == std::string::npos) break;
				size_t q2 = body.find('"', q1 + 1);
				if (q2 == std::string::npos) break;
				std::string id = body.substr(q1 + 1, q2 - q1 - 1);
				if (!id.empty() && seen.insert(id).second)
					out.push_back(std::move(id));
				p = q2 + 1;
			}
		}

		void CollectDoneAchievements(const std::string& body, std::vector<int>& out)
		{
			out.clear();
			size_t pos = 0;
			while (pos < body.size())
			{
				const size_t idKey = body.find("\"id\"", pos);
				if (idKey == std::string::npos)
					break;
				const size_t colon = body.find(':', idKey);
				if (colon == std::string::npos || colon > idKey + 8)
				{
					pos = idKey + 4;
					continue;
				}
				char* end = nullptr;
				const long id = std::strtol(body.c_str() + colon + 1, &end, 10);
				if (!end || id <= 0)
				{
					pos = idKey + 4;
					continue;
				}
				size_t blockEnd = body.find('{', static_cast<size_t>(end - body.c_str()));
				if (blockEnd == std::string::npos || blockEnd > idKey + 500)
					blockEnd = (idKey + 500 < body.size()) ? idKey + 500 : body.size();
				const std::string slice = body.substr(idKey, blockEnd - idKey);
				if (slice.find("\"done\":true") != std::string::npos ||
					slice.find("\"done\": true") != std::string::npos)
					out.push_back(static_cast<int>(id));
				pos = idKey + 4;
			}
		}

		int ParseFractalLevel(const std::string& body)
		{
			const size_t k = body.find("\"fractal_level\"");
			if (k == std::string::npos)
				return 0;
			const size_t c = body.find(':', k);
			if (c == std::string::npos)
				return 0;
			return static_cast<int>(std::strtol(body.c_str() + c + 1, nullptr, 10));
		}

		void CollectDailyFractalIds(const std::string& body, std::vector<int>& ids)
		{
			ids.clear();
			const size_t fr = body.find("\"fractals\"");
			if (fr == std::string::npos)
				return;
			const size_t arr = body.find('[', fr);
			if (arr == std::string::npos)
				return;
			size_t p = arr;
			const size_t end = body.find(']', arr);
			const size_t lim = (end == std::string::npos) ? body.size() : end;
			while (p < lim)
			{
				const size_t idKey = body.find("\"id\"", p);
				if (idKey == std::string::npos || idKey >= lim)
					break;
				const size_t colon = body.find(':', idKey);
				if (colon == std::string::npos || colon >= lim)
					break;
				const int id = static_cast<int>(std::strtol(body.c_str() + colon + 1, nullptr, 10));
				if (id > 0)
					ids.push_back(id);
				p = colon + 1;
			}
		}

		void ResolveAchievementNames(const std::vector<int>& ids, std::vector<std::string>& names)
		{
			names.clear();
			if (ids.empty())
				return;
			std::string path = "/v2/achievements?ids=";
			for (size_t i = 0; i < ids.size() && i < 20; ++i)
			{
				if (i) path += ',';
				path += std::to_string(ids[i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (!r.ok)
				return;
			for (int id : ids)
			{
				char key[32];
				std::snprintf(key, sizeof(key), "\"id\":%d", id);
				size_t at = r.body.find(key);
				if (at == std::string::npos)
				{
					std::snprintf(key, sizeof(key), "\"id\": %d", id);
					at = r.body.find(key);
				}
				if (at == std::string::npos)
				{
					names.push_back("Achievement " + std::to_string(id));
					continue;
				}
				const size_t nk = r.body.find("\"name\"", at);
				if (nk == std::string::npos || nk > at + 400)
				{
					names.push_back("Achievement " + std::to_string(id));
					continue;
				}
				const size_t q1 = r.body.find('"', nk + 6);
				const size_t q2 = (q1 == std::string::npos) ? std::string::npos : r.body.find('"', q1 + 1);
				if (q1 == std::string::npos || q2 == std::string::npos)
					names.push_back("Achievement " + std::to_string(id));
				else
					names.push_back(r.body.substr(q1 + 1, q2 - q1 - 1));
			}
		}

		void CollectIntArray(const std::string& body, std::vector<int>& out)
		{
			out.clear();
			size_t p = 0;
			while (p < body.size())
			{
				while (p < body.size() && (body[p] < '0' || body[p] > '9')) ++p;
				if (p >= body.size()) break;
				int v = 0;
				while (p < body.size() && body[p] >= '0' && body[p] <= '9')
				{
					v = v * 10 + (body[p] - '0');
					++p;
				}
				if (v > 0)
					out.push_back(v);
			}
		}

		void BuildStoryCompletions(const std::unordered_set<int>& questDone,
			std::vector<int>& completeStories)
		{
			completeStories.clear();
			/* Build story -> quests from public /v2/quests (batched). */
			auto idList = Gw2Http::Api("/v2/quests", nullptr, kHttpTimeoutMs);
			if (!idList.ok || idList.body.empty())
				return;
			std::vector<int> allIds;
			CollectIntArray(idList.body, allIds);
			std::unordered_map<int, std::vector<int>> storyQuests;
			for (size_t i = 0; i < allIds.size(); i += 50)
			{
				std::string path = "/v2/quests?ids=";
				const size_t end = (std::min)(i + 50, allIds.size());
				for (size_t j = i; j < end; ++j)
				{
					if (j > i) path += ',';
					path += std::to_string(allIds[j]);
				}
				auto batch = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
				if (!batch.ok)
					continue;
				size_t pos = 0;
				while (pos < batch.body.size())
				{
					const size_t brace = batch.body.find('{', pos);
					if (brace == std::string::npos) break;
					size_t depth = 1;
					size_t p = brace + 1;
					while (p < batch.body.size() && depth)
					{
						if (batch.body[p] == '{') ++depth;
						else if (batch.body[p] == '}') --depth;
						++p;
					}
					const std::string obj = batch.body.substr(brace, p - brace);
					const long qid = ParseIntAfterKey(obj, "\"id\"", 0);
					const long sid = ParseIntAfterKey(obj, "\"story\"", 0);
					if (qid > 0 && sid > 0)
						storyQuests[static_cast<int>(sid)].push_back(static_cast<int>(qid));
					pos = p;
				}
			}
			for (const auto& kv : storyQuests)
			{
				if (kv.second.empty())
					continue;
				bool ok = true;
				for (int q : kv.second)
				{
					if (!questDone.count(q))
					{
						ok = false;
						break;
					}
				}
				if (ok)
					completeStories.push_back(kv.first);
			}
		}

		void CollectCharacterQuests(std::unordered_set<int>& questDone)
		{
			auto chars = Gw2Http::Api("/v2/characters", G::Gw2ApiKey, kHttpTimeoutMs);
			if (!chars.ok)
				return;
			std::vector<std::string> names;
			CollectQuotedIds(chars.body, names);
			for (size_t i = 0; i < names.size() && i < 20; ++i)
			{
				std::string enc;
				for (unsigned char c : names[i])
				{
					if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
						(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
						enc.push_back(static_cast<char>(c));
					else
					{
						char hex[8];
						std::snprintf(hex, sizeof(hex), "%%%02X", c);
						enc += hex;
					}
				}
				const std::string path = "/v2/characters/" + enc + "/quests";
				auto q = Gw2Http::Api(path.c_str(), G::Gw2ApiKey, kHttpTimeoutMs);
				if (!q.ok)
					continue;
				std::vector<int> ids;
				CollectIntArray(q.body, ids);
				for (int id : ids)
					questDone.insert(id);
			}
		}

		DWORD WINAPI SyncWorker(void*)
		{
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
			std::vector<std::string> raidIds;
			std::vector<int> achIds;
			std::vector<int> storyIds;
			std::vector<std::string> dailies;
			int fractalLevel = 0;
			std::string note;

			if (!G::Gw2ApiKey[0])
			{
				note = "No API key — only public daily fractals load. Add progression key in Settings.";
				auto daily = Gw2Http::Api("/v2/achievements/daily", nullptr, kHttpTimeoutMs);
				if (daily.ok)
				{
					std::vector<int> dids;
					CollectDailyFractalIds(daily.body, dids);
					ResolveAchievementNames(dids, dailies);
				}
			}
			else
			{
				auto raids = Gw2Http::Api("/v2/account/raids", G::Gw2ApiKey, kHttpTimeoutMs);
				if (raids.ok)
					CollectQuotedIds(raids.body, raidIds);
				else if (raids.status == 401 || raids.status == 403)
					note = "API key needs progression for raids/achievements/quests.";

				auto acct = Gw2Http::Api("/v2/account", G::Gw2ApiKey, kHttpTimeoutMs);
				if (acct.ok)
					fractalLevel = ParseFractalLevel(acct.body);

				auto ach = Gw2Http::Api("/v2/account/achievements", G::Gw2ApiKey, kHttpTimeoutMs);
				if (ach.ok)
					CollectDoneAchievements(ach.body, achIds);

				auto daily = Gw2Http::Api("/v2/achievements/daily", nullptr, kHttpTimeoutMs);
				if (daily.ok)
				{
					std::vector<int> dids;
					CollectDailyFractalIds(daily.body, dids);
					ResolveAchievementNames(dids, dailies);
				}

				std::unordered_set<int> questDone;
				CollectCharacterQuests(questDone);
				BuildStoryCompletions(questDone, storyIds);

				char buf[160];
				std::snprintf(buf, sizeof(buf),
					"Synced: %zu raids, FR %d, %zu ach, %zu stories, %zu dailies.",
					raidIds.size(), fractalLevel, achIds.size(), storyIds.size(), dailies.size());
				if (note.empty())
					note = buf;
				else
					note = note + " " + buf;
			}

			{
				std::lock_guard<std::mutex> lock(gMu);
				gPendRaidIds = std::move(raidIds);
				gPendAchIds = std::move(achIds);
				gPendStoryIds = std::move(storyIds);
				gPendDailies = std::move(dailies);
				gPendFractalLevel = fractalLevel;
				gPendNote = std::move(note);
				gReady = true;
				gBusy = false;
			}
			return 0;
		}
	}

	bool RaidSyncBusy() { return gBusy.load(); }

	void TickRaidSync()
	{
		if (!gReady.load())
			return;
		std::vector<std::string> raids;
		std::vector<int> ach;
		std::vector<int> stories;
		std::vector<std::string> dailies;
		int fr = 0;
		std::string note;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!gReady)
				return;
			raids = std::move(gPendRaidIds);
			ach = std::move(gPendAchIds);
			stories = std::move(gPendStoryIds);
			dailies = std::move(gPendDailies);
			fr = gPendFractalLevel;
			note = std::move(gPendNote);
			gPendRaidIds.clear();
			gPendAchIds.clear();
			gPendStoryIds.clear();
			gPendDailies.clear();
			gReady = false;
			if (gThread)
			{
				WaitForSingleObject(gThread, 0);
				CloseHandle(gThread);
				gThread = nullptr;
			}
		}
		gFractalLevel = fr;
		gDailyFractals = std::move(dailies);
		ApplyRaidEncounterIds(raids);
		if (!ach.empty())
			ApplyAchievementIds(ach);
		if (!stories.empty())
			ApplyStoryCompletions(stories);
		gLastSyncMs = GetTickCount();
		std::snprintf(gStatus, sizeof(gStatus), "%s", note.c_str());
	}

	void StartRaidSync(bool force)
	{
		const DWORD now = GetTickCount();
		if (!force && gLastSyncMs != 0 && (now - gLastSyncMs) < kMinResyncMs)
			return;
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		std::snprintf(gStatus, sizeof(gStatus), "Syncing instances...");
		gThread = CreateThread(nullptr, 0, SyncWorker, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			std::snprintf(gStatus, sizeof(gStatus), "Could not start sync.");
		}
	}
}
