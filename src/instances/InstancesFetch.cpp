/* Instances weekly raid sync via /v2/account/raids. */
#include "InstancesShared.h"

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
		constexpr int kHttpTimeoutMs = 8000;
		std::mutex gMu;
		std::atomic<bool> gBusy{false};
		std::atomic<bool> gReady{false};
		HANDLE gThread = nullptr;
		std::vector<std::string> gPendIds;
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

		DWORD WINAPI RaidWorker(void*)
		{
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
			std::vector<std::string> ids;
			std::string note;
			if (!G::Gw2ApiKey[0])
			{
				note = "No API key — raids stay local. Add progression key in Settings.";
			}
			else
			{
				auto r = Gw2Http::Api("/v2/account/raids", G::Gw2ApiKey, kHttpTimeoutMs);
				if (!r.ok && (r.status == 401 || r.status == 403))
					note = "API key needs progression scope for weekly raid clears.";
				else if (!r.ok)
					note = r.error.empty() ? "Raid sync failed." : r.error;
				else
				{
					CollectQuotedIds(r.body, ids);
					char buf[96];
					std::snprintf(buf, sizeof(buf),
						"Raids synced (%zu encounters this week).", ids.size());
					note = buf;
				}
			}
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPendIds = std::move(ids);
				gPendNote = std::move(note);
				gReady = true;
				gBusy = false;
			}
			return 0;
		}
	}

	void ApplyRaidEncounterIds(const std::vector<std::string>& ids)
	{
		EnsureCatalog();
		std::unordered_set<std::string> set(ids.begin(), ids.end());
		for (size_t i = 0; i < Count(); ++i)
		{
			Entry* e = At(i);
			if (!e || e->kind != Kind::Raid) continue;
			bool allMapped = !e->steps.empty();
			bool anyMapped = false;
			for (auto& s : e->steps)
			{
				if (!s.apiId[0])
				{
					allMapped = false;
					continue;
				}
				anyMapped = true;
				s.done = set.count(s.apiId) > 0;
				if (!s.done)
					allMapped = false;
			}
			if (anyMapped)
				e->cleared = allMapped && !e->steps.empty();
		}
		SaveProgress();
	}

	bool RaidSyncBusy() { return gBusy.load(); }

	void TickRaidSync()
	{
		if (!gReady.load())
			return;
		std::vector<std::string> ids;
		std::string note;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!gReady)
				return;
			ids = std::move(gPendIds);
			note = std::move(gPendNote);
			gPendIds.clear();
			gPendNote.clear();
			gReady = false;
			if (gThread)
			{
				WaitForSingleObject(gThread, 0);
				CloseHandle(gThread);
				gThread = nullptr;
			}
		}
		const bool applied = !ids.empty() || note.find("synced") != std::string::npos;
		if (applied)
		{
			ApplyRaidEncounterIds(ids);
			gLastSyncMs = GetTickCount();
		}
		std::snprintf(gStatus, sizeof(gStatus), "%s", note.c_str());
	}

	void StartRaidSync(bool force)
	{
		const DWORD now = GetTickCount();
		if (!force && gLastSyncMs != 0 && (now - gLastSyncMs) < kMinResyncMs)
			return;
		/* No key: skip the worker so we do not burn the soft re-sync throttle. */
		if (!G::Gw2ApiKey[0])
		{
			if (force)
				std::snprintf(gStatus, sizeof(gStatus),
					"No API key — raids stay local. Add progression key in Settings.");
			return;
		}
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		std::snprintf(gStatus, sizeof(gStatus), "Syncing weekly raids...");
		gThread = CreateThread(nullptr, 0, RaidWorker, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			std::snprintf(gStatus, sizeof(gStatus), "Could not start raid sync.");
		}
	}
}
