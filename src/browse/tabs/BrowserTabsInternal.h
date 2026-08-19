#pragma once

/* Shared state / helpers for BrowserTabs.cpp + BrowserTabsState.cpp + BrowserTabsNav.cpp. */

#include "BrowserTabs.h"

#include <string>

namespace BrowserTabsDetail
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

	extern TabState gTabs[BrowserTabs::kMaxTabs];
	extern int gCount;
	extern int gActive;

	extern ClosedTab gClosed[BrowserTabs::kClosedStack];
	extern int gClosedCount;

	void SyncSitesFromTab(const BrowserTabs::Tab& tab);
	void FillFromSite(TabState& t, const char* siteId);
	void StashActiveUrl();
	void PushClosed(const BrowserTabs::Tab& tab);
	void ApplyTabTitle(BrowserTabs::Tab& tab, const char* title);
	void RefreshTabLabelFromUrl(BrowserTabs::Tab& tab, bool preferPageTitle);
	void SyncSlotToHelper(int slot, bool activate, bool forceNavigate = true);
	void SyncAllToHelper();
	void BumpLegendaryLedgerIfNewDest(const std::string& prev, const std::string& dest);
}
