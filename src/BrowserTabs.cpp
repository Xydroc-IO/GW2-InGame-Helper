#include "BrowserTabs.h"

#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	struct TabState
	{
		BrowserTabs::Tab tab{};
		std::vector<std::string> back;
		std::vector<std::string> forward;
		bool suppressHistory = false;
	};

	TabState gTabs[BrowserTabs::kMaxTabs];
	int gCount = 0;
	int gActive = 0;

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
		t.back.clear();
		t.forward.clear();
		t.suppressHistory = true;

		if (!siteId || !siteId[0])
			siteId = "home";
		if (!Sites::SetActiveById(siteId))
			Sites::SetActiveById("home");

		const SiteDef& site = Sites::Active();
		std::snprintf(t.tab.siteId, sizeof(t.tab.siteId), "%s", Sites::ActiveId());
		std::snprintf(t.tab.title, sizeof(t.tab.title), "%s",
			site.label ? site.label : Sites::ActiveId());
		t.tab.url = Sites::ResolveUrl(site);
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

	void NavigateTab(TabState& t)
	{
		t.suppressHistory = true;
		SyncSitesFromTab(t.tab);
		if (!t.tab.url.empty())
			WikiBrowser::Navigate(t.tab.url);
		else
			WikiBrowser::NavigateActiveSite();
	}

	void PushHistory(TabState& t, const std::string& fromUrl)
	{
		if (t.suppressHistory)
			return;
		if (fromUrl.empty() || fromUrl == t.tab.url)
			return;
		t.back.push_back(fromUrl);
		if (t.back.size() > 64)
			t.back.erase(t.back.begin());
		t.forward.clear();
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
	NavigateTab(gTabs[gActive]);
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

	/* TabNSite / TabNUrl */
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
	}
}

void BrowserTabs::Tick()
{
	EnsureDefault();
	if (gActive < 0 || gActive >= gCount)
		return;

	TabState& t = gTabs[gActive];
	const std::string cur = WikiBrowser::CurrentUrl();
	if (cur.empty())
		return;

	if (t.suppressHistory)
	{
		t.tab.url = cur;
		t.suppressHistory = false;
		return;
	}

	if (cur != t.tab.url)
	{
		PushHistory(t, t.tab.url);
		t.tab.url = cur;
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
	FillFromSite(gTabs[gActive], siteId);
	Settings::SetDirty();
	if (navigate)
		NavigateTab(gTabs[gActive]);
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
		NavigateTab(gTabs[gActive]);
	return gActive;
}

void BrowserTabs::Activate(int index)
{
	EnsureDefault();
	if (index < 0 || index >= gCount || index == gActive)
		return;

	StashActiveUrl();
	gActive = index;
	NavigateTab(gTabs[gActive]);
}

void BrowserTabs::Close(int index)
{
	EnsureDefault();
	if (gCount <= 1 || index < 0 || index >= gCount)
		return;

	const bool closingActive = (index == gActive);
	if (closingActive)
		StashActiveUrl();

	for (int i = index; i < gCount - 1; ++i)
		gTabs[i] = std::move(gTabs[i + 1]);
	gTabs[gCount - 1] = TabState{};
	--gCount;

	if (gActive > index)
		--gActive;
	else if (gActive >= gCount)
		gActive = gCount - 1;

	if (closingActive)
		NavigateTab(gTabs[gActive]);
	else
		SyncSitesFromTab(gTabs[gActive].tab);
}

bool BrowserTabs::CanGoBack()
{
	EnsureDefault();
	if (gCount > 1)
		return !gTabs[gActive].back.empty();
	return WikiBrowser::CanGoBack() || !gTabs[gActive].back.empty();
}

bool BrowserTabs::CanGoForward()
{
	EnsureDefault();
	if (gCount > 1)
		return !gTabs[gActive].forward.empty();
	return WikiBrowser::CanGoForward() || !gTabs[gActive].forward.empty();
}

void BrowserTabs::GoBack()
{
	EnsureDefault();
	TabState& t = gTabs[gActive];
	if (!t.back.empty())
	{
		t.forward.push_back(t.tab.url);
		t.tab.url = t.back.back();
		t.back.pop_back();
		t.suppressHistory = true;
		WikiBrowser::Navigate(t.tab.url);
		return;
	}
	if (gCount == 1)
		WikiBrowser::GoBack();
}

void BrowserTabs::GoForward()
{
	EnsureDefault();
	TabState& t = gTabs[gActive];
	if (!t.forward.empty())
	{
		t.back.push_back(t.tab.url);
		t.tab.url = t.forward.back();
		t.forward.pop_back();
		t.suppressHistory = true;
		WikiBrowser::Navigate(t.tab.url);
		return;
	}
	if (gCount == 1)
		WikiBrowser::GoForward();
}

void BrowserTabs::GoHome()
{
	EnsureDefault();
	FillFromSite(gTabs[gActive], "home");
	Settings::SetDirty();
	NavigateTab(gTabs[gActive]);
}

void BrowserTabs::Reload()
{
	WikiBrowser::Reload();
}
