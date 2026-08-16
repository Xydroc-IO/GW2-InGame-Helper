#include "BrowserTabs.h"
#include "BrowserTabsInternal.h"

#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace BrowserTabsDetail;

void BrowserTabs::EnsureDefault()
{
	if (gCount > 0)
		return;
	const char* land = "browse";
	if (G::DefaultSiteId[0] && Sites::IndexOfId(G::DefaultSiteId) >= 0)
		land = G::DefaultSiteId;
	if (Sites::IndexOfId(land) < 0)
		land = "browse";
	FillFromSite(gTabs[0], land);
	if (Sites::IndexOfId("browse") < 0 &&
		(std::strcmp(land, "browse") == 0 || gTabs[0].tab.url.empty()))
	{
		std::snprintf(gTabs[0].tab.siteId, sizeof(gTabs[0].tab.siteId), "browse");
		std::snprintf(gTabs[0].tab.title, sizeof(gTabs[0].tab.title), "Browse");
		gTabs[0].tab.url = "about:browse-hub";
	}
	gCount = 1;
	gActive = 0;
	Settings::SetDirty();
}

void BrowserTabs::NavigateActive()
{
	EnsureDefault();
	SyncAllToHelper();
}

void BrowserTabs::PrepareSave()
{
	StashActiveUrl();
}

void BrowserTabs::ParseKey(const char* key, const char* val)
{
	if (!key || !val)
		return;

	if (std::strcmp(key, "TabCount") == 0)
	{
		int n = std::atoi(val);
		if (n < 0) n = 0;
		if (n > kMaxTabs) n = kMaxTabs;
		gCount = n;
		return;
	}
	if (std::strcmp(key, "ActiveTab") == 0)
	{
		gActive = std::atoi(val);
		return;
	}

	/* TabNSite / TabNUrl / TabNPinned */
	if (std::strncmp(key, "Tab", 3) != 0)
		return;
	const char* p = key + 3;
	if (*p < '0' || *p > '9')
		return;
	int idx = 0;
	while (*p >= '0' && *p <= '9')
	{
		idx = idx * 10 + (*p - '0');
		++p;
	}
	if (idx < 0 || idx >= kMaxTabs)
		return;

	if (idx + 1 > gCount)
		gCount = idx + 1;

	if (std::strcmp(p, "Site") == 0)
	{
		std::snprintf(gTabs[idx].tab.siteId, sizeof(gTabs[idx].tab.siteId), "%s", val);
		const int si = Sites::IndexOfId(val);
		if (si >= 0)
		{
			size_t n = 0;
			const SiteDef* sites = Sites::All(&n);
			if (sites && sites[si].label)
				std::snprintf(gTabs[idx].tab.title, sizeof(gTabs[idx].tab.title), "%s", sites[si].label);
		}
		if (!gTabs[idx].tab.title[0])
			std::snprintf(gTabs[idx].tab.title, sizeof(gTabs[idx].tab.title), "%s", val);
	}
	else if (std::strcmp(p, "Url") == 0)
	{
		gTabs[idx].tab.url = val;
	}
	else if (std::strcmp(p, "Title") == 0)
	{
		std::snprintf(gTabs[idx].tab.title, sizeof(gTabs[idx].tab.title), "%s", val);
	}
	else if (std::strcmp(p, "Pinned") == 0)
	{
		gTabs[idx].tab.pinned = (val[0] == '1' || val[0] == 't' || val[0] == 'T' || val[0] == 'y');
	}
}

