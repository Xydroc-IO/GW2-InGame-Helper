#include "BrowserTabs.h"
#include "BrowserTabsInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "LivePanels.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace BrowserTabsDetail;

void BrowserTabs::Tick()
{
	EnsureDefault();

	/* CREATE_TAB is dropped while the helper is still starting — resync once ready. */
	static bool sSyncedReady = false;
	const bool ready = WikiBrowser::IsReady();
	if (ready && !sSyncedReady)
	{
		SyncAllToHelper();
		sSyncedReady = true;
	}
	else if (!ready)
	{
		sSyncedReady = false;
	}

	if (gActive < 0 || gActive >= gCount)
		return;

	/* CLOSE/ACTIVATE are async IPC. Until the helper's active_tab matches our
	   gActive AND that slot has a live browser, CurrentUrl/CurrentTitle still
	   describe the dying (or previous) browser — writing them into gTabs[gActive]
	   overwrites the neighbour tab. */
	if (WikiBrowser::ActiveTabSlot() != gActive || !WikiBrowser::HasTab(gActive))
		return;

	BrowserTabs::Tab& active = gTabs[gActive].tab;
	bool urlChanged = false;
	const char* cur = WikiBrowser::CurrentUrlCStr();
	if (cur && cur[0] && std::strcmp(cur, active.url.c_str()) != 0)
	{
		active.url = cur;
		urlChanged = true;
		Settings::SetDirty();
	}

	if (urlChanged)
	{
		RefreshTabLabelFromUrl(active, true);
		SyncSitesFromTab(active);
	}
	else
	{
		/* Late CEF title — cheap path; skip BestMatch unless title actually changed. */
		static char sLastPageTitle[128]{};
		const char* page = WikiBrowser::CurrentTitleCStr();
		if (page && page[0] && std::strcmp(page, "about:blank") != 0 &&
			std::strncmp(sLastPageTitle, page, sizeof(sLastPageTitle) - 1) != 0)
		{
			std::snprintf(sLastPageTitle, sizeof(sLastPageTitle), "%s", page);
			const int match = Sites::BestMatchForUrl(active.url);
			if (match < 0)
				ApplyTabTitle(active, page);
			else
			{
				size_t n = 0;
				const SiteDef* sites = Sites::All(&n);
				const char* home = (sites && match >= 0) ? sites[match].homeUrl : nullptr;
				bool atHome = false;
				if (home && home[0] &&
					(active.url == home ||
						(active.url.size() == std::strlen(home) + 1 &&
							active.url.compare(0, std::strlen(home), home) == 0 && active.url.back() == '/') ||
						std::strncmp(home, "about:", 6) == 0 ||
						std::strncmp(home, "file:", 5) == 0))
					atHome = true;
				if (!atHome)
					ApplyTabTitle(active, page);
			}
		}
	}
}

bool BrowserTabs::CanGoBack()
{
	return WikiBrowser::CanGoBack();
}

bool BrowserTabs::CanGoForward()
{
	return WikiBrowser::CanGoForward();
}

void BrowserTabs::GoBack()
{
	WikiBrowser::GoBack();
}

void BrowserTabs::GoForward()
{
	WikiBrowser::GoForward();
}

void BrowserTabs::GoHome()
{
	EnsureDefault();

	/* Already on the Home landing (Browse hub). Do not rewrite live-browse-hub.html
	   and force CREATE_TAB/load_url — that crash-looped the CEF helper under Wine
	   (exit 0x80000003 / 2147483651) when Home was pressed while already Home. */
	auto isBrowseHome = [](const char* u) -> bool {
		if (!u || !u[0])
			return false;
		if (std::strcmp(u, "about:browse-hub") == 0)
			return true;
		return std::strstr(u, "live-browse-hub.html") != nullptr;
	};
	const char* cefCur = WikiBrowser::CurrentUrlCStr();
	if (isBrowseHome(gTabs[gActive].tab.url.c_str()) || isBrowseHome(cefCur))
	{
		WikiBrowser::ActivateTab(gActive);
		return;
	}

	StashActiveUrl();
	/* Home always lands on the Browse hub (factory default). Settings
	   DefaultSiteId still seeds empty-tab / first-run via EnsureDefault. */
	const char* land = "browse";
	if (Sites::IndexOfId(land) < 0 && G::DefaultSiteId[0] &&
		Sites::IndexOfId(G::DefaultSiteId) >= 0)
		land = G::DefaultSiteId;
	const bool keepPin = gTabs[gActive].tab.pinned;
	FillFromSite(gTabs[gActive], land);
	gTabs[gActive].tab.pinned = keepPin;
	/* If catalog lacked browse, still force the hub URL. */
	if (std::strcmp(gTabs[gActive].tab.siteId, "browse") != 0 &&
		Sites::IndexOfId("browse") < 0)
	{
		std::snprintf(gTabs[gActive].tab.siteId, sizeof(gTabs[gActive].tab.siteId), "browse");
		std::snprintf(gTabs[gActive].tab.title, sizeof(gTabs[gActive].tab.title), "Browse");
		gTabs[gActive].tab.url = "about:browse-hub";
	}
	Settings::SetDirty();
	SyncSitesFromTab(gTabs[gActive].tab);
	SyncSlotToHelper(gActive, true);
}

void BrowserTabs::Reload()
{
	EnsureDefault();
	const std::string& url = gTabs[gActive].tab.url;
	/* Live panels: rebuild HTML from API then navigate to file:// (CEF reload alone
	   would keep a stale/shell page, and about:live-* is blocked by Chromium). */
	if (LivePanels::IsLiveUrl(url.c_str()) || LivePanels::IsLiveAbout(url.c_str()) ||
		LivePanels::IsLiveAbout(Sites::ResolveUrl(Sites::Active()).c_str()))
	{
		LivePanels::InvalidateCaches(AddonPaths::DataDir());
		const char* about = nullptr;
		if (LivePanels::IsLiveAbout(url.c_str()))
			about = url.c_str();
		else if (url.find("live-dailies") != std::string::npos ||
			std::strcmp(gTabs[gActive].tab.siteId, "live_dailies") == 0)
			about = "about:live-dailies";
		else if (url.find("live-news") != std::string::npos ||
			std::strcmp(gTabs[gActive].tab.siteId, "live_news") == 0)
			about = "about:live-news";
		else if (url.find("live-fashion") != std::string::npos ||
			std::strcmp(gTabs[gActive].tab.siteId, "live_fashion") == 0)
			about = "about:live-fashion";
		else if (url.find("live-tp") != std::string::npos ||
			std::strcmp(gTabs[gActive].tab.siteId, "live_tp") == 0)
			about = "about:live-tp";
		else if (url.find("live-progress") != std::string::npos ||
			std::strcmp(gTabs[gActive].tab.siteId, "live_progress") == 0)
			about = "about:live-progress";
		else if (url.find("gw2-api-check") != std::string::npos)
			about = "about:gw2-api-check";
		else
		{
			const std::string resolved = Sites::ResolveUrl(Sites::Active());
			if (LivePanels::IsLiveAbout(resolved.c_str()))
				about = resolved.c_str();
		}
		if (about)
		{
			gTabs[gActive].tab.url = about;
			WikiBrowser::Navigate(about);
			return;
		}
	}
	WikiBrowser::Reload();
}
