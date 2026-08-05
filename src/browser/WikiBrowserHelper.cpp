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

extern "C" {
	extern const unsigned char _binary_build_helper_blob_exe_start[];
	extern const unsigned char _binary_build_helper_blob_exe_end[];
}

namespace WikiBrowserDetail
{
	void CleanupStaleAddonRootFiles()
	{
		const std::wstring data = AddonDir();
		wchar_t path[MAX_PATH]{};
		if (!G::Self || !GetModuleFileNameW(G::Self, path, MAX_PATH))
			return;
		std::wstring full = path;
		const size_t slash = full.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return;
		const std::wstring root = full.substr(0, slash);
		if (root.empty() || _wcsicmp(root.c_str(), data.c_str()) == 0)
			return;

		const wchar_t* stale[] = {
			L"\\GW2HelperBrowser.exe",
			L"\\helper-home.html",
			L"\\helper-home.ver",
			L"\\home-logo.png",
			L"\\home-cover.jpg",
			L"\\raid-food.html",
			L"\\raid-food.ver",
			L"\\raid-utilities.html",
			L"\\raid-utilities.ver",
			L"\\fractal-consumables.html",
			L"\\fractal-consumables.ver",
			L"\\sigils-runes.html",
			L"\\sigils-runes.ver",
			L"\\relics-guide.html",
			L"\\relics-guide.ver",
			L"\\boon-checklist.html",
			L"\\boon-checklist.ver",
			L"\\cc-defiance.html",
			L"\\cc-defiance.ver",
			L"\\raid-wings.html",
			L"\\raid-wings.ver",
			L"\\home-garden.html",
			L"\\home-garden.ver",
			L"\\ubers-all-in-one.html",
			L"\\ubers-all-in-one.ver",
			L"\\strike-missions.html",
			L"\\strike-missions.ver",
			L"\\fractal-cm-list.html",
			L"\\fractal-cm-list.ver",
			L"\\squad-template.html",
			L"\\squad-template.ver",
			L"\\stability-cleanse.html",
			L"\\stability-cleanse.ver",
			L"\\material-conversions.html",
			L"\\material-conversions.ver",
			L"\\legendary-paths.html",
			L"\\legendary-paths.ver",
			L"\\live-legendary-vault.html",
			L"\\live-legendary-vault.ver",
			L"\\live-legendary-vault.ok",
			L"\\craft-plan-cmd.txt",
			L"\\open-about-cmd.txt",
			L"\\live-cheatsheets-hub.html",
			L"\\live-cheatsheets-hub.ver",
			L"\\live-cheatsheets-hub.ok",
			L"\\legendary-detail-cmd.txt",
			L"\\mount-unlock.html",
			L"\\mount-unlock.ver",
			L"\\daily-weekly.html",
			L"\\daily-weekly.ver",
			L"\\live-dailies.html",
			L"\\live-dailies.ver",
			L"\\live-dailies.ok",
			L"\\live-news.html",
			L"\\live-news.ver",
			L"\\live-news.ok",
			L"\\live-fashion.html",
			L"\\live-fashion.ver",
			L"\\live-fashion.ok",
			L"\\live-tp.html",
			L"\\live-tp.ver",
			L"\\live-tp.ok",
			L"\\live-tp-cmd.txt",
			L"\\live-progress.html",
			L"\\live-progress.ver",
			L"\\live-progress.ok",
			L"\\live-colors.json",
			L"\\currency-sinks.html",
			L"\\currency-sinks.ver",
			L"\\ascended-start.html",
			L"\\ascended-start.ver",
			L"\\portals-pulls.html",
			L"\\portals-pulls.ver",
			L"\\homestead-extras.html",
			L"\\homestead-extras.ver",
			L"\\wvw-consumables.html",
			L"\\wvw-consumables.ver",
			L"\\dps-log-setup.html",
			L"\\dps-log-setup.ver",
			L"\\api-key-setup.html",
			L"\\api-key-setup.ver",
			L"\\settings.ini",
		};
		for (const wchar_t* name : stale)
		{
			const std::wstring p = root + name;
			DeleteFileW(p.c_str());
		}
	}
	void KillHelperByPid(DWORD pid)
	{
		if (!pid)
			return;
		HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
		if (!proc)
			return;
		TerminateProcess(proc, 0);
		CloseHandle(proc);
	}