void BrowserTabs::FinalizeLoad()
{
	/* Drop tabs with unknown site ids; keep URL if site still valid. */
	int w = 0;
	for (int i = 0; i < gCount && i < kMaxTabs; ++i)
	{
		/* Migrate former How-to-use landing tabs → Browse hub. */
		const bool wasHome = std::strcmp(gTabs[i].tab.siteId, "home") == 0;
		const bool wasHelperHome =
			gTabs[i].tab.url.find("helper-home") != std::string::npos ||
			gTabs[i].tab.url == "about:helper-home";
		if (wasHome && (wasHelperHome || gTabs[i].tab.url.empty()) &&
			Sites::IndexOfId("browse") >= 0)
		{
			std::snprintf(gTabs[i].tab.siteId, sizeof(gTabs[i].tab.siteId), "browse");
			gTabs[i].tab.url = "about:browse-hub";
			std::snprintf(gTabs[i].tab.title, sizeof(gTabs[i].tab.title), "Browse");
		}

		if (!gTabs[i].tab.siteId[0] || Sites::IndexOfId(gTabs[i].tab.siteId) < 0)
			continue;
		if (gTabs[i].tab.url.empty())
		{
			Sites::SetActiveById(gTabs[i].tab.siteId);
			gTabs[i].tab.url = Sites::ResolveUrl(Sites::Active());
		}
		if (!gTabs[i].tab.title[0])
		{
			const int si = Sites::IndexOfId(gTabs[i].tab.siteId);
			size_t n = 0;
			const SiteDef* sites = Sites::All(&n);
			if (si >= 0 && sites && sites[si].label)
				std::snprintf(gTabs[i].tab.title, sizeof(gTabs[i].tab.title), "%s", sites[si].label);
		}
		if (w != i)
			gTabs[w] = std::move(gTabs[i]);
		++w;
	}
	for (int i = w; i < kMaxTabs; ++i)
		gTabs[i] = TabState{};
	gCount = w;

	if (gCount <= 0)
	{
		EnsureDefault();
		return;
	}
	if (gActive < 0 || gActive >= gCount)
		gActive = 0;
	SyncSitesFromTab(gTabs[gActive].tab);
}

void BrowserTabs::WriteSettings(FILE* f)
{
	if (!f)
		return;
	PrepareSave();
	EnsureDefault();
	std::fprintf(f, "TabCount=%d\n", gCount);
	std::fprintf(f, "ActiveTab=%d\n", gActive);
	for (int i = 0; i < gCount; ++i)
	{
		std::fprintf(f, "Tab%dSite=%s\n", i, gTabs[i].tab.siteId);
		std::fprintf(f, "Tab%dUrl=%s\n", i, gTabs[i].tab.url.c_str());
		std::fprintf(f, "Tab%dTitle=%s\n", i, gTabs[i].tab.title);
		std::fprintf(f, "Tab%dPinned=%d\n", i, gTabs[i].tab.pinned ? 1 : 0);
	}
}

int BrowserTabs::Count()
{
	return gCount;
}

int BrowserTabs::ActiveIndex()
{
	return gActive;
}

const BrowserTabs::Tab& BrowserTabs::At(int index)
{
	EnsureDefault();
	if (index < 0 || index >= gCount)
		return gTabs[0].tab;
	return gTabs[index].tab;
}

void BrowserTabs::OpenInActive(const char* siteId, bool navigate)
{
	EnsureDefault();
	/* Side-rail Browse while already on hub — skip no-op file:// reload (Wine CEF). */
	if (siteId && std::strcmp(siteId, "browse") == 0)
	{
		auto isBrowseHub = [](const char* u) -> bool {
			if (!u || !u[0])
				return false;
			if (std::strcmp(u, "about:browse-hub") == 0)
				return true;
			return std::strstr(u, "live-browse-hub.html") != nullptr;
		};
		const char* cefCur = WikiBrowser::CurrentUrlCStr();
		if (isBrowseHub(gTabs[gActive].tab.url.c_str()) || isBrowseHub(cefCur))
		{
			WikiBrowser::ActivateTab(gActive);
			return;
		}
	}
	/* Keep CEF history coherent — stash live URL before replacing the tab site. */
	StashActiveUrl();
	const bool keepPin = gTabs[gActive].tab.pinned;
	FillFromSite(gTabs[gActive], siteId);
	gTabs[gActive].tab.pinned = keepPin;
	Settings::SetDirty();
	if (navigate)
	{
		SyncSitesFromTab(gTabs[gActive].tab);
		SyncSlotToHelper(gActive, true);
	}
}

void BrowserTabs::OpenUrlInActive(const char* siteId, const std::string& url)
{
	EnsureDefault();
	if (url.empty())
		return;
	auto sameUrl = [&](const char* u) -> bool {
		return u && u[0] && url == u;
	};
	/* Re-clicking API Check must rebuild; other about: hubs skip a no-op reload. */
	const bool forceReload = url.find("gw2-api-check") != std::string::npos;
	const char* cefCur = WikiBrowser::CurrentUrlCStr();
	if (!forceReload &&
		(sameUrl(gTabs[gActive].tab.url.c_str()) || sameUrl(cefCur)))
	{
		WikiBrowser::ActivateTab(gActive);
		return;
	}
	StashActiveUrl();
	const bool keepPin = gTabs[gActive].tab.pinned;
	FillFromSite(gTabs[gActive], siteId && siteId[0] ? siteId : "browse");
	gTabs[gActive].tab.pinned = keepPin;
	gTabs[gActive].tab.url = url;
	Settings::SetDirty();
	SyncSitesFromTab(gTabs[gActive].tab);
	SyncSlotToHelper(gActive, true);
}

