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
