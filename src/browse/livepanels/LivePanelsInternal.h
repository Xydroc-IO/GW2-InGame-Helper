#pragma once

/* Shared state / helpers for LivePanels.cpp + LivePanelsAsync.cpp. */

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsDetail
{
	constexpr const char* kPanelVer = "38";
	constexpr DWORD kHtmlTtlSec = 10u * 60u;       /* avoid rebuild storms */
	constexpr DWORD kTpHtmlTtlSec = 60u;
	constexpr DWORD kApiCheckTtlSec = 45u;         /* diagnostics should re-probe often */
	constexpr DWORD kLegendaryVaultTtlSec = 45u;   /* Owned/Missing should stay near-live */
	constexpr int kMaxLiveWorkers = 3;

	std::string WideToUtf8(const std::wstring& w);
	std::string PathToFileUrl(const std::wstring& path);
	std::wstring StemPath(const std::wstring& addonDir, const char* stem, const wchar_t* ext);
	bool FileFresh(const std::wstring& path, DWORD ttlSec);
	bool WriteUtf8File(const std::wstring& path, const std::string& data);
	std::string ReadUtf8File(const std::wstring& path);

	bool MutateTpWatchlist(const char* op, int id);
	bool ProcessTpWatchCmdFile(const std::wstring& addonDir);
	bool ProcessCraftPlanCmdFile(const std::wstring& addonDir);
	bool ProcessLegendaryDetailCmdFile(const std::wstring& addonDir);
	bool ProcessOpenAboutCmdFile(const std::wstring& addonDir);
	bool ProcessOpenSiteCmdFile(const std::wstring& addonDir);
	bool ProcessFavCmdFile(const std::wstring& addonDir);
	void InvalidateBrowseHubCaches(const std::wstring& addonDir);
	/* Favorites change: refresh hub (+ optional category stem). Does not wipe other cats. */
	void InvalidateBrowseFavCaches(const std::wstring& addonDir, const char* categoryStem);
	bool ParseTpWatchMutateUrl(const std::string& url, const char** opOut, int* idOut);
	bool ParseCraftPlanUrl(const std::string& url, int* idOut);
	bool ParseLegendaryItemUrl(const std::string& url, int* idOut, bool* syncOut);
	void QueueCraftPlanCmd(const std::wstring& addonDir, int itemId);

	std::string OfflineShellHtml(const char* title, const char* heading, const char* note);
	bool VerMatches(const std::wstring& verPath);
	bool PanelReady(const std::wstring& addonDir, const char* stem);

	struct LiveAsyncJob
	{
		std::wstring addonDir;
		std::string stem;
		std::string apiKey;
		std::string tpWatchIds;
		unsigned generation = 0;
		int itemId = 0; /* LegendaryDetail / sync */
		enum Kind { Dailies, News, Fashion, Tp, Progress, ApiCheck, LegendaryLedger,
			LegendaryDetail, CheatSheetsHub, BrowseHub, BrowseCategory } kind = Dailies;
	};

	struct LiveReadyNav
	{
		std::string stem;
		std::string fileUrl;
	};

	struct LiveAsyncState
	{
		std::mutex mu;
		unsigned generation = 1;
		std::vector<HANDLE> joinable; /* finished threads awaiting CloseHandle */
		std::vector<std::string> runningStems;
		std::deque<LiveAsyncJob*> queue;
		std::vector<LiveReadyNav> readyNav;
	};

	extern LiveAsyncState gAsync;

	bool StemIsRunningOrQueued(const std::string& stem);
	void PumpLiveQueueUnlocked();
	DWORD WINAPI LiveWorkerProc(void* param);
	void ReapJoinableUnlocked();
	void StartLiveWorker(const std::wstring& addonDir, const char* stem, LiveAsyncJob::Kind kind,
		int itemId = 0);
	std::string EnsurePanel(const std::wstring& addonDir, const char* stem,
		LiveAsyncJob::Kind kind, const char* offlineTitle, const char* offlineHeading,
		int itemId = 0);
}
