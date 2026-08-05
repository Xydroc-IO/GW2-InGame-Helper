#include "LivePanels.h"

#include "LivePanelsInternal.h"
#include "LivePanelsBuild.h"

#include "AddonPaths.h"
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
			"Live — Dailies &amp; Vault", "Dailies &amp; Wizard’s Vault");
	if (url == "about:live-news")
		return EnsurePanel(addonDir, "live-news", LiveAsyncJob::News,
			"Live — News &amp; Patch Digest", "News &amp; Patch Digest");
	if (url == "about:live-fashion")
		return EnsurePanel(addonDir, "live-fashion", LiveAsyncJob::Fashion,
			"Live — Fashion Wishlist", "Fashion Wishlist");
	if (url == "about:live-tp")
		return EnsurePanel(addonDir, "live-tp", LiveAsyncJob::Tp,
			"Live — Trading Post Watchlist", "My TP Watchlist");
	if (url == "about:live-progress")
		return EnsurePanel(addonDir, "live-progress", LiveAsyncJob::Progress,
			"Live — Legendaries &amp; Characters", "Legendaries &amp; Characters");
	if (url == "about:legendary-vault")
	{
		/* Always re-fetch /v2/account/legendaryarmory so Owned/Missing stays current
		   without requiring Sync craft tree. */
		DeleteFileW(StemPath(addonDir, "live-acc-armory", L".json").c_str());
		DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ver").c_str());
		DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ok").c_str());
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
	if (!cur)
		return;
	for (const LiveReadyNav& nav : ready)
	{
		if (nav.fileUrl.empty() || nav.stem.empty())
			continue;
		const bool onPanel =
			std::strstr(cur, (nav.stem + ".html").c_str()) != nullptr ||
			(nav.stem == "live-dailies" && std::strstr(cur, "about:live-dailies")) ||
			(nav.stem == "live-news" && std::strstr(cur, "about:live-news")) ||
			(nav.stem == "live-fashion" && std::strstr(cur, "about:live-fashion")) ||
			(nav.stem == "live-tp" && std::strstr(cur, "about:live-tp")) ||
			(nav.stem == "live-progress" && std::strstr(cur, "about:live-progress")) ||
			(nav.stem == "live-legendary-vault" &&
				(std::strstr(cur, "about:legendary-vault") ||
					std::strstr(cur, "live-legendary-vault"))) ||
			(nav.stem == "live-cheatsheets-hub" &&
				(std::strstr(cur, "about:cheatsheets-hub") ||
					std::strstr(cur, "live-cheatsheets-hub"))) ||
			(nav.stem == "live-browse-hub" &&
				(std::strstr(cur, "about:browse-hub") ||
					std::strstr(cur, "live-browse-hub"))) ||
			(nav.stem.rfind("live-browse-cat-", 0) == 0 &&
				std::strstr(cur, nav.stem.c_str())) ||
			(nav.stem.rfind("live-legendary-detail-", 0) == 0 &&
				(std::strstr(cur, nav.stem.c_str()) ||
					std::strstr(cur, "about:legendary-vault-item-") ||
					std::strstr(cur, "about:legendary-vault-sync-"))) ||
			(nav.stem == "gw2-api-check" && std::strstr(cur, "about:gw2-api-check"));
		if (onPanel)
		{
			/* Same file:// path as the loading shell — Navigate is a no-op in CEF;
			   Reload (or cache-bust) is required, same lesson as API Check waits. */
			if (std::strstr(cur, (nav.stem + ".html").c_str()) != nullptr)
				WikiBrowser::Reload();
			else
				WikiBrowser::Navigate(nav.fileUrl);
		}
	}
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
		DeleteFileW(StemPath(addonDir, stem, L".html").c_str());
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
