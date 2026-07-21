#include "BrowserTabs.h"

#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
	struct TabState
	{
		BrowserTabs::Tab tab{};
	};

	struct ClosedTab
	{
		char        siteId[64]{};
		char        title[48]{};
		std::string url;
		bool        pinned = false;
	};

	TabState gTabs[BrowserTabs::kMaxTabs];
	int gCount = 0;
	int gActive = 0;

	ClosedTab gClosed[BrowserTabs::kClosedStack];
	int gClosedCount = 0;

	void SyncSitesFromTab(const BrowserTabs::Tab& tab)
	{
		if (tab.siteId[0] && Sites::SetActiveById(tab.siteId))
		{
			std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
			Settings::SetDirty();
		}
	}

	void FillFromSite(TabState& t, const char* siteId)
	{
		t.tab = BrowserTabs::Tab{};

		if (!siteId || !siteId[0])
			siteId = "home";
		if (!Sites::SetActiveById(siteId))
			Sites::SetActiveById("home");

		const SiteDef& site = Sites::Active();
		std::snprintf(t.tab.siteId, sizeof(t.tab.siteId), "%s", Sites::ActiveId());
		std::snprintf(t.tab.title, sizeof(t.tab.title), "%s",
			site.label ? site.label : Sites::ActiveId());
		t.tab.url = Sites::ResolveUrl(site);
		t.tab.pinned = false;
		std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
	}

	void StashActiveUrl()
	{
		if (gCount <= 0 || gActive < 0 || gActive >= gCount)
			return;
		const std::string cur = WikiBrowser::CurrentUrl();
		if (!cur.empty())
			gTabs[gActive].tab.url = cur;
	}

	void PushClosed(const BrowserTabs::Tab& tab)
	{
		if (gClosedCount < BrowserTabs::kClosedStack)
			++gClosedCount;
		else
		{
			for (int i = 0; i < BrowserTabs::kClosedStack - 1; ++i)
				gClosed[i] = std::move(gClosed[i + 1]);
		}
		ClosedTab& c = gClosed[gClosedCount - 1];
		c = ClosedTab{};
		std::snprintf(c.siteId, sizeof(c.siteId), "%s", tab.siteId);
		std::snprintf(c.title, sizeof(c.title), "%s", tab.title);
		c.url = tab.url;
		c.pinned = tab.pinned;
	}

	void SyncSlotToHelper(int slot, bool activate)
	{
		if (slot < 0 || slot >= gCount)
			return;
		const std::string& url = gTabs[slot].tab.url;
		const char* start = url.empty() ? "about:blank" : url.c_str();
		WikiBrowser::CreateTab(slot, start);
		if (activate)
		{
			WikiBrowser::ActivateTab(slot);
			/* Navigate after activate — resolves about:helper-home / about:raid-food
			   / cheat sheets to file:// (CreateTab alone used to leave a white CEF page). */
			if (!url.empty())
				WikiBrowser::Navigate(url);
		}
	}

	void SyncAllToHelper()
	{
		if (gCount <= 0)
			return;
		for (int i = 0; i < gCount; ++i)
			SyncSlotToHelper(i, false);
		WikiBrowser::ActivateTab(gActive);
		SyncSitesFromTab(gTabs[gActive].tab);
		if (!gTabs[gActive].tab.url.empty())
			WikiBrowser::Navigate(gTabs[gActive].tab.url);
	}
}

void BrowserTabs::EnsureDefault()
{
	if (gCount > 0)
		return;
	const char* land = (G::DefaultSiteId[0] ? G::DefaultSiteId : "home");
	FillFromSite(gTabs[0], land);
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
		std::fprintf(f, "Tab%dPinned=%d\n", i, gTabs[i].tab.pinned ? 1 : 0);
	}
}

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

	const std::string cur = WikiBrowser::CurrentUrl();
	if (!cur.empty() && cur != gTabs[gActive].tab.url)
		gTabs[gActive].tab.url = cur;
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
	FillFromSite(gTabs[gCount], c.siteId[0] ? c.siteId : "home");
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
	const bool keepPin = gTabs[gActive].tab.pinned;
	FillFromSite(gTabs[gActive], "home");
	gTabs[gActive].tab.pinned = keepPin;
	Settings::SetDirty();
	SyncSitesFromTab(gTabs[gActive].tab);
	SyncSlotToHelper(gActive, true);
}

void BrowserTabs::Reload()
{
	WikiBrowser::Reload();
}
