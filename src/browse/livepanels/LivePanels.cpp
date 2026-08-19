#include "LivePanels.h"

#include "LivePanelsInternal.h"
#include "LivePanelsBuild.h"

#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "CheatSheets.h"
#include "EiRuntime.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

using namespace LivePanelsDetail;

bool LivePanels::IsLiveAbout(const char* url)
{
	if (!url)
		return false;
	if (std::strncmp(url, "about:live-tp-add-", 18) == 0 ||
		std::strncmp(url, "about:live-tp-remove-", 21) == 0 ||
		std::strncmp(url, "about:craft-plan-", 17) == 0 ||
		std::strncmp(url, "about:legendary-vault-item-", 27) == 0 ||
		std::strncmp(url, "about:legendary-vault-sync-", 27) == 0 ||
		std::strncmp(url, "about:browse-cat-", 17) == 0)
		return true;
	return std::strcmp(url, "about:live-dailies") == 0 ||
		std::strcmp(url, "about:live-news") == 0 ||
		std::strcmp(url, "about:live-fashion") == 0 ||
		std::strcmp(url, "about:live-tp") == 0 ||
		std::strcmp(url, "about:live-progress") == 0 ||
		std::strcmp(url, "about:legendary-vault") == 0 ||
		std::strcmp(url, "about:cheatsheets-hub") == 0 ||
		std::strcmp(url, "about:browse-hub") == 0 ||
		std::strcmp(url, "about:gw2-api-check") == 0;
}

bool LivePanels::IsLiveUrl(const char* url)
{
	if (!url || !url[0])
		return false;
	if (IsLiveAbout(url))
		return true;
	return std::strstr(url, "live-dailies.html") != nullptr ||
		std::strstr(url, "live-news.html") != nullptr ||
		std::strstr(url, "live-fashion.html") != nullptr ||
		std::strstr(url, "live-tp.html") != nullptr ||
		std::strstr(url, "live-progress.html") != nullptr ||
		std::strstr(url, "live-legendary-vault.html") != nullptr ||
		std::strstr(url, "live-legendary-detail-") != nullptr ||
		std::strstr(url, "live-cheatsheets-hub.html") != nullptr ||
		std::strstr(url, "live-browse-hub.html") != nullptr ||
		std::strstr(url, "live-browse-cat-") != nullptr ||
		std::strstr(url, "gw2-api-check.html") != nullptr;
}

