#include "LivePanels.h"

#include "LivePanelsInternal.h"

#include "AddonPaths.h"
#include "WikiBrowser.h"

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
		std::strncmp(url, "about:live-tp-remove-", 21) == 0)
		return true;
	return std::strcmp(url, "about:live-dailies") == 0 ||
		std::strcmp(url, "about:live-news") == 0 ||
		std::strcmp(url, "about:live-fashion") == 0 ||
		std::strcmp(url, "about:live-tp") == 0 ||
		std::strcmp(url, "about:live-progress") == 0;
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
		std::strstr(url, "live-progress.html") != nullptr;
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
	return {};
}

void LivePanels::Tick()
{
	/* Legacy CEF helper cmd file (TP is ImGui now) — keep cheap no-op if absent. */
	{
		const std::wstring dir = AddonPaths::DataDir();
		if (!dir.empty())
			ProcessTpWatchCmdFile(dir);
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
			(nav.stem == "live-progress" && std::strstr(cur, "about:live-progress"));
		if (onPanel)
			WikiBrowser::Navigate(nav.fileUrl);
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