	void ResetIpcQueues()
	{
		if (!gIpc)
			return;
		/* Drop any unconsumed commands/input so a restarted helper cannot
		   replay CLOSE_TAB / CREATE_TAB against fresh CEF state. */
		gIpc->cmd_read = 0;
		gIpc->cmd_write = 0;
		gIpc->cmd = WIKI_CMD_NONE;
		gIpc->cmd_a = 0;
		gIpc->cmd_arg[0] = 0;
		gIpc->last_cmd_seq = gIpc->cmd_seq;
		gIpc->input_read = 0;
		gIpc->input_write = 0;
		gIpc->tab_mask = 0;
		gIpc->active_tab = 0;
		gIpc->frame_seq = 0;
		gIpc->frame_front = 0;
		gIpc->frame_reading = 0xFFFFFFFFu;
		gIpc->url[0] = 0;
		gIpc->url_len = 0;
		gIpc->url_seq = 0;
		gIpc->title[0] = 0;
		gIpc->title_len = 0;
		gIpc->title_seq = 0;
		gIpc->open_ext_seq = 0;
		gIpc->open_ext_url[0] = 0;
		gLastOpenExtSeq = 0;
		gIpc->open_tab_seq = 0;
		gIpc->open_tab_url[0] = 0;
		gLastOpenTabSeq = 0;
		gUrlCache[0] = 0;
		gUrlCacheSeq = 0xFFFFFFFFu;
		gTitleCache[0] = 0;
		gTitleCacheSeq = 0xFFFFFFFFu;
		gPendingCmdCount = 0;
	}

