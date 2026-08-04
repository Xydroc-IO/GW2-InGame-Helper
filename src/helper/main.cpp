/* GW2HelperBrowser.exe — windowless CEF helper using private CEF 150 under
   addons/GW2-InGame-Helper/cef/libcef.dll. Paints BGRA frames into shared
   memory for the addon. CSS downlevel is gated off (native oklch / color-mix). */

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include "WikiIpc.h"
#include "HelperInternal.h"

#include "include/capi/cef_app_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
{
	LRESULT CALLBACK HelperWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		if (msg == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wp, lp);
	}

	bool CreateOsRBrowser()
	{
		gActiveSlot = 0;
		gCreateQueueHead = 0;
		gCreateQueueCount = 0;
		gCreateInFlightSlot = -1;
		gCreateInFlightDiscard = false;
		if (gIpc)
		{
			/* Do not create an about:blank browser here — it stays in CEF history
			   and makes Back after a site change show a white page. The addon
			   CREATE_TAB supplies the real start URL. */
			gIpc->tab_mask = 0;
			gIpc->active_tab = 0;
			/* ready = accepting cmds (CREATE_TAB). Must NOT wait for OnAfterCreated:
			   the host only SyncAllToHelper after ready — that would deadlock. */
			gIpc->ready = 1;
			gIpc->alive = GetTickCount();
		}
		SetStatus("Starting…");
		return true;
	}

	std::wstring GetArg(const wchar_t* name, int argc, wchar_t** argv)
	{
		const std::wstring key = std::wstring(L"--") + name + L"=";
		for (int i = 1; i < argc; ++i)
		{
			std::wstring a = argv[i];
			if (a.rfind(key, 0) == 0)
				return a.substr(key.size());
		}
		return {};
	}
}

using namespace HelperDetail;

