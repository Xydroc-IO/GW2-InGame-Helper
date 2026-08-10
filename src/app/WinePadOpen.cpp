#include "WinePadOpen.h"

#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "CrashTrail.h"
#include "Globals.h"
#include "Settings.h"
#include "WatchCapture.h"
#include "WatchPadInternal.h"
#include "WikiBrowser.h"

#include <algorithm>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	bool MirrorSoftHot()
	{
		return G::ShowWatchMirror
			|| WatchPadDetail::gWantMirrorWhenReady
			|| WatchPadDetail::gDeferMirrorOpenFrames > 0
			|| WatchPadDetail::gSoftStopPhase > 0
			|| WatchCapture::IsCapturing()
			|| WatchCapture::IsStreaming();
	}
}

bool WinePadOpen::CompanionWaitingOnMirror()
{
	/* soft-open no longer holds for Mirror soft-stop. */
	return false;
}

bool WinePadOpen::SoftWorkBusy()
{
	if (!Soft())
		return false;
	/* Settle / fire only — queued soft-open must not quiet Mirror. */
	if (CompanionFiredThisFrame() || CompanionSettleFrames() > 0)
		return true;
	return RailSlot().kind != RailNav::None || RailFiredThisFrame();
}

const char* WinePadOpen::PendingCompanionName()
{
	const CompanionPending& p = CompanionSlot();
	return p.name[0] ? p.name : "pad";
}

void WinePadOpen::SoftOpen(CompanionOpenFn fn, const char* name)
{
	if (!fn)
		return;
	if (!Soft())
	{
		fn();
		return;
	}
	/* Always soft-open — hard mutex (hold until Mirror soft-stop)
	   left panels dead while Mirror ran. Dedicated D3D + settle
	   still reduce Wine pressure. */
	QueueCompanionOpen(fn, name);
	{
		char tag[192];
		std::snprintf(tag, sizeof(tag), "softopen:%s mirror_hot=%d",
			CompanionSlot().name[0] ? CompanionSlot().name : "pad",
			MirrorSoftHot() ? 1 : 0);
		CrashTrail::Note(tag);
	}
}

void WinePadOpen::TickCompanionPending()
{
	if (CrashTrail::DetailArmed())
		CrashTrail::Note("soft:TickCompanion enter");
	CrashTrail::Tick();
	CompanionFiredThisFrame() = false;
	if (CompanionSettleFrames() > 0)
	{
		--CompanionSettleFrames();
		if (CompanionSettleFrames() <= 0)
			CompanionSettleName()[0] = 0;
	}

	CompanionPending& p = CompanionSlot();
	if (!p.fn)
	{
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("soft:TickCompanion idle");
		return;
	}

	/* soft-stop drain only — do not hold on live Mirror. */
	if (WatchPadDetail::CompanionSoftBlocked())
	{
		if (p.frames < 3)
			p.frames = 3;
		return;
	}

	if (p.frames > 0)
	{
		--p.frames;
		return;
	}
	CompanionOpenFn fn = p.fn;
	char pendingName[48];
	std::snprintf(pendingName, sizeof(pendingName), "%s",
		p.name[0] ? p.name : "pad");
	p.fn = nullptr;
	p.name[0] = 0;
	CompanionFiredThisFrame() = true;
	{
		char tag[192];
		std::snprintf(tag, sizeof(tag), "softfire:%s", pendingName);
		CrashTrail::Note(tag);
	}
	std::snprintf(CompanionSettleName(), 48, "%s", pendingName);
	CompanionSettleFrames() = std::max(CompanionSettleFrames(), DeferFrames() * 4);
	WatchMirrorQuietFrames() =
		std::max(WatchMirrorQuietFrames(), CompanionSettleFrames() + 2);
	CrashTrail::ArmDetail(CompanionSettleFrames() + 8);
	CrashTrail::Mark(pendingName);
	fn();
	CrashTrail::NoteF("softfire:done settle=%d quiet=%d name=%s mirror=%d cap=%d",
		CompanionSettleFrames(), WatchMirrorQuietFrames(), CompanionSettleName(),
		G::ShowWatchMirror ? 1 : 0, WatchCapture::IsCapturing() ? 1 : 0);
}

namespace
{
	void ClearApiCheckCacheFiles()
	{
		const std::wstring dir = AddonPaths::DataDir();
		if (dir.empty())
			return;
		auto kill = [&](const wchar_t* ext) {
			std::wstring p = dir;
			if (!p.empty() && p.back() != L'\\' && p.back() != L'/')
				p.push_back(L'\\');
			p += L"gw2-api-check";
			p += ext;
			DeleteFileW(p.c_str());
		};
		kill(L".html");
		kill(L".ver");
		kill(L".ok");
	}
}

void WinePadOpen::TickRailPending()
{
	if (CrashTrail::DetailArmed())
		CrashTrail::Note("soft:TickRail enter");
	RailFiredThisFrame() = false;
	RailPending& p = RailSlot();
	if (p.kind == RailNav::None)
	{
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("soft:TickRail idle");
		return;
	}
	CrashTrail::NoteF("soft:TickRail fire kind=%d", static_cast<int>(p.kind));

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
	RailFiredThisFrame() = true;

	G::ShowWiki = true;
	Settings::SetDirty();

	if (kind == RailNav::UrlNewTab && std::strstr(url, "gw2-api-check"))
		ClearApiCheckCacheFiles();

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
