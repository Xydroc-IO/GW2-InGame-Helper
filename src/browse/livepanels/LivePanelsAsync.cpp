#include "LivePanelsInternal.h"

#include "LivePanels.h"
#include "LivePanelsBuild.h"

#include "Globals.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsDetail
{
LiveAsyncState gAsync;

std::string OfflineShellHtml(const char* title, const char* heading, const char* note)
{
	std::string body = "<section class=\"block\"><div class=\"head\"><h2>Loading Live data…</h2></div><div class=\"body\">";
	body += "<p class=\"note\">";
	body += note;
	body += "</p>";
	body += "<ul class=\"rows\">";
	body += "<li><a class=\"link\" href=\"about:daily-weekly\">Open offline Daily / Weekly checklist</a></li>";
	body += "<li><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Wizard%27s_Vault/Easy_objectives\">Wiki — Easy Vault objectives</a></li>";
	body += "<li><a class=\"link\" href=\"https://gw2timer.com/\">GW2Timer</a></li>";
	body += "</ul></div></section>\n";
	return LivePanelsBuild::BuildPage(title, "GW2 In-Game Helper · Live", heading,
		"Loading in the background so the game stays smooth…",
		nullptr, body);
}

bool VerMatches(const std::wstring& verPath)
{
	std::string v = ReadUtf8File(verPath);
	while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' '))
		v.pop_back();
	return v == kPanelVer;
}

bool PanelReady(const std::wstring& addonDir, const char* stem)
{
	const std::wstring okPath = StemPath(addonDir, stem, L".ok");
	return GetFileAttributesW(okPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool StemIsRunningOrQueued(const std::string& stem)
{
	for (const std::string& s : gAsync.runningStems)
		if (s == stem) return true;
	for (LiveAsyncJob* j : gAsync.queue)
		if (j && j->stem == stem) return true;
	return false;
}

DWORD WINAPI LiveWorkerProc(void* param)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	LiveAsyncJob* job = static_cast<LiveAsyncJob*>(param);
	std::string html;
	if (job->kind == LiveAsyncJob::Dailies)
		html = LivePanelsBuild::BuildDailiesHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::News)
		html = LivePanelsBuild::BuildNewsHtml();
	else if (job->kind == LiveAsyncJob::Fashion)
		html = LivePanelsBuild::BuildFashionHtml(job->addonDir);
	else if (job->kind == LiveAsyncJob::Tp)
		html = LivePanelsBuild::BuildTpHtml(job->tpWatchIds.c_str(), true);
	else if (job->kind == LiveAsyncJob::ApiCheck)
		html = LivePanelsBuild::BuildApiCheckHtml(job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::LegendaryLedger)
		html = LivePanelsBuild::BuildLegendaryLedgerHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::LegendaryDetail)
		html = LivePanelsBuild::BuildLegendaryDetailHtml(job->addonDir, job->apiKey.c_str(),
			job->itemId);
	else if (job->kind == LiveAsyncJob::CheatSheetsHub)
		html = LivePanelsBuild::BuildCheatSheetsHubHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::BrowseHub)
		html = LivePanelsBuild::BuildBrowseHubHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::BrowseCategory)
	{
		const char* slug = nullptr;
		static const char kPrefix[] = "live-browse-cat-";
		if (job->stem.rfind(kPrefix, 0) == 0)
			slug = job->stem.c_str() + (sizeof(kPrefix) - 1);
		const char* cat = LivePanelsBuild::BrowseCategoryFromSlug(slug);
		html = LivePanelsBuild::BuildBrowseCategoryHtml(job->addonDir, cat ? cat : "");
	}
	else
		html = LivePanelsBuild::BuildProgressHtml(job->addonDir, job->apiKey.c_str());

	/* Write on the worker — never dump multi-KB HTML on the game/UI thread. */
	std::string fileUrl;
	bool accept = false;
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		accept = (job->generation == gAsync.generation && !html.empty());
	}
	if (accept)
	{
		const std::wstring htmlPath = StemPath(job->addonDir, job->stem.c_str(), L".html");
		const std::wstring verPath = StemPath(job->addonDir, job->stem.c_str(), L".ver");
		const std::wstring okPath = StemPath(job->addonDir, job->stem.c_str(), L".ok");
		if (WriteUtf8File(htmlPath, html))
		{
			WriteUtf8File(verPath, kPanelVer);
			WriteUtf8File(okPath, "1");
			fileUrl = PathToFileUrl(htmlPath);
		}
	}

	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		if (!fileUrl.empty())
		{
			LiveReadyNav nav;
			nav.stem = job->stem;
			nav.fileUrl = std::move(fileUrl);
			gAsync.readyNav.push_back(std::move(nav));
		}
		for (size_t i = 0; i < gAsync.runningStems.size(); ++i)
		{
			if (gAsync.runningStems[i] == job->stem)
			{
				gAsync.runningStems.erase(gAsync.runningStems.begin() +
					static_cast<std::ptrdiff_t>(i));
				break;
			}
		}
		PumpLiveQueueUnlocked();
	}
	delete job;
	return 0;
}

void PumpLiveQueueUnlocked()
{
	while (static_cast<int>(gAsync.runningStems.size()) < kMaxLiveWorkers &&
		!gAsync.queue.empty())
	{
		LiveAsyncJob* job = gAsync.queue.front();
		gAsync.queue.pop_front();
		if (!job)
			continue;
		if (job->generation != gAsync.generation)
		{
			delete job;
			continue;
		}
		gAsync.runningStems.push_back(job->stem);
		HANDLE th = CreateThread(nullptr, 0, LiveWorkerProc, job, 0, nullptr);
		if (!th)
		{
			gAsync.runningStems.pop_back();
			delete job;
			continue;
		}
		gAsync.joinable.push_back(th);
	}
}