int APIENTRY wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv)
		return 1;

	gCefDir = GetArg(L"cef-dir", argc, argv);
	{
		const std::wstring start = GetArg(L"start-url", argc, argv);
		if (!start.empty())
			gStartUrl = WideToUtf8(start);
	}
	DWORD hostPid = 0;
	{
		const std::wstring pidStr = GetArg(L"host-pid", argc, argv);
		if (!pidStr.empty())
			hostPid = static_cast<DWORD>(_wtoi(pidStr.c_str()));
	}
	LocalFree(argv);

	if (gCefDir.empty())
	{
		wchar_t env[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"GW2_HELPER_CEF_DIR", env, MAX_PATH) > 0)
			gCefDir = env;
	}
	if (gCefDir.empty())
		return 2;

	SetDllDirectoryW(gCefDir.c_str());
	if (!LoadCef(gCefDir))
		return 6;

	InitHandlers();

	cef_main_args_t mainArgs{};
	mainArgs.instance = hi;

	const int exitCode = g_execute_process(&mainArgs, &gApp, nullptr);
	if (exitCode >= 0)
		return exitCode;

	if (hostPid == 0)
		hostPid = GetCurrentProcessId(); /* fallback — still unique vs other clients */

	/* Watchdog: if GW2 dies without a clean QUIT (crash, hard kill), exit too.
	   Orphaned helpers keep the named sections alive and stall the next launch. */
	HANDLE hostProc = nullptr;
	if (hostPid != GetCurrentProcessId())
		hostProc = OpenProcess(SYNCHRONIZE, FALSE, hostPid);

	char ipcName[96]{};
	char frameName[96]{};
	char wakeName[96]{};
	WikiIpcFormatNames(hostPid, ipcName, frameName, wakeName, sizeof(ipcName));

	gMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, ipcName);
	if (!gMap)
		return 4;
	gIpc = static_cast<WikiIpcState*>(MapViewOfFile(gMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(WikiIpcState)));
	if (!gIpc || gIpc->magic != kWikiIpcMagic)
		return 5;

	gFrameMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, frameName);
	if (!gFrameMap)
	{
		SetStatus("Failed to open frame mapping");
		return 9;
	}
	gFramePixels = static_cast<uint8_t*>(MapViewOfFile(gFrameMap, FILE_MAP_ALL_ACCESS, 0, 0, kWikiFrameMapBytes));
	if (!gFramePixels)
	{
		SetStatus("Failed to map frame buffer");
		return 10;
	}
	gIpc->frame_front = 0;
	gIpc->frame_reading = 0xFFFFFFFFu;
	gIpc->dirty_w = kWikiFrameMaxW;
	gIpc->dirty_h = kWikiFrameMaxH;
	gWakeEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, wakeName);

	SetStatus("Initializing CEF…");

	cef_settings_t settings{};
	settings.size = sizeof(settings);
	settings.no_sandbox = 1;
	/* Single-threaded CEF UI loop so send_key/mouse/focus from DrainInput
	   run on the browser UI thread. multi_threaded_message_loop=1 made
	   OSR input silently fail (page painted but typing/clicks did nothing). */
	settings.multi_threaded_message_loop = 0;
	settings.external_message_pump = 0;
	settings.log_severity = LOGSEVERITY_DISABLE;
	settings.command_line_args_disabled = 0;
	settings.windowless_rendering_enabled = 1;

	wchar_t selfPath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, selfPath, MAX_PATH);

	const std::string cefDirUtf8 = WideToUtf8(gCefDir);
	const std::string selfUtf8 = WideToUtf8(selfPath);
	MakeCefString(&settings.browser_subprocess_path, selfUtf8.c_str());
	MakeCefString(&settings.resources_dir_path, cefDirUtf8.c_str());
	const std::string localesUtf8 = cefDirUtf8 + "\\locales";
	MakeCefString(&settings.locales_dir_path, localesUtf8.c_str());
	/* Chromium profile under %LOCALAPPDATA% — never under Guild Wars 2/addons.
	   Previously %TEMP%, but Windows Storage Sense / Disk Cleanup / CCleaner wipe
	   %TEMP%, discarding Google cookies each session so users hit /sorry/index
	   "unusual traffic". %LOCALAPPDATA% persists like a normal browser profile.
	   Falls back to %TEMP% only if the variable is missing (rare). */
	std::wstring cacheRoot;
	{
		wchar_t local[MAX_PATH]{};
		const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
		if (n > 0 && n < MAX_PATH)
		{
			cacheRoot = local;
			cacheRoot += L"\\GW2-InGame-Helper";
			CreateDirectoryW(cacheRoot.c_str(), nullptr);
		}
		else
		{
			wchar_t tmp[MAX_PATH]{};
			GetTempPathW(MAX_PATH, tmp);
			cacheRoot = tmp;
		}
	}
	const std::wstring cache = cacheRoot + L"\\cef-cache";
	CreateDirectoryW(cache.c_str(), nullptr);
	const std::string cacheUtf8 = WideToUtf8(cache);
	MakeCefString(&settings.cache_path, cacheUtf8.c_str());
	MakeCefString(&settings.root_cache_path, cacheUtf8.c_str());
	/* Match private CEF Stable 150 — do not spoof an older Chrome major.
	   Trailing product token lets publishers allow/deny this client (see
	   docs/PUBLISHER_ACCESS.md). Keep the token stable once shipped. */
	MakeCefString(&settings.user_agent,
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
		"Chrome/150.0.7871.129 Safari/537.36 GW2-InGame-Helper");

	if (!g_initialize(&mainArgs, &settings, &gApp, nullptr))
	{
		SetStatus("cef_initialize failed");
		ClearCefString(&settings.browser_subprocess_path);
		ClearCefString(&settings.resources_dir_path);
		ClearCefString(&settings.locales_dir_path);
		ClearCefString(&settings.cache_path);
		ClearCefString(&settings.root_cache_path);
		ClearCefString(&settings.user_agent);
		return 7;
	}
	ClearCefString(&settings.browser_subprocess_path);
	ClearCefString(&settings.resources_dir_path);
	ClearCefString(&settings.locales_dir_path);
	ClearCefString(&settings.cache_path);
	ClearCefString(&settings.root_cache_path);
	ClearCefString(&settings.user_agent);

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = HelperWndProc;
	wc.hInstance = hi;
	wc.lpszClassName = L"GW2InGameHelper_CefHelperWnd";
	RegisterClassExW(&wc);
	gHelperWnd = CreateWindowExW(0, wc.lpszClassName, L"GW2HelperBrowserWnd",
		WS_OVERLAPPED, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hi, nullptr);

	if (!CreateOsRBrowser())
	{
		g_shutdown();
		return 8;
	}

	MSG msg{};
	while (gRunning)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				gRunning = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		/* Must run on this thread with multi_threaded_message_loop=0. */
		g_do_message_loop_work();
		ProcessCommands();

		if (hostProc && WaitForSingleObject(hostProc, 0) == WAIT_OBJECT_0)
		{
			gRunning = false;
			break;
		}

		/* Idle: wake on DLL cmds/input, else longer timeout for CEF timers.
		   Never spin at 1–2ms forever — under Proton that can lock the host. */
		const bool inputPending = gIpc && (gIpc->input_read != gIpc->input_write);
		const bool painted = gIpc && gIpc->frame_seq != 0;
		const bool busy = gIpc && gIpc->visible && painted;
		DWORD timeout = 16u;
		if (inputPending)
			timeout = 4u;
		else if (busy)
			timeout = 8u;
		else if (gIpc && gIpc->visible && !painted)
			timeout = 33u; /* create/paint stalled — back off hard */
		HANDLE waits[2]{};
		DWORD waitCount = 0;
		if (gWakeEvent)
			waits[waitCount++] = gWakeEvent;
		if (hostProc)
			waits[waitCount++] = hostProc;
		if (waitCount)
		{
			MsgWaitForMultipleObjects(waitCount, waits, FALSE, timeout, QS_ALLINPUT);
			if (gWakeEvent)
				ResetEvent(gWakeEvent);
		}
		else
			Sleep(timeout);
	}

	for (int i = 0; i < kWikiMaxTabs; ++i)
	{
		if (!gBrowsers[i])
			continue;
		gBrowsers[i]->base.release(&gBrowsers[i]->base);
		gBrowsers[i] = nullptr;
	}
	g_shutdown();
	if (gIpc)
	{
		gIpc->ready = 0;
		gIpc->alive = 0;
		UnmapViewOfFile(gIpc);
	}
	if (gFramePixels)
		UnmapViewOfFile(gFramePixels);
	if (gMap)
		CloseHandle(gMap);
	if (gFrameMap)
		CloseHandle(gFrameMap);
	if (gWakeEvent)
		CloseHandle(gWakeEvent);
	if (hostProc)
		CloseHandle(hostProc);
	return 0;
}
