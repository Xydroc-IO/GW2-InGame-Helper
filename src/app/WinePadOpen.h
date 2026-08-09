#pragma once

#include "EiRuntime.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

/* Wine crashes when side-rail opens kick CreateThread / SetNextWindowFocus /
   CEF Navigate / heavy HTML builds on the same ImGui click frame as Mirror +
   compass draw. Soft-open: show UI first; run work a few frames later. */
namespace WinePadOpen
{
	inline bool Soft()
	{
		return EiRuntime::IsWine();
	}

	/* Frames to wait after Open before starting network/work. 0 = do it now. */
	inline int DeferFrames()
	{
		return Soft() ? 3 : 0;
	}

	inline void ApplyFocus(bool& focusFlag)
	{
		if (!focusFlag)
			return;
		focusFlag = false;
		if (!Soft())
			ImGui::SetNextWindowFocus();
	}

	/* Call once per Render while countdown > 0; returns true on the fire frame. */
	inline bool TickDefer(int& framesLeft)
	{
		if (framesLeft <= 0)
			return false;
		--framesLeft;
		return framesLeft == 0;
	}

	enum class RailNav : int
	{
		None = 0,
		SiteActive,
		SiteNewTab,
		UrlNewTab,
	};

	struct RailPending
	{
		RailNav kind = RailNav::None;
		int     frames = 0;
		char    siteId[64]{};
		char    url[512]{};
	};

	inline RailPending& RailSlot()
	{
		static RailPending s;
		return s;
	}

	/* Wine only — native callers should invoke BrowserTabs immediately. */
	inline void QueueRailSiteActive(const char* siteId)
	{
		RailPending& p = RailSlot();
		p.kind = RailNav::SiteActive;
		p.frames = DeferFrames();
		std::snprintf(p.siteId, sizeof(p.siteId), "%s", siteId ? siteId : "");
		p.url[0] = 0;
	}

	inline void QueueRailSiteNewTab(const char* siteId)
	{
		RailPending& p = RailSlot();
		p.kind = RailNav::SiteNewTab;
		p.frames = DeferFrames();
		std::snprintf(p.siteId, sizeof(p.siteId), "%s", siteId ? siteId : "");
		p.url[0] = 0;
	}

	inline void QueueRailUrlNewTab(const char* siteId, const char* url)
	{
		RailPending& p = RailSlot();
		p.kind = RailNav::UrlNewTab;
		p.frames = DeferFrames();
		std::snprintf(p.siteId, sizeof(p.siteId), "%s", siteId ? siteId : "browse");
		std::snprintf(p.url, sizeof(p.url), "%s", url ? url : "");
	}

	/* Drain deferred rail nav (BrowserTabs / Navigate). Call from UI render. */
	void TickRailPending();

	/* Wine: open Watch a few frames after the rail/keybind click — same tip-over
	   as CEF when API/live panels already loaded heavy state. */
	inline int& WatchOpenFrames()
	{
		static int s = 0;
		return s;
	}

	inline void QueueWatchOpen()
	{
		if (!Soft())
			return;
		WatchOpenFrames() = DeferFrames();
	}

	inline void CancelWatchOpen()
	{
		WatchOpenFrames() = 0;
	}

	void TickWatchPending();
}
