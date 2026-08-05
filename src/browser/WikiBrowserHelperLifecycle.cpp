#include "WikiBrowser.h"
#include "WikiBrowserShared.h"

#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "CefRuntime.h"
#include "Globals.h"
#include "Sites.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include <windows.h>
#include <shellapi.h>

namespace WikiBrowserDetail
{
	void FinishStopHelper(bool terminateIfAlive)
	{
		const DWORD ownedPid = gProcessId;
		if (gProcess)
		{
			const bool stillAlive = HelperAlive();
			if (terminateIfAlive && stillAlive)
				TerminateProcess(gProcess, 0);
			CloseHandle(gProcess);
			gProcess = nullptr;
			gProcessId = 0;
			if (terminateIfAlive)
				gHelperSpawnMs = 0; /* intentional stop — not a crash */
			else
				NoteHelperDied(); /* discovered already-dead helper */
		}
		else if (terminateIfAlive && ownedPid)
		{
			KillHelperByPid(ownedPid);
			gHelperSpawnMs = 0;
		}
		if (gJob)
		{
			CloseHandle(gJob);
			gJob = nullptr;
		}
		if (gIpc)
		{
			ResetIpcQueues();
			gIpc->ready = 0;
			gIpc->alive = 0;
			std::snprintf(gIpc->status, sizeof(gIpc->status), "Stopped");
		}
		gLastFrameSeq = 0;
		gTexHasContent = false;
		gPartialCopySeq = 0;
		gPartialCopyFront = 0;
		gPartialCopyY = 0;
		gStagingReady = false;
		gStagingSeq = 0;
		gStarting.store(false);
		gLaunchRequested.store(false);
		gRelaunchAfterQuit.store(false);
		gQuitPending.store(false);
		gQuitStartedMs = 0;
		ReleaseGpu();
	}


	void RequestStopHelper()
	{
		if (gQuitPending.load())
			return;
		gRelaunchAfterQuit.store(false);
		if (!HelperAlive() && !gProcess)
		{
			FinishStopHelper(false);
			return;
		}
		PostQuitCmd();
		gQuitPending.store(true);
		gQuitStartedMs = GetTickCount();
		SetLocalStatus("Closing browser…");
	}

	/* Helper asked the DLL to open a URL (Discord OAuth / YouTube) — ShellExecute
	   from the game process works under Proton; from the CEF helper often does not. */
	void DrainOpenExtRequests()
	{
		if (!gIpc)
			return;
		const uint32_t seq = gIpc->open_ext_seq;
		if (seq == gLastOpenExtSeq)
			return;
		gLastOpenExtSeq = seq;
		char url[sizeof(gIpc->open_ext_url)];
		std::snprintf(url, sizeof(url), "%s", gIpc->open_ext_url);
		if (url[0] &&
			(std::strncmp(url, "http://", 7) == 0 ||
				std::strncmp(url, "https://", 8) == 0 ||
				std::strncmp(url, "discord:", 8) == 0 ||
				std::strncmp(url, "Discord:", 8) == 0))
		{
			ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
			if (std::strncmp(url, "discord:", 8) == 0 || std::strncmp(url, "Discord:", 8) == 0)
				SetLocalStatus("Opening Discord app…");
			else
				SetLocalStatus("Opened in system browser (Open Ext)");
		}
	}

	/* Live character → gw2efficiency: open inside the addon as a new tab. */
	void DrainOpenTabRequests()
	{
		if (!gIpc)
			return;
		const uint32_t seq = gIpc->open_tab_seq;
		if (seq == gLastOpenTabSeq)
			return;
		gLastOpenTabSeq = seq;
		char url[sizeof(gIpc->open_tab_url)];
		std::snprintf(url, sizeof(url), "%s", gIpc->open_tab_url);
		if (!url[0] ||
			(std::strncmp(url, "http://", 7) != 0 && std::strncmp(url, "https://", 8) != 0))
			return;

		const char* siteId = "gw2efficiency";
		if (std::strstr(url, "gw2efficiency.com") == nullptr)
			siteId = Sites::ActiveId();
		if (BrowserTabs::OpenNewUrl(siteId, url) < 0)
		{
			/* Tab bar full — fall back to current tab rather than dropping the click. */
			WikiBrowser::Navigate(url);
			SetLocalStatus("Tab limit reached — opened in this tab");
		}
		else
			SetLocalStatus("Opened in a new tab");
	}

	/* Browse hub → catalog site id in a new addon tab. */
	void DrainOpenSiteRequests()
	{
		if (!gIpc)
			return;
		const uint32_t seq = gIpc->open_site_seq;
		if (seq == gLastOpenSiteSeq)
			return;
		gLastOpenSiteSeq = seq;
		char id[sizeof(gIpc->open_site_id)];
		std::snprintf(id, sizeof(id), "%s", gIpc->open_site_id);
		if (!id[0] || Sites::IndexOfId(id) < 0)
			return;
		if (BrowserTabs::OpenNew(id, true) < 0)
		{
			BrowserTabs::OpenInActive(id, true);
			SetLocalStatus("Tab limit reached — opened in this tab");
		}
		else
			SetLocalStatus("Opened in a new tab");
	}

	/* Complete a pending QUIT across frames — never Sleep/Terminate mid-SetVisible. */
	void TickQuitPending()
	{
		if (!gQuitPending.load())
			return;
		if (!HelperAlive())
		{
			FinishStopHelper(false);
			if (gRelaunchAfterQuit.exchange(false) && gWantVisible.load())
			{
				SetLocalStatus("Restarting browser…");
				RequestStartHelper();
			}
			else
				SetLocalStatus("Closed — press Ctrl+Shift+H to open");
			return;
		}
		/* ~120 ms grace for CEF to flush; then terminate (only from Tick). */
		if (GetTickCount() - gQuitStartedMs >= 120u)
		{
			FinishStopHelper(true);
			if (gRelaunchAfterQuit.exchange(false) && gWantVisible.load())
			{
				SetLocalStatus("Restarting browser…");
				RequestStartHelper();
			}
			else
				SetLocalStatus("Closed — press Ctrl+Shift+H to open");
		}
	}

	void StopHelper()
	{
		/* Prefer QUIT, allow one short wait on unload only — never on RT_Render. */
		PostQuitCmd();
		if (gProcess)
		{
			WaitForSingleObject(gProcess, 50);
			if (HelperAlive())
				TerminateProcess(gProcess, 0);
		}
		FinishStopHelper(true);
	}

	HANDLE EnsureJob()
	{
		if (gJob)
			return gJob;
		gJob = CreateJobObjectW(nullptr, nullptr);
		if (!gJob)
			return nullptr;
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
		info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!SetInformationJobObject(gJob, JobObjectExtendedLimitInformation, &info, sizeof(info)))
		{
			CloseHandle(gJob);
			gJob = nullptr;
		}
		return gJob;
	}
}
