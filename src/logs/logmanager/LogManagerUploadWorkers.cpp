#include "LogManagerUpload.h"
#include "LogManagerUploadInternal.h"

#include "Globals.h"
#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

namespace LogManagerDetail
{
DWORD WINAPI UploadWorker(LPVOID)
{
	int sessionDone = 0;
	for (;;)
	{
		std::vector<std::string> queue;
		{
			std::lock_guard<std::mutex> lock(gMu);
			queue.swap(gUploadQueue);
			if (queue.empty())
			{
				/* Release busy only while queue is still empty — else drain again. */
				gUploadBusy.store(false);
				std::snprintf(gStatus, sizeof(gStatus),
					"Upload finished (%d) → dps.report.", sessionDone);
				return 0;
			}
		}

		gUploadTotal.store(sessionDone + static_cast<int>(queue.size()));
		if (sessionDone == 0)
			gUploadDone.store(0);

		for (const std::string& path : queue)
		{
			if (gCancel.load())
				break;
			std::wstring pathW = Utf8ToWide(path.c_str());
			{
				std::lock_guard<std::mutex> lock(gMu);
				for (auto& e : gLogs)
				{
					if (e.pathUtf8 == path)
					{
						e.state = ParseState::Uploading;
						break;
					}
				}
				gGen.fetch_add(1);
			}

			std::snprintf(gStatus, sizeof(gStatus),
				"Uploading to dps.report %d / %d…",
				sessionDone + 1, gUploadTotal.load());

			std::string resp, err;
			const bool ok = UploadToDpsReport(pathW, resp, err);
			{
				std::lock_guard<std::mutex> lock(gMu);
				for (auto& e : gLogs)
				{
					if (e.pathUtf8 != path)
						continue;
					if (ok)
					{
						e.state = ParseState::Uploaded;
						ApplyDpsReportMeta(e, resp);
						e.parseError.clear();
					}
					else
					{
						e.parseError = err;
						if (e.state == ParseState::Uploading)
							e.state = e.encounter.empty() ? ParseState::Pending : ParseState::Parsed;
					}
					break;
				}
				++sessionDone;
				gUploadDone.store(sessionDone);
				SaveCacheLocked();
				gGen.fetch_add(1);
			}
		}

		if (gCancel.load())
		{
			std::lock_guard<std::mutex> lock(gMu);
			gUploadQueue.clear();
			gUploadBusy.store(false);
			std::snprintf(gStatus, sizeof(gStatus), "Upload cancelled (%d).", sessionDone);
			return 0;
		}
	}
}

DWORD WINAPI HydrateWorker(LPVOID)
{
	const bool force = gHydrateForce.exchange(false);
	std::vector<std::pair<std::string, std::string>> jobs; /* path, permalink */
	{
		std::lock_guard<std::mutex> lock(gMu);
		for (const auto& e : gLogs)
		{
			if (e.dpsReportUrl.empty())
				continue;
			if (!force)
			{
				const bool needBasics =
					e.encounter.empty() || e.result < 0 || e.durationMs <= 0 || e.players.empty();
				const bool needStats = PlayersNeedCombatStats(e.players);
				if (!needBasics && !needStats)
					continue;
			}
			jobs.emplace_back(e.pathUtf8, e.dpsReportUrl);
		}
	}
	int done = 0;
	int jsonOk = 0;
	int jsonFail = 0;
	for (const auto& job : jobs)
	{
		if (gCancel.load())
			break;
		std::string resp, err;
		std::snprintf(gStatus, sizeof(gStatus), "Loading report stats %d / %d…",
			done + 1, static_cast<int>(jobs.size()));
		if (FetchDpsReportMeta(job.second, resp, err))
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (auto& e : gLogs)
			{
				if (e.pathUtf8 != job.first)
					continue;
				ApplyDpsReportMeta(e, resp);
				break;
			}
			gGen.fetch_add(1);
		}

		/* Full EI JSON from dps.report — DPS + boon uptimes + guild IDs. */
		std::string eiJson;
		if (FetchEiJsonFromReport(job.second, eiJson, err))
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (auto& e : gLogs)
			{
				if (e.pathUtf8 != job.first)
					continue;
				ApplyEiJsonToEntry(e, eiJson);
				if (!e.dpsReportUrl.empty())
					e.state = ParseState::Uploaded;
				break;
			}
			gGen.fetch_add(1);
			++jsonOk;
		}
		else
		{
			++jsonFail;
			if (!err.empty())
			{
				std::lock_guard<std::mutex> lock(gMu);
				for (auto& e : gLogs)
				{
					if (e.pathUtf8 != job.first)
						continue;
					if (e.parseError.empty())
						e.parseError = err;
					break;
				}
			}
		}
		++done;
	}
	{
		std::lock_guard<std::mutex> lock(gMu);
		SaveCacheLocked();
		gGen.fetch_add(1);
	}
	if (jobs.empty())
		std::snprintf(gStatus, sizeof(gStatus), "No reports to load (upload first).");
	else if (jsonFail > 0)
		std::snprintf(gStatus, sizeof(gStatus),
			"Loaded %d report(s); %d getJson ok, %d failed.", done, jsonOk, jsonFail);
	else
	std::snprintf(gStatus, sizeof(gStatus),
		"Loaded DPS/boons/guilds for %d report(s).", jsonOk);
	gHydrateBusy.store(false);
	return 0;
}

void BeginHydrateFromReports(bool force)
{
	if (gHydrateBusy.exchange(true))
		return;
	gHydrateForce.store(force);
	gCancel.store(false);
	std::snprintf(gStatus, sizeof(gStatus),
		force ? "Refreshing DPS/boons from dps.report…" : "Loading metadata from dps.report…");
	if (gHydrateThread)
	{
		CloseHandle(gHydrateThread);
		gHydrateThread = nullptr;
	}
	gHydrateThread = CreateThread(nullptr, 0, HydrateWorker, nullptr, 0, nullptr);
	if (!gHydrateThread)
	{
		gHydrateBusy.store(false);
		gHydrateForce.store(false);
	}
}

void BeginUpload(const std::vector<std::string>& paths)
{
	if (paths.empty())
		return;
	{
		std::lock_guard<std::mutex> lock(gMu);
		for (const auto& p : paths)
			gUploadQueue.push_back(p);
	}
	if (gUploadBusy.exchange(true))
		return; /* worker already draining — will pick up queued paths */
	gCancel.store(false);
	std::snprintf(gStatus, sizeof(gStatus), "Uploading to dps.report…");
	if (gUploadThread)
	{
		CloseHandle(gUploadThread);
		gUploadThread = nullptr;
	}
	gUploadThread = CreateThread(nullptr, 0, UploadWorker, nullptr, 0, nullptr);
	if (!gUploadThread)
		gUploadBusy.store(false);
}

} // namespace LogManagerDetail