std::string LivePanels::ResolveAboutUrl(const std::wstring& addonDir, const std::string& url)
{
	if (addonDir.empty() || url.empty())
		return {};
	const char* op = nullptr;
	int id = 0;
	if (ParseTpWatchMutateUrl(url, &op, &id))
	{
		MutateTpWatchlist(op, id); /* InvalidateTpCache inside */
		return EnsurePanel(addonDir, "live-tp", LiveAsyncJob::Tp,
			"Live — Trading Post Watchlist", "My TP Watchlist");
	}
	if (ParseCraftPlanUrl(url, &id))
	{
		QueueCraftPlanCmd(addonDir, id);
		char stem[64];
		std::snprintf(stem, sizeof(stem), "live-legendary-detail-%d", id);
		if (PanelReady(addonDir, stem))
			return PathToFileUrl(StemPath(addonDir, stem, L".html"));
		return EnsurePanel(addonDir, "live-legendary-vault", LiveAsyncJob::LegendaryLedger,
			"GW2 Legendary Ledger", "The Complete GW2 Legendary Collection");
	}
	bool sync = false;
	if (ParseLegendaryItemUrl(url, &id, &sync))
	{
		char stem[64];
		std::snprintf(stem, sizeof(stem), "live-legendary-detail-%d", id);
		/* Sync forces a full rebuild; normal open serves the cached craft tree
		   instantly when the panel is ready (wiki forge expand is expensive). */
		if (sync)
		{
			char craftStem[64];
			std::snprintf(craftStem, sizeof(craftStem), "live-leg-craft-%d", id);
			DeleteFileW(StemPath(addonDir, stem, L".html").c_str());
			DeleteFileW(StemPath(addonDir, stem, L".ver").c_str());
			DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());
			DeleteFileW(StemPath(addonDir, craftStem, L".json").c_str());
			DeleteFileW(StemPath(addonDir, "live-acc-armory", L".json").c_str());
			DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ver").c_str());
			DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ok").c_str());
		}
		else if (PanelReady(addonDir, stem))
			return PathToFileUrl(StemPath(addonDir, stem, L".html"));
		char title[96];
		std::snprintf(title, sizeof(title), "Legendary craft #%d", id);
		return EnsurePanel(addonDir, stem, LiveAsyncJob::LegendaryDetail,
			"GW2 Legendary Ledger", title, id);
	}
	if (url == "about:live-dailies")
		return EnsurePanel(addonDir, "live-dailies", LiveAsyncJob::Dailies,
			"Live — Today board", "Today — Vault, crafting &amp; bosses");
	if (url == "about:live-news")
		return EnsurePanel(addonDir, "live-news", LiveAsyncJob::News,
			"Live — News &amp; Patch Digest", "News &amp; Patch Digest");
	if (url == "about:live-fashion")
		return EnsurePanel(addonDir, "live-fashion", LiveAsyncJob::Fashion,
			"Live — Fashion Wishlist", "Fashion Wishlist");
	if (url == "about:live-tp")
		return EnsurePanel(addonDir, "live-tp", LiveAsyncJob::Tp,
			"Live — Trading Post Watchlist", "My TP Watchlist");
	/* live-progress demoted — Ledger is primary legendary discovery. */
	if (url == "about:live-progress" || url == "about:legendary-vault")
	{
		return EnsurePanel(addonDir, "live-legendary-vault", LiveAsyncJob::LegendaryLedger,
			"GW2 Legendary Ledger", "The Complete GW2 Legendary Collection");
	}
	if (url == "about:cheatsheets-hub")
		return EnsurePanel(addonDir, "live-cheatsheets-hub", LiveAsyncJob::CheatSheetsHub,
			"Cheat Sheets", "Cheat Sheets");
	if (url == "about:browse-hub")
		return EnsurePanel(addonDir, "live-browse-hub", LiveAsyncJob::BrowseHub,
			"Browse", "Browse");
	if (url.rfind("about:browse-cat-", 0) == 0)
	{
		const std::string slug = url.substr(17);
		const char* cat = LivePanelsBuild::BrowseCategoryFromSlug(slug.c_str());
		if (!cat || !cat[0] || std::strcmp(cat, "Cheat Sheets") == 0)
			return {};
		const std::string stem = std::string("live-browse-cat-") + slug;
		return EnsurePanel(addonDir, stem.c_str(), LiveAsyncJob::BrowseCategory,
			"Browse", cat);
	}
	if (url == "about:gw2-api-check")
		return EnsurePanel(addonDir, "gw2-api-check", LiveAsyncJob::ApiCheck,
			"GW2 API Check", "GW2 API Check");
	return {};
}

namespace
{
	bool UrlMentionsStem(const char* url, const std::string& stem)
	{
		if (!url || !url[0] || stem.empty())
			return false;
		if (std::strstr(url, (stem + ".html").c_str()) != nullptr
			|| std::strstr(url, stem.c_str()) != nullptr)
			return true;
		if (stem == "live-dailies" && std::strstr(url, "about:live-dailies"))
			return true;
		if (stem == "live-news" && std::strstr(url, "about:live-news"))
			return true;
		if (stem == "live-fashion" && std::strstr(url, "about:live-fashion"))
			return true;
		if (stem == "live-tp" && std::strstr(url, "about:live-tp"))
			return true;
		if (stem == "live-progress" && std::strstr(url, "about:live-progress"))
			return true;
		if (stem == "live-legendary-vault" &&
			(std::strstr(url, "about:legendary-vault") || std::strstr(url, "live-legendary-vault")))
			return true;
		if (stem == "live-cheatsheets-hub" &&
			(std::strstr(url, "about:cheatsheets-hub") || std::strstr(url, "live-cheatsheets-hub")))
			return true;
		if (stem == "live-browse-hub" &&
			(std::strstr(url, "about:browse-hub") || std::strstr(url, "live-browse-hub")))
			return true;
		if (stem.rfind("live-browse-cat-", 0) == 0)
		{
			const std::string about = std::string("about:browse-cat-") + stem.substr(16);
			if (std::strstr(url, about.c_str()) || std::strstr(url, stem.c_str()))
				return true;
		}
		if (stem.rfind("live-legendary-detail-", 0) == 0 &&
			(std::strstr(url, stem.c_str()) ||
				std::strstr(url, "about:legendary-vault-item-") ||
				std::strstr(url, "about:legendary-vault-sync-")))
			return true;
		if (stem == "gw2-api-check" && std::strstr(url, "about:gw2-api-check"))
			return true;
		return false;
	}

