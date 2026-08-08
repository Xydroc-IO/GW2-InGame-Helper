/* Instances sync: raids, fractal level, daily fractals, curated CM achievements.
   Story quest crawl removed from the hot path — it pulled the full /v2/quests
   catalog (~dozen multi-MB requests) and froze the client under load.

   Soft sync skips /v2/account/achievements (multi-MB). Force Sync / open does it
   after raids so weekly clears paint first. */
#include "InstancesShared.h"

#include "BgFetch.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace InstancesDetail
{
	namespace
	{
		/* ≤4000 ⇒ Gw2Http one-shot (no 4× retry storm). */
		constexpr int kHttpTimeoutMs = 4000;
		std::mutex gMu;
		std::atomic<bool> gBusy{false};
		std::atomic<bool> gReady{false};
		std::atomic<bool> gFetchAch{false}; /* true = Sync button / open */
		HANDLE gThread = nullptr;
		std::vector<std::string> gPendRaidIds;
		std::vector<int> gPendAchIds;
		std::vector<int> gPendStoryIds;
		std::vector<std::string> gPendDailies;
		int gPendFractalLevel = 0;
		bool gPendHasRaids = false;
		bool gPendHasAch = false;
		bool gPendHasDaily = false;
		bool gPendHasFr = false;
		bool gPendDone = false;
		std::string gPendNote;
		DWORD gLastSyncMs = 0;
		/* Soft while-open throttle — open/Sync still force. */
		constexpr DWORD kMinResyncMs = 45u * 1000u;

		struct HttpSlot
		{
			const char* path = nullptr;
			const char* key = nullptr; /* nullptr = public */
			Gw2Http::Result r{};
		};

		DWORD WINAPI HttpWorker(void* p)
		{
			auto* s = static_cast<HttpSlot*>(p);
			if (s && s->path)
				s->r = Gw2Http::Api(s->path, s->key, kHttpTimeoutMs);
			return 0;
		}

		void RunParallel(HttpSlot* slots, int n)
		{
			if (!slots || n <= 0)
				return;
			if (n == 1)
			{
				HttpWorker(&slots[0]);
				return;
			}
			HANDLE th[8]{};
			const int use = (n > 8) ? 8 : n;
			for (int i = 0; i < use; ++i)
			{
				th[i] = CreateThread(nullptr, 0, HttpWorker, &slots[i], 0, nullptr);
				if (!th[i])
					HttpWorker(&slots[i]);
			}
			for (int i = 0; i < use; ++i)
			{
				if (!th[i])
					continue;
				WaitForSingleObject(th[i], INFINITE);
				CloseHandle(th[i]);
			}
		}

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

		void WantedAchievementIds(std::unordered_set<int>& want)
		{
			want.clear();
			EnsureCatalog();
			for (size_t i = 0; i < Count(); ++i)
			{
				Entry* e = At(i);
				if (!e) continue;
				if (e->achId > 0)
					want.insert(e->achId);
				for (const auto& s : e->steps)
					if (s.achId > 0)
						want.insert(s.achId);
			}
		}

		/* Keep only curated overlay ids — account achievements JSON is huge. */
		void CollectWantedDoneAchievements(const std::string& body,
			const std::unordered_set<int>& want, std::vector<int>& out)
		{
			out.clear();
			if (want.empty() || body.empty())
				return;
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
				const int iid = static_cast<int>(id);
				if (!want.count(iid))
				{
					pos = idKey + 4;
					continue;
				}
				const size_t scanEnd = (idKey + 280 < body.size()) ? idKey + 280 : body.size();
				bool done = false;
				for (size_t k = idKey; k + 11 < scanEnd; ++k)
				{
					if (body.compare(k, 11, "\"done\":true") == 0 ||
						body.compare(k, 12, "\"done\": true") == 0)
					{
						done = true;
						break;
					}
				}
				if (done)
					out.push_back(iid);
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
			for (size_t i = 0; i < ids.size() && i < 12; ++i)
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

		/* Publish whatever is ready; UI applies on next Tick without waiting for CM ach. */
		void PublishPartial(std::vector<std::string>* raids, std::vector<int>* ach,
			std::vector<std::string>* dailies, int* fractalLevel, const std::string& note,
			bool done)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (raids)
			{
				gPendRaidIds = std::move(*raids);
				gPendHasRaids = true;
			}
			if (ach)
			{
				gPendAchIds = std::move(*ach);
				gPendHasAch = true;
			}
			if (dailies)
			{
				gPendDailies = std::move(*dailies);
				gPendHasDaily = true;
			}
			if (fractalLevel)
			{
				gPendFractalLevel = *fractalLevel;
				gPendHasFr = true;
			}
			if (!note.empty())
				gPendNote = note;
			gPendDone = done;
			gReady = true;
			if (done)
				gBusy = false;
		}

		DWORD WINAPI SyncWorker(void*)
		{
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
			const bool wantAch = gFetchAch.load();

			std::vector<std::string> raidIds;
			std::vector<int> achIds;
			std::vector<std::string> dailies;
			int fractalLevel = 0;
			std::string note;
			std::string keyNote;

			/* Parallel hot path: daily (public) + raids + account (when keyed). */
			HttpSlot slots[3]{};
			int n = 0;
			slots[n++] = { "/v2/achievements/daily", nullptr, {} };
			if (G::Gw2ApiKey[0])
			{
				slots[n++] = { "/v2/account/raids", G::Gw2ApiKey, {} };
				slots[n++] = { "/v2/account", G::Gw2ApiKey, {} };
			}
			RunParallel(slots, n);

			const Gw2Http::Result& daily = slots[0].r;
			if (daily.ok)
			{
				std::vector<int> dids;
				CollectDailyFractalIds(daily.body, dids);
				ResolveAchievementNames(dids, dailies);
			}

			if (!G::Gw2ApiKey[0])
			{
				note = "No API key — daily fractals only. Add progression key for raids/CM.";
				PublishPartial(nullptr, nullptr, &dailies, nullptr, note, true);
				return 0;
			}

			const Gw2Http::Result& raids = slots[1].r;
			const Gw2Http::Result& acct = slots[2].r;
			if (raids.ok)
				CollectQuotedIds(raids.body, raidIds);
			else if (raids.status == 401 || raids.status == 403)
				keyNote = "API key needs progression for raids/achievements.";

			if (acct.ok)
				fractalLevel = ParseFractalLevel(acct.body);

			{
				char buf[160];
				std::snprintf(buf, sizeof(buf),
					"Raids synced (%zu).%s",
					raidIds.size(),
					wantAch ? " Loading CM overlays…" : "");
				note = keyNote.empty() ? buf : (keyNote + " " + buf);
			}
			/* Paint weekly clears immediately — don't wait on achievements. */
			{
				std::vector<std::string> raidCopy = raidIds;
				std::vector<std::string> dailyCopy = dailies;
				int fr = fractalLevel;
				PublishPartial(&raidCopy, nullptr, &dailyCopy, &fr, note, !wantAch);
			}

			if (wantAch)
			{
				std::unordered_set<int> want;
				WantedAchievementIds(want);
				if (!want.empty())
				{
					auto ach = Gw2Http::Api("/v2/account/achievements", G::Gw2ApiKey, kHttpTimeoutMs);
					if (ach.ok)
						CollectWantedDoneAchievements(ach.body, want, achIds);
				}
				char buf[160];
				std::snprintf(buf, sizeof(buf),
					"Synced: %zu raids, FR %d, %zu CM ach, %zu dailies.",
					raidIds.size(), fractalLevel, achIds.size(), dailies.size());
				note = keyNote.empty() ? buf : (keyNote + " " + buf);
				PublishPartial(nullptr, &achIds, nullptr, nullptr, note, true);
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
		std::vector<std::string> dailies;
		int fr = 0;
		bool hasRaids = false, hasAch = false, hasDaily = false, hasFr = false, done = false;
		std::string note;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!gReady)
				return;
			hasRaids = gPendHasRaids;
			hasAch = gPendHasAch;
			hasDaily = gPendHasDaily;
			hasFr = gPendHasFr;
			done = gPendDone;
			if (hasRaids)
			{
				raids = std::move(gPendRaidIds);
				gPendRaidIds.clear();
				gPendHasRaids = false;
			}
			if (hasAch)
			{
				ach = std::move(gPendAchIds);
				gPendAchIds.clear();
				gPendHasAch = false;
			}
			if (hasDaily)
			{
				dailies = std::move(gPendDailies);
				gPendDailies.clear();
				gPendHasDaily = false;
			}
			if (hasFr)
			{
				fr = gPendFractalLevel;
				gPendHasFr = false;
			}
			note = gPendNote;
			gReady = false;
			gPendDone = false;
			if (done && gThread)
			{
				WaitForSingleObject(gThread, 0);
				CloseHandle(gThread);
				gThread = nullptr;
			}
		}
		if (hasFr)
			gFractalLevel = fr;
		if (hasDaily)
			gDailyFractals = std::move(dailies);
		if (hasRaids)
			ApplyRaidEncounterIds(raids);
		if (hasAch && !ach.empty())
			ApplyAchievementIds(ach);
		if (done || hasRaids)
			gLastSyncMs = GetTickCount();
		if (!note.empty())
			std::snprintf(gStatus, sizeof(gStatus), "%s", note.c_str());
	}

	void StartRaidSync(bool force)
	{
		BgFetch::SetWanted(BgFetch::Channel::Instances, true);
		if (!force && !BgFetch::AllowWork(BgFetch::Channel::Instances))
			return;
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
		gFetchAch = force; /* soft = raids/dailies/FR only; force also pulls CM ach */
		std::snprintf(gStatus, sizeof(gStatus), "Syncing instances...");
		gThread = CreateThread(nullptr, 0, SyncWorker, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			std::snprintf(gStatus, sizeof(gStatus), "Could not start sync.");
		}
	}
}