void ReapJoinableUnlocked()
{
	std::vector<HANDLE> keep;
	keep.reserve(gAsync.joinable.size());
	for (HANDLE th : gAsync.joinable)
	{
		if (!th)
			continue;
		if (WaitForSingleObject(th, 0) == WAIT_OBJECT_0)
			CloseHandle(th);
		else
			keep.push_back(th);
	}
	gAsync.joinable.swap(keep);
}

void StartLiveWorker(const std::wstring& addonDir, const char* stem, LiveAsyncJob::Kind kind,
	int itemId)
{
	if (!stem || !stem[0])
		return;
	std::lock_guard<std::mutex> lock(gAsync.mu);
	ReapJoinableUnlocked();
	if (StemIsRunningOrQueued(stem))
		return;

	auto* job = new LiveAsyncJob();
	job->addonDir = addonDir;
	job->stem = stem;
	job->apiKey = G::Gw2ApiKey;
	job->tpWatchIds = G::TpWatchIds;
	job->generation = gAsync.generation;
	job->kind = kind;
	job->itemId = itemId;
	gAsync.queue.push_back(job);
	PumpLiveQueueUnlocked();
}

std::string EnsurePanel(const std::wstring& addonDir, const char* stem,
	LiveAsyncJob::Kind kind, const char* offlineTitle, const char* offlineHeading,
	int itemId)
{
	const std::wstring path = StemPath(addonDir, stem, L".html");
	const std::wstring verPath = StemPath(addonDir, stem, L".ver");
	DWORD ttl = kHtmlTtlSec;
	if (kind == LiveAsyncJob::Tp)
		ttl = kTpHtmlTtlSec;
	else if (kind == LiveAsyncJob::ApiCheck)
		ttl = kApiCheckTtlSec;
	else if (kind == LiveAsyncJob::LegendaryLedger)
		ttl = kLegendaryVaultTtlSec;
	else if (kind == LiveAsyncJob::LegendaryDetail)
		ttl = 2u * 60u * 60u; /* craft tree is expensive — reuse until Sync */
	else if (kind == LiveAsyncJob::BrowseCategory)
		ttl = 7u * 24u * 60u * 60u; /* catalog — version stamp is the real invalidator */
	/* Browse hub: never serve a stale favorites list (cheap sync rebuild). */
	if (kind != LiveAsyncJob::BrowseHub &&
		VerMatches(verPath) && FileFresh(path, ttl) && PanelReady(addonDir, stem))
		return PathToFileUrl(path);

	/* TP tip page — no network; ImGui TpWatchPad owns the real watchlist. */
	if (kind == LiveAsyncJob::Tp)
	{
		WriteUtf8File(path, LivePanelsBuild::BuildTpHtml(nullptr, false));
		WriteUtf8File(verPath, kPanelVer);
		WriteUtf8File(StemPath(addonDir, stem, L".ok"), "1");
		return PathToFileUrl(path);
	}
	/* Cheat sheets hub — catalog only, no network. */
	if (kind == LiveAsyncJob::CheatSheetsHub)
	{
		WriteUtf8File(path, LivePanelsBuild::BuildCheatSheetsHubHtml(addonDir, nullptr));
		WriteUtf8File(verPath, kPanelVer);
		WriteUtf8File(StemPath(addonDir, stem, L".ok"), "1");
		return PathToFileUrl(path);
	}
	/* Browse hub — rebuild favorites when changed; skip disk write if identical
	   so CEF is not forced to reload a file it already has open (Wine crash). */
	if (kind == LiveAsyncJob::BrowseHub)
	{
		const std::string html = LivePanelsBuild::BuildBrowseHubHtml(addonDir, nullptr);
		if (PanelReady(addonDir, stem) && VerMatches(verPath) &&
			ReadUtf8File(path) == html)
			return PathToFileUrl(path);
		if (!WriteUtf8File(path, html))
			return {};
		WriteUtf8File(verPath, kPanelVer);
		WriteUtf8File(StemPath(addonDir, stem, L".ok"), "1");
		return PathToFileUrl(path);
	}
	/* Browse category (Wiki is huge) — shell now, build on worker, then Reload. */
	if (kind == LiveAsyncJob::BrowseCategory)
	{
		const char* slug = nullptr;
		static const char kPrefix[] = "live-browse-cat-";
		if (stem && std::strncmp(stem, kPrefix, sizeof(kPrefix) - 1) == 0)
			slug = stem + (sizeof(kPrefix) - 1);
		const char* cat = LivePanelsBuild::BrowseCategoryFromSlug(slug);
		if (!WriteUtf8File(path,
				LivePanelsBuild::BuildBrowseCategoryShellHtml(cat ? cat : "Browse")))
			return {};
		DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());
		StartLiveWorker(addonDir, stem, kind, itemId);
		return PathToFileUrl(path);
	}

	const std::string shell = kind == LiveAsyncJob::LegendaryDetail
		? LivePanelsBuild::BuildLegendaryDetailShellHtml(itemId)
		: OfflineShellHtml(offlineTitle, offlineHeading,
			kind == LiveAsyncJob::ApiCheck
				? "Probing api.guildwars2.com in the background. This page will refresh when ready."
				: "Fetching Live data in the background. This page will refresh when ready. "
				  "You can keep playing — the game should not freeze.");
	WriteUtf8File(path, shell);
	DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());

	StartLiveWorker(addonDir, stem, kind, itemId);
	return PathToFileUrl(path);
}

} // namespace LivePanelsDetail