	bool ExtractHelper()
	{
		const unsigned char* begin = _binary_build_helper_blob_exe_start;
		const unsigned char* end = _binary_build_helper_blob_exe_end;
		if (end <= begin)
			return false;
		const size_t size = static_cast<size_t>(end - begin);
		const std::wstring path = HelperPath();
		/* Bump when helper behavior changes — size-only reuse can keep a stale exe
		   if the blob happens to match byte length (or Wine holds the old file). */
		static constexpr const char* kHelperStamp = "2209";
		const std::wstring verPath = path + L".ver";

		bool stampOk = false;
		HANDLE verIn = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (verIn != INVALID_HANDLE_VALUE)
		{
			char buf[32]{};
			DWORD got = 0;
			if (ReadFile(verIn, buf, sizeof(buf) - 1, &got, nullptr) && got > 0)
				stampOk = (std::strncmp(buf, kHelperStamp, std::strlen(kHelperStamp)) == 0);
			CloseHandle(verIn);
		}

		/* Fast path: reuse existing extract if size + stamp match. */
		HANDLE existing = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (existing != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER li{};
			const bool same = stampOk && GetFileSizeEx(existing, &li) &&
				static_cast<size_t>(li.QuadPart) == size;
			CloseHandle(existing);
			if (same)
				return true;
		}

		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
		{
			char buf[256];
			std::snprintf(buf, sizeof(buf), "Extract CreateFile failed (%lu)", GetLastError());
			SetLocalStatus(buf);
			return false;
		}
		DWORD written = 0;
		const BOOL ok = WriteFile(out, begin, static_cast<DWORD>(size), &written, nullptr);
		CloseHandle(out); /* no FlushFileBuffers — that stalled the game on Wine */
		if (!ok || written != size)
		{
			SetLocalStatus("Extract WriteFile incomplete");
			return false;
		}

		HANDLE verOut = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (verOut != INVALID_HANDLE_VALUE)
		{
			DWORD vw = 0;
			WriteFile(verOut, kHelperStamp, static_cast<DWORD>(std::strlen(kHelperStamp)), &vw, nullptr);
			CloseHandle(verOut);
		}
		return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	void EnsureIpcNames()
	{
		const DWORD pid = GetCurrentProcessId();
		if (gHostPid == pid && gIpcName[0])
			return;
		gHostPid = pid;
		WikiIpcFormatNames(pid, gIpcName, gFrameName, gWakeName, sizeof(gIpcName));
	}

	bool EnsureIpc()
	{
		if (gIpc && gFramePixels)
			return true;

		EnsureIpcNames();

		if (!gMap)
		{
			gMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
				0, sizeof(WikiIpcState), gIpcName);
			if (!gMap)
			{
				SetLocalStatus("Failed to create IPC mapping");
				return false;
			}
		}
		if (!gIpc)
		{
			gIpc = static_cast<WikiIpcState*>(MapViewOfFile(gMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(WikiIpcState)));
			if (!gIpc)
			{
				SetLocalStatus("Failed to map IPC");
				return false;
			}
			std::memset(gIpc, 0, sizeof(*gIpc));
			gIpc->magic = kWikiIpcMagic;
			gIpc->frame_reading = 0xFFFFFFFFu;
			gIpc->dirty_w = kWikiFrameMaxW;
			gIpc->dirty_h = kWikiFrameMaxH;
			std::snprintf(gIpc->status, sizeof(gIpc->status), "Idle");
		}

		if (!gWakeEvent)
		{
			gWakeEvent = CreateEventA(nullptr, FALSE, FALSE, gWakeName);
			if (!gWakeEvent)
			{
				/* Non-fatal — helper falls back to Sleep. */
			}
		}

		if (!gFrameMap)
		{
			gFrameMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
				0, kWikiFrameMapBytes, gFrameName);
			if (!gFrameMap)
			{
				SetLocalStatus("Failed to create frame mapping");
				return false;
			}
		}
		if (!gFramePixels)
		{
			gFramePixels = static_cast<uint8_t*>(MapViewOfFile(
				gFrameMap, FILE_MAP_ALL_ACCESS, 0, 0, kWikiFrameMapBytes));
			if (!gFramePixels)
			{
				SetLocalStatus("Failed to map frame buffer");
				return false;
			}
			/* Don't memset ~18MB here — that hitch froze the game on open. */
		}
		return true;
	}

	bool HelperAlive()
	{
		if (!gProcess)
			return false;
		return WaitForSingleObject(gProcess, 0) == WAIT_TIMEOUT;
	}

	/* Helper died soon after spawn → stop relaunching (prevents system lockup). */
	void NoteHelperDied()
	{
		if (gHelperSpawnMs == 0)
			return;
		const DWORD lived = GetTickCount() - gHelperSpawnMs;
		gHelperSpawnMs = 0;
		if (lived < 12000u)
		{
			++gQuickDeathCount;
			if (gQuickDeathCount >= 2)
			{
				gLaunchDisabled.store(true);
				gLaunchRequested.store(false);
				gRelaunchAfterQuit.store(false);
				SetLocalStatus("Browser helper crashed repeatedly — closed to protect system. Restart GW2 to retry");
			}
		}
		else
			gQuickDeathCount = 0;
	}

	void NoteHelperSpawned()
	{
		gHelperSpawnMs = GetTickCount();
	}

	void PostQuitCmd()
	{
		if (!gIpc || !HelperAlive())
			return;
		/* Same ring-first rule as PostCmd — do not bump cmd_seq when the
		   ring accepts QUIT (avoids double HandleCmd on shutdown). */
		const uint32_t w = gIpc->cmd_write;
		const uint32_t next = (w + 1u) % kWikiCmdQueueSize;
		if (next != gIpc->cmd_read)
		{
			WikiCmdEvent& ev = gIpc->cmd_q[w % kWikiCmdQueueSize];
			ev.cmd = WIKI_CMD_QUIT;
			ev.a = 0;
			ev.arg[0] = 0;
			gIpc->cmd_write = next;
			gIpc->cmd = WIKI_CMD_QUIT;
			gIpc->cmd_a = 0;
			gIpc->cmd_arg[0] = 0;
		}
		else
		{
			gIpc->cmd = WIKI_CMD_QUIT;
			gIpc->cmd_a = 0;
			gIpc->cmd_arg[0] = 0;
			++gIpc->cmd_seq;
		}
		WakeHelper();
	}
}