	void RetryStuckCheatSheetNav(const std::wstring& dir)
	{
		const char* cur = WikiBrowser::CurrentUrlCStr();
		if (!cur || !cur[0] || dir.empty())
			return;
		if (std::strstr(cur, "cheatsheets/") != nullptr)
			return;

		const bool onStub = std::strstr(cur, "opening-cheatsheet") != nullptr;
		size_t n = 0;
		const CheatSheets::Sheet* sheets = CheatSheets::All(&n);
		const CheatSheets::Sheet* hit = nullptr;
		for (size_t i = 0; i < n; ++i)
		{
			if (sheets[i].fileStem && sheets[i].fileStem[0] &&
				std::strstr(cur, sheets[i].fileStem) != nullptr)
			{
				hit = &sheets[i];
				break;
			}
		}
		if (!hit)
		{
			const std::string home = Sites::ResolveUrl(Sites::Active());
			hit = CheatSheets::FindByAbout(home.c_str());
		}
		if (!hit)
			return;
		if (!onStub)
		{
			/* Stale pages/<stem>.html loading shell from older helpers. */
			if (std::strstr(cur, "/pages/") == nullptr && std::strstr(cur, "\\pages\\") == nullptr)
				return;
		}

		const std::string fileUrl = CheatSheets::EnsureFileUrl(dir, *hit);
		if (fileUrl.empty())
			return;
		static DWORD sLastMs = 0;
		static std::string sLastStem;
		const DWORD now = GetTickCount();
		if (sLastStem == hit->fileStem && sLastMs != 0 && (now - sLastMs) < 400u)
			return;
		sLastMs = now;
		sLastStem = hit->fileStem ? hit->fileStem : "";
		WikiBrowser::Navigate(fileUrl);
	}
} // namespace

void LivePanels::Tick()
{
	/* Legacy CEF helper cmd file (TP is ImGui now) — keep cheap no-op if absent. */
	{
		const std::wstring dir = AddonPaths::DataDir();
		if (!dir.empty())
		{
			ProcessTpWatchCmdFile(dir);
			ProcessCraftPlanCmdFile(dir);
			ProcessLegendaryDetailCmdFile(dir);
			ProcessFavCmdFile(dir);
			ProcessOpenSiteCmdFile(dir);
			ProcessOpenAboutCmdFile(dir);
			RetryStuckCheatSheetNav(dir);
		}
	}

	std::vector<LiveReadyNav> ready;
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		ReapJoinableUnlocked();
		ready.swap(gAsync.readyNav);
	}
	if (ready.empty())
		return;

	/* Navigate only — HTML already on disk from the worker. */
	const char* cur = WikiBrowser::CurrentUrlCStr();
	std::vector<LiveReadyNav> keep;
	keep.reserve(ready.size());
	for (LiveReadyNav& nav : ready)
	{
		if (nav.fileUrl.empty() || nav.stem.empty())
			continue;
		bool activeOn = UrlMentionsStem(cur, nav.stem);
		const int n = BrowserTabs::Count();
		const int active = BrowserTabs::ActiveIndex();
		for (int i = 0; i < n && !activeOn; ++i)
		{
			if (i == active && UrlMentionsStem(BrowserTabs::At(i).url.c_str(), nav.stem))
				activeOn = true;
		}
		if (!activeOn)
		{
			if (++nav.waitTicks < 180)
				keep.push_back(std::move(nav));
			continue;
		}
		const bool onStub = cur && std::strstr(cur, "opening-cheatsheet") != nullptr;
		const bool onAbout = cur && std::strncmp(cur, "about:", 6) == 0;
		if ((onStub || onAbout) && !nav.fileUrl.empty())
			WikiBrowser::Navigate(nav.fileUrl);
		/* File is on disk. Native Windows: Reload when already on the live file.
		   Wine: Reload of file:// crashes — Navigate above handles stub→file. */
		else if (!EiRuntime::IsWine())
			WikiBrowser::Reload();
	}
	if (!keep.empty())
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		for (LiveReadyNav& nav : keep)
			gAsync.readyNav.push_back(std::move(nav));
	}
}

