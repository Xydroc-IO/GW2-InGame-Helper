#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

namespace BrowserTabs
{
	constexpr int kMaxTabs = 8;

	struct Tab
	{
		char        siteId[64];
		char        title[48];
		std::string url;
	};

	void EnsureDefault(); /* at least one tab (uses DefaultSiteId) */
	void Tick();         /* sync active tab URL from browser */
	void NavigateActive(); /* load active tab into the browser */
	void PrepareSave();  /* stash current URL before settings write */

	/* Settings persistence */
	void ParseKey(const char* key, const char* val);
	void FinalizeLoad(); /* prune after settings load; ensure ≥1 tab */
	void WriteSettings(FILE* f);

	int  Count();
	int  ActiveIndex();
	const Tab& At(int index);

	/* Replace the active tab with a site (Browse left-click). */
	void OpenInActive(const char* siteId, bool navigate);

	/* Open site in a new tab (Browse Ctrl+click / +). Returns new index, or -1. */
	int  OpenNew(const char* siteId, bool navigate);

	void Activate(int index);
	void Close(int index);

	bool CanGoBack();
	bool CanGoForward();
	void GoBack();
	void GoForward();
	void GoHome();   /* how-to page in active tab */
	void Reload();
}
