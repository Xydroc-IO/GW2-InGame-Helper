#include "WinePadOpen.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Settings.h"
#include "WatchPad.h"
#include "WikiBrowser.h"

#include <cstring>

void WinePadOpen::TickWatchPending()
{
	int& frames = WatchOpenFrames();
	if (frames <= 0)
		return;
	--frames;
	if (frames == 0)
		WatchPad::Open();
}

void WinePadOpen::TickRailPending()
{
	RailPending& p = RailSlot();
	if (p.kind == RailNav::None)
		return;

	if (p.frames > 0)
	{
		--p.frames;
		return;
	}

	const RailNav kind = p.kind;
	char siteId[64];
	char url[512];
	std::snprintf(siteId, sizeof(siteId), "%s", p.siteId);
	std::snprintf(url, sizeof(url), "%s", p.url);
	p.kind = RailNav::None;
	p.siteId[0] = 0;
	p.url[0] = 0;

	G::ShowWiki = true;
	Settings::SetDirty();

	switch (kind)
	{
	case RailNav::SiteActive:
		if (siteId[0])
			BrowserTabs::OpenInActive(siteId, true);
		break;
	case RailNav::SiteNewTab:
		if (siteId[0])
		{
			if (BrowserTabs::OpenNew(siteId, true) < 0)
				BrowserTabs::OpenInActive(siteId, true);
		}
		break;
	case RailNav::UrlNewTab:
		if (url[0])
		{
			if (BrowserTabs::OpenNewUrl(siteId[0] ? siteId : "browse", url) < 0)
				WikiBrowser::Navigate(url);
		}
		break;
	default:
		break;
	}
}