void LivePanels::BumpLegendaryVaultOpen(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return;
	/* Stamp-only — keep ledger HTML on disk for CEF history / wait-until-complete poll. */
	DeleteFileW(StemPath(addonDir, "live-acc-armory", L".json").c_str());
	DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ver").c_str());
	DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ok").c_str());
}

void LivePanels::InvalidateTpCache(const std::wstring& addonDir)
{
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		++gAsync.generation;
		for (LiveAsyncJob* j : gAsync.queue)
			delete j;
		gAsync.queue.clear();
		gAsync.readyNav.clear();
		/* Allow a new job for the same stem; old workers discard via generation. */
		gAsync.runningStems.clear();
	}
	if (addonDir.empty())
		return;
	DeleteFileW(StemPath(addonDir, "live-tp", L".ver").c_str());
	DeleteFileW(StemPath(addonDir, "live-tp", L".ok").c_str());
}

void LivePanels::InvalidateCaches(const std::wstring& addonDir)
{
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		++gAsync.generation;
		for (LiveAsyncJob* j : gAsync.queue)
			delete j;
		gAsync.queue.clear();
		gAsync.readyNav.clear();
		gAsync.runningStems.clear();
	}
	if (addonDir.empty())
		return;
	const char* stems[] = {
		"live-dailies", "live-news", "live-fashion", "live-tp", "live-progress",
		"live-legendary-vault", "live-cheatsheets-hub", "live-browse-hub", "gw2-api-check",
		"live-colors", "live-armory", "live-armory-names",
		"live-season", "live-craft", "live-bosses", "live-vault-obj",
		"live-vault-daily", "live-vault-weekly", "live-vault-special",
		"live-acc-armory", "live-chars", "live-chars-detail"
	};
	for (const char* stem : stems)
	{
		/* Never delete .html — CEF session history keeps file:// entries; wiping
		   the file surfaces Chromium ERR_FILE_NOT_FOUND on Back. Stamp (+ json)
		   wipe is enough for EnsurePanel to rebuild on next about: open. */
		DeleteFileW(StemPath(addonDir, stem, L".ver").c_str());
		DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());
		DeleteFileW(StemPath(addonDir, stem, L".json").c_str());
	}
	InvalidateBrowseHubCaches(addonDir);
}

void LivePanels::Shutdown()
{
	std::vector<HANDLE> wait;
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		++gAsync.generation;
		for (LiveAsyncJob* j : gAsync.queue)
			delete j;
		gAsync.queue.clear();
		gAsync.readyNav.clear();
		gAsync.runningStems.clear();
		wait.swap(gAsync.joinable);
	}
	/* Bounded join — never hang Nexus unload on a stuck WinHTTP call. */
	for (HANDLE th : wait)
	{
		if (!th)
			continue;
		WaitForSingleObject(th, 500);
		CloseHandle(th);
	}
}

void LivePanels::NotifyFavoritesChanged()
{
	const std::wstring dir = AddonPaths::DataDir();
	if (dir.empty())
		return;

	/* Stamp-only invalidate — category pages refresh stars; hub no longer lists bookmarks. */
	InvalidateBrowseFavCaches(dir, nullptr);
}
