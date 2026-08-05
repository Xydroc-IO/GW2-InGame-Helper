#include "BrowserTabsInternal.h"

#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace BrowserTabsDetail
{
	TabState gTabs[BrowserTabs::kMaxTabs];
	int gCount = 0;
	int gActive = 0;

	ClosedTab gClosed[BrowserTabs::kClosedStack];
	int gClosedCount = 0;

	void SyncSitesFromTab(const BrowserTabs::Tab& tab)
	{
		if (!tab.siteId[0] || !Sites::SetActiveById(tab.siteId))
			return;
		if (std::strcmp(G::ActiveSiteId, Sites::ActiveId()) == 0)
			return;
		std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
		Settings::SetDirty();
	}

	void FillFromSite(TabState& t, const char* siteId)
	{
		t.tab = BrowserTabs::Tab{};

		if (!siteId || !siteId[0])
			siteId = "browse";
		if (!Sites::SetActiveById(siteId) && !Sites::SetActiveById("browse"))
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
		/* Skip until CEF has a live browser for this slot and IPC agrees —
		   otherwise CurrentUrl still describes the previous/dying tab. */
		if (!WikiBrowser::HasTab(gActive) || WikiBrowser::ActiveTabSlot() != gActive)
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

	void ApplyTabTitle(BrowserTabs::Tab& tab, const char* title)
	{
		if (!title || !title[0])
			return;
		char cleaned[96]{};
		std::snprintf(cleaned, sizeof(cleaned), "%s", title);
		/* Strip common trailing site brands for shorter tab labels. */
		auto stripSuffix = [](char* s, const char* suf) {
			const size_t n = std::strlen(s);
			const size_t m = std::strlen(suf);
			if (n > m && std::strcmp(s + (n - m), suf) == 0)
				s[n - m] = 0;
		};
		stripSuffix(cleaned, " - Guild Wars 2 Wiki");
		stripSuffix(cleaned, " | Guild Wars 2 Wiki");
		stripSuffix(cleaned, " — Guild Wars 2");
		stripSuffix(cleaned, " - Google Search");
		stripSuffix(cleaned, " - Google");
		/* Trim whitespace / control chars (settings.ini safety). */
		size_t len = std::strlen(cleaned);
		while (len > 0 && (cleaned[len - 1] == ' ' || cleaned[len - 1] == '-' ||
			cleaned[len - 1] == '|' || cleaned[len - 1] == '\r' || cleaned[len - 1] == '\n'))
			cleaned[--len] = 0;
		for (size_t i = 0; cleaned[i]; ++i)
		{
			if (cleaned[i] == '\r' || cleaned[i] == '\n' || cleaned[i] == '=')
				cleaned[i] = ' ';
		}
		if (!cleaned[0])
			return;
		/* Title updates are UI-only — do not SetDirty (was freezing GW2 via
		   per-frame settings.ini writes). Persist on next real save. */
		if (std::strncmp(tab.title, cleaned, sizeof(tab.title) - 1) != 0)
			std::snprintf(tab.title, sizeof(tab.title), "%s", cleaned);
	}

	void RefreshTabLabelFromUrl(BrowserTabs::Tab& tab, bool preferPageTitle)
	{
		const std::string& url = tab.url;
		if (url.empty())
			return;

		const int match = Sites::BestMatchForUrl(url);
		if (match >= 0)
		{
			size_t n = 0;
			const SiteDef* sites = Sites::All(&n);
			if (sites && sites[match].id)
			{
				if (std::strcmp(tab.siteId, sites[match].id) != 0)
				{
					std::snprintf(tab.siteId, sizeof(tab.siteId), "%s", sites[match].id);
					Settings::SetDirty();
				}
				/* Deep pages: use document title. Site home: short registry label. */
				const char* home = sites[match].homeUrl;
				bool atHome = false;
				if (home && home[0])
				{
					if (url == home)
						atHome = true;
					else if (url.size() == std::strlen(home) + 1 &&
						url.compare(0, std::strlen(home), home) == 0 && url.back() == '/')
						atHome = true;
					else if (std::strncmp(home, "about:", 6) == 0 || std::strncmp(home, "file:", 5) == 0)
						atHome = true;
				}
				if (!atHome && preferPageTitle)
				{
					const std::string page = WikiBrowser::CurrentTitle();
					if (!page.empty() && page != "about:blank")
					{
						ApplyTabTitle(tab, page.c_str());
						return;
					}
				}
				if (sites[match].label)
					ApplyTabTitle(tab, sites[match].label);
				return;
			}
		}

		if (preferPageTitle)
		{
			const std::string page = WikiBrowser::CurrentTitle();
			if (!page.empty() && page != "about:blank")
			{
				ApplyTabTitle(tab, page.c_str());
				return;
			}
		}

		/* Hostname fallback */
		std::string host = url;
		const size_t scheme = host.find("://");
		if (scheme != std::string::npos)
			host = host.substr(scheme + 3);
		if (host.rfind("www.", 0) == 0)
			host = host.substr(4);
		const size_t slash = host.find('/');
		if (slash != std::string::npos)
			host = host.substr(0, slash);
		if (!host.empty() && host != "about:blank")
			ApplyTabTitle(tab, host.c_str());
	}

	void SyncSlotToHelper(int slot, bool activate, bool forceNavigate)
	{
		if (slot < 0 || slot >= gCount)
			return;
		const std::string& url = gTabs[slot].tab.url;
		const char* start = url.empty() ? "about:blank" : url.c_str();
		/* CreateTab loads this slot only (new browser start-URL, or NavigateSlot).
		   Do NOT call WikiBrowser::Navigate here — that targets the active CEF
		   browser and races ahead of ACTIVATE when opening a new tab, so the
		   previous tab would load the new page too.
		   forceNavigate=false (SyncAll resync): only create missing slots so
		   reopening the helper does not reload every live tab. */
		if (forceNavigate || !WikiBrowser::HasTab(slot))
			WikiBrowser::CreateTab(slot, start);
		if (activate)
			WikiBrowser::ActivateTab(slot);
	}

	void SyncAllToHelper()
	{
		if (gCount <= 0)
			return;
		for (int i = 0; i < gCount; ++i)
			SyncSlotToHelper(i, false, false);
		WikiBrowser::ActivateTab(gActive);
		SyncSitesFromTab(gTabs[gActive].tab);
	}
}