int BrowserTabs::OpenNew(const char* siteId, bool navigate)
{
	EnsureDefault();
	if (gCount >= kMaxTabs)
		return -1;

	StashActiveUrl();
	FillFromSite(gTabs[gCount], siteId);
	gActive = gCount;
	++gCount;
	Settings::SetDirty();
	if (navigate)
	{
		SyncSitesFromTab(gTabs[gActive].tab);
		SyncSlotToHelper(gActive, true);
	}
	return gActive;
}

int BrowserTabs::OpenNewUrl(const char* siteId, const std::string& url)
{
	EnsureDefault();
	if (gCount >= kMaxTabs)
		return -1;

	StashActiveUrl();
	FillFromSite(gTabs[gCount], siteId && siteId[0] ? siteId : Sites::ActiveId());
	if (!url.empty())
		gTabs[gCount].tab.url = url;
	gActive = gCount;
	++gCount;
	Settings::SetDirty();
	SyncSitesFromTab(gTabs[gActive].tab);
	SyncSlotToHelper(gActive, true);
	return gActive;
}

void BrowserTabs::Activate(int index)
{
	EnsureDefault();
	if (index < 0 || index >= gCount || index == gActive)
		return;

	StashActiveUrl();
	gActive = index;
	SyncSitesFromTab(gTabs[gActive].tab);
	/* Missing CEF slot (create was dropped at startup) — create+navigate.
	   Existing live tabs only activate (no reload). */
	if (!WikiBrowser::HasTab(gActive))
		SyncSlotToHelper(gActive, true);
	else
		WikiBrowser::ActivateTab(gActive);
	Settings::SetDirty();
}

void BrowserTabs::Close(int index)
{
	EnsureDefault();
	if (gCount <= 1 || index < 0 || index >= gCount)
		return;
	if (gTabs[index].tab.pinned)
		return;

	const bool closingActive = (index == gActive);
	if (closingActive)
		StashActiveUrl();

	PushClosed(gTabs[index].tab);
	WikiBrowser::CloseTab(index);

	for (int i = index; i < gCount - 1; ++i)
		gTabs[i] = std::move(gTabs[i + 1]);
	gTabs[gCount - 1] = TabState{};
	--gCount;

	if (gActive > index)
		--gActive;
	else if (gActive >= gCount)
		gActive = gCount - 1;

	SyncSitesFromTab(gTabs[gActive].tab);
	if (closingActive)
		WikiBrowser::ActivateTab(gActive);
	Settings::SetDirty();
}

void BrowserTabs::TogglePin(int index)
{
	EnsureDefault();
	if (index < 0 || index >= gCount)
		return;
	gTabs[index].tab.pinned = !gTabs[index].tab.pinned;
	Settings::SetDirty();
}

bool BrowserTabs::CanReopenClosed()
{
	return gClosedCount > 0 && gCount < kMaxTabs;
}

int BrowserTabs::ReopenClosed()
{
	EnsureDefault();
	if (gClosedCount <= 0 || gCount >= kMaxTabs)
		return -1;

	ClosedTab c = std::move(gClosed[gClosedCount - 1]);
	--gClosedCount;

	StashActiveUrl();
	FillFromSite(gTabs[gCount], c.siteId[0] ? c.siteId : "browse");
	if (c.title[0])
		std::snprintf(gTabs[gCount].tab.title, sizeof(gTabs[gCount].tab.title), "%s", c.title);
	if (!c.url.empty())
		gTabs[gCount].tab.url = c.url;
	gTabs[gCount].tab.pinned = c.pinned;
	gActive = gCount;
	++gCount;
	Settings::SetDirty();
	SyncSitesFromTab(gTabs[gActive].tab);
	SyncSlotToHelper(gActive, true);
	return gActive;
}
