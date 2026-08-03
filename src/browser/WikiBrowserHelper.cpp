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
	/* Remove stale helper left in addons/ root from older builds. */
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
		static constexpr const char* kHelperStamp = "2101";
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

	void RequestStartHelper();

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

	bool StartHelper()
	{
		if (gShuttingDown.load())
			return false;
		if (gLaunchDisabled.load())
		{
			SetLocalStatus("Browser helper disabled after launch failure — restart GW2 to retry");
			return false;
		}
		if (HelperAlive())
			return true;
		if (gStarting.exchange(true))
			return false; /* already starting */

		const DWORD now = GetTickCount();
		if (now - gLastStartAttemptMs < 1500)
		{
			gStarting.store(false);
			return false; /* cooldown — don't spam CreateProcess every frame */
		}
		gLastStartAttemptMs = now;

		if (!EnsureIpc())
		{
			gStarting.store(false);
			return false;
		}
		/* Clear any leftover commands from a previous helper before spawn. */
		ResetIpcQueues();
		gIpc->ready = 0;
		gIpc->alive = 0;
		std::snprintf(gIpc->status, sizeof(gIpc->status), "Launching…");

		const std::wstring addon = AddonDir();
		if (addon.empty())
		{
			SetLocalStatus("Addon data directory missing");
			gLaunchDisabled.store(true);
			gStarting.store(false);
			return false;
		}
		if (!CefRuntime::EnsureInstalled(addon.c_str(), [](const char* msg) {
				if (msg)
					SetLocalStatus(msg);
			}))
		{
			gLaunchDisabled.store(true);
			gStarting.store(false);
			return false;
		}

		const std::wstring cef = CefDir();
		if (cef.empty() || GetFileAttributesW((cef + L"\\libcef.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
		{
			SetLocalStatus("Private cef/libcef.dll not found");
			gLaunchDisabled.store(true);
			gStarting.store(false);
			return false;
		}
		if (!ExtractHelper())
		{
			gStarting.store(false);
			return false;
		}

		SetEnvironmentVariableW(L"GW2_HELPER_CEF_DIR", cef.c_str());

		/* Helper stays under the addon directory only — never write into bin64/cef. */
		const std::wstring helperAddon = HelperPath();

		STARTUPINFOW si{};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};
		DWORD lastErr = 0;

		SetLocalStatus("Launching browser…");

		auto tryLaunch = [&](const std::wstring& helper, const std::wstring& cwd) -> bool {
			if (GetFileAttributesW(helper.c_str()) == INVALID_FILE_ATTRIBUTES)
				return false;
			if (!cwd.empty() && GetFileAttributesW(cwd.c_str()) == INVALID_FILE_ATTRIBUTES)
				return false;

			/* host-pid scopes IPC maps so two GW2 clients cannot collide.
			   Avoid extra quoted --start-url= (broke CEF under Wine/Proton). */
			EnsureIpcNames();
			wchar_t pidArg[48];
			std::swprintf(pidArg, 48, L" --host-pid=%lu",
				static_cast<unsigned long>(gHostPid));
			std::wstring cmdLine = L"\"" + helper + L"\" --cef-dir=\"" + cef + L"\"";
			cmdLine += pidArg;
			ZeroMemory(&pi, sizeof(pi));
			/* No CREATE_SUSPENDED / no Sleep — keep the game frame responsive. */
			BOOL ok = CreateProcessW(
				helper.c_str(), cmdLine.data(),
				nullptr, nullptr, FALSE, 0,
				nullptr, cwd.empty() ? nullptr : cwd.c_str(),
				&si, &pi);
			if (!ok)
			{
				lastErr = GetLastError();
				cmdLine = L"\"" + helper + L"\" --cef-dir=\"" + cef + L"\"";
				cmdLine += pidArg;
				ZeroMemory(&pi, sizeof(pi));
				ok = CreateProcessW(
					nullptr, cmdLine.data(),
					nullptr, nullptr, FALSE, 0,
					nullptr, cwd.empty() ? nullptr : cwd.c_str(),
					&si, &pi);
				if (!ok)
				{
					lastErr = GetLastError();
					return false;
				}
			}

			if (HANDLE job = EnsureJob())
				AssignProcessToJobObject(job, pi.hProcess);

			CloseHandle(pi.hThread);
			if (gProcess)
				CloseHandle(gProcess);
			gProcess = pi.hProcess;
			gProcessId = pi.dwProcessId;
			return true;
		};

		/* Prefer addon cwd; fall back to CEF cwd for DLL search without copying files. */
		bool started = tryLaunch(helperAddon, AddonDir());
		if (!started)
			started = tryLaunch(helperAddon, cef);

		if (!started)
		{
			std::string helperUtf8;
			{
				int n = WideCharToMultiByte(CP_UTF8, 0, helperAddon.c_str(), -1, nullptr, 0, nullptr, nullptr);
				helperUtf8.assign(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
				if (n > 0)
					WideCharToMultiByte(CP_UTF8, 0, helperAddon.c_str(), -1, helperUtf8.data(), n, nullptr, nullptr);
			}
			char buf[512];
			std::snprintf(buf, sizeof(buf), "CreateProcess failed (%lu) path=%s", lastErr, helperUtf8.c_str());
			SetLocalStatus(buf);
			gLaunchDisabled.store(true);
			gStarting.store(false);
			return false;
		}

		gStarting.store(false);
		NoteHelperSpawned();
		/* Helper readiness is polled via IPC next frames — never Sleep on this thread. */
		return true;
	}
	void RequestStartHelper()
	{
		if (gShuttingDown.load())
			return;
		if (gLaunchDisabled.load())
		{
			SetLocalStatus("Browser helper disabled after launch failure — restart GW2 to retry");
			return;
		}
		if (gQuitPending.load())
		{
			/* Wait for TickQuitPending — do not CreateProcess over a dying helper. */
			gRelaunchAfterQuit.store(true);
			return;
		}
		if (HelperAlive() || gStarting.load() || gLaunchWorkerBusy.load())
			return;
		/* Dead handle still held — clear it and count a quick death before relaunch. */
		if (gProcess)
		{
			DWORD exitCode = 0;
			GetExitCodeProcess(gProcess, &exitCode);
			NoteHelperDied();
			CloseHandle(gProcess);
			gProcess = nullptr;
			gProcessId = 0;
			if (gLaunchDisabled.load())
				return;
			/* Back off after a crash — do not respawn every frame. */
			if (GetTickCount() - gLastStartAttemptMs < 5000u)
			{
				char buf[160];
				std::snprintf(buf, sizeof(buf),
					"Browser helper exited (code=%lu) — waiting to relaunch",
					static_cast<unsigned long>(exitCode));
				SetLocalStatus(buf);
				return;
			}
			{
				char buf[160];
				std::snprintf(buf, sizeof(buf),
					"Browser helper exited (code=%lu) — relaunching",
					static_cast<unsigned long>(exitCode));
				SetLocalStatus(buf);
			}
		}
		if (gLaunchRequested.exchange(true))
			return;
		/* StartHelper logs "Launching browser…" — avoid a second Launching line. */
	}

	DWORD WINAPI LaunchHelperWorker(LPVOID)
	{
		StartHelper();
		gLaunchWorkerBusy.store(false);
		return 0;
	}

	/* Queue CreateProcess/extract on a worker thread — RT_Render only polls. */
	void TickLaunchPending()
	{
		if (gShuttingDown.load())
			return;
		if (gQuitPending.load())
			return;
		if (!gLaunchRequested.load())
		{
			if (HelperAlive() && gWantVisible.load() && gPendingCmdCount > 0)
			{
				FlushPendingCmds();
				FlushPendingNavigate();
			}
			return;
		}
		if (gLaunchDisabled.load() || HelperAlive())
		{
			gLaunchRequested.store(false);
			if (HelperAlive() && gWantVisible.load())
			{
				FlushPendingCmds();
				FlushPendingNavigate();
			}
			return;
		}
		if (gStarting.load() || gLaunchWorkerBusy.load())
			return;
		gLaunchRequested.store(false);
		gLaunchWorkerBusy.store(true);
		/* Keep the handle — Shutdown must join before the DLL unmaps, or the
		   worker runs on freed code/state and the game hangs on exit. */
		if (gLaunchThread)
		{
			CloseHandle(gLaunchThread);
			gLaunchThread = nullptr;
		}
		gLaunchThread = CreateThread(nullptr, 0, LaunchHelperWorker, nullptr, 0, nullptr);
		if (!gLaunchThread)
		{
			gLaunchWorkerBusy.store(false);
			/* Fallback: sync launch if the OS refuses a worker. */
			StartHelper();
		}
	}
}
