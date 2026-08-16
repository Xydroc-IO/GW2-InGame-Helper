#pragma once

#include "EiRuntime.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

/* Wine crashes when side-rail opens kick CreateThread / SetNextWindowFocus /
   CEF Navigate / heavy HTML builds / first ImGui Begin on the same click frame
   as Mirror + compass + PresentFrame. Soft-open: queue work; run a few frames later. */
namespace WinePadOpen
{
	inline bool Soft()
	{
		return EiRuntime::IsWine();
	}

	/* Frames to wait after Open before starting network/work. 0 = do it now.
	   Wine: 5 — first Begin must clear PresentFrame/Map pressure after idle. */
	inline int DeferFrames()
	{
		return Soft() ? 5 : 0;
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
		UrlActive,
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
		const char* sid = siteId ? siteId : "";
		if (p.kind == RailNav::SiteActive && std::strcmp(p.siteId, sid) == 0)
			return;
		p.kind = RailNav::SiteActive;
		p.frames = DeferFrames();
		std::snprintf(p.siteId, sizeof(p.siteId), "%s", sid);
		p.url[0] = 0;
	}

	inline void QueueRailSiteNewTab(const char* siteId)
	{
		RailPending& p = RailSlot();
		const char* sid = siteId ? siteId : "";
		if (p.kind == RailNav::SiteNewTab && std::strcmp(p.siteId, sid) == 0)
			return;
		p.kind = RailNav::SiteNewTab;
		p.frames = DeferFrames();
		std::snprintf(p.siteId, sizeof(p.siteId), "%s", sid);
		p.url[0] = 0;
	}

	inline void QueueRailUrlNewTab(const char* siteId, const char* url)
	{
		RailPending& p = RailSlot();
		const char* sid = siteId ? siteId : "browse";
		const char* u = url ? url : "";
		/* Held rail click used to re-queue every frame and fire API Check
		   ~6× in 70ms (crash-0 17:23 TickRail fire kind=3 storm). */
		if (p.kind == RailNav::UrlNewTab && std::strcmp(p.url, u) == 0
			&& std::strcmp(p.siteId, sid) == 0)
			return;
		p.kind = RailNav::UrlNewTab;
		p.frames = DeferFrames();
		std::snprintf(p.siteId, sizeof(p.siteId), "%s", sid);
		std::snprintf(p.url, sizeof(p.url), "%s", u);
	}

	inline void QueueRailUrlActive(const char* siteId, const char* url)
	{
		RailPending& p = RailSlot();
		const char* sid = siteId ? siteId : "browse";
		const char* u = url ? url : "";
		if (p.kind == RailNav::UrlActive && std::strcmp(p.url, u) == 0
			&& std::strcmp(p.siteId, sid) == 0)
			return;
		p.kind = RailNav::UrlActive;
		p.frames = DeferFrames();
		std::snprintf(p.siteId, sizeof(p.siteId), "%s", sid);
		std::snprintf(p.url, sizeof(p.url), "%s", u);
	}

	/* Drain deferred rail nav (BrowserTabs / Navigate). Call from UI render. */
	void TickRailPending();

	/* Wine: defer companion pad Open / OpenAndRefresh off the ImGui click frame
	   so first Begin + PaintPadChrome is not same-frame as PresentFrame. */
	using CompanionOpenFn = void (*)();

	struct CompanionPending
	{
		CompanionOpenFn fn = nullptr;
		int             frames = 0;
		char            name[48]{};
	};

	inline CompanionPending& CompanionSlot()
	{
		static CompanionPending s;
		return s;
	}

	inline bool& CompanionFiredThisFrame()
	{
		static bool s = false;
		return s;
	}

	inline void QueueCompanionOpen(CompanionOpenFn fn, const char* name = nullptr)
	{
		if (!Soft() || !fn)
			return;
		CompanionPending& p = CompanionSlot();
		p.fn = fn;
		p.frames = DeferFrames();
		std::snprintf(p.name, sizeof(p.name), "%s",
			(name && name[0]) ? name : "pad");
	}

	inline void CancelCompanionOpen()
	{
		CompanionPending& p = CompanionSlot();
		p.fn = nullptr;
		p.frames = 0;
		p.name[0] = 0;
	}

	inline int CompanionOpenFrames()
	{
		return CompanionSlot().frames;
	}

	inline bool& RailFiredThisFrame()
	{
		static bool s = false;
		return s;
	}

	/* Frames after soft-open fire — keep SoftWorkBusy through first pad Begin. */
	inline int& CompanionSettleFrames()
	{
		static int s = 0;
		return s;
	}

	/* Which pad softfire is settling (name survives after queue slot clears). */
	inline char (&CompanionSettleName())[48]
	{
		static char s[48]{};
		return s;
	}

	/* True while soft-open is settling / firing (not while only waiting on Mirror). */
	inline bool CompanionSoftBusy()
	{
		if (!Soft())
			return false;
		if (CompanionFiredThisFrame() || CompanionSettleFrames() > 0)
			return true;
		/* Queued soft-open after Mirror idle — SoftWorkBusy in .cpp adds Mirror check. */
		return false;
	}

	/* Wine: skip Watch control Begin while another companion pad settles
	   (crash-0 pinned watch:post Begin during Events settle). Not when Watch itself settles. */
	inline bool SkipWatchControlForSettle()
	{
		if (!Soft())
			return false;
		if (!CompanionFiredThisFrame() && CompanionSettleFrames() <= 0)
			return false;
		const char* n = CompanionSettleName();
		if (!n || !n[0])
			return true;
		return std::strcmp(n, "Watch") != 0;
	}

	/* Watch soft-open (dedicated timer) — set from WatchPad; SoftWorkBusy reads these. */
	inline int& WatchSoftOpenFrames()
	{
		static int s = 0;
		return s;
	}

	inline bool& WatchSoftOpenFired()
	{
		static bool s = false;
		return s;
	}

	/* Wine: rail / soft-open settle — skip CEF Present / WorldGps.
	   Queued while Mirror hot is NOT busy (keep Mirror streaming). */
	bool SoftWorkBusy();

	/* Watch soft-open: pause Mirror upload/AddImage only (Mirror still Begins). */
	inline int& WatchMirrorQuietFrames()
	{
		static int s = 0;
		return s;
	}

	/* Always false — soft-open no longer holds for Mirror. */
	bool CompanionWaitingOnMirror();
	const char* PendingCompanionName();

	/* Soft: queue fn. Native: call immediately. name for crash-trail. */
	void SoftOpen(CompanionOpenFn fn, const char* name = nullptr);

	/* Call at end of UI_Render so soft-open Begin is next frame after Mirror. */
	void TickCompanionPending();
}
