#include "WikiBrowser.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "CheatSheets.h"
#include "HomePage.h"
#include "RaidFood.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiIpc.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <d3d11.h>
#include <dxgi.h>
#include <tlhelp32.h>
#include <windows.h>

extern "C" {
	extern const unsigned char _binary_build_helper_blob_exe_start[];
	extern const unsigned char _binary_build_helper_blob_exe_end[];
}

namespace
{
	HANDLE gMap = nullptr;
	HANDLE gFrameMap = nullptr;
	WikiIpcState* gIpc = nullptr;
	uint8_t* gFramePixels = nullptr;
	HANDLE gProcess = nullptr;
	HANDLE gJob = nullptr;
	DWORD gProcessId = 0;
	std::mutex gMutex;
	std::string gStatus = "Closed — press Ctrl+Shift+H to open";
	std::string gPendingNavigate;
	std::atomic<bool> gWantVisible{false};
	std::atomic<bool> gLaunchDisabled{false}; /* set if helper launch fails hard */
	std::atomic<bool> gStarting{false};       /* StartHelper in progress (avoid re-entry) */
	DWORD gLastStartAttemptMs = 0;

	ID3D11Device* gDevice = nullptr;
	ID3D11DeviceContext* gContext = nullptr;
	ID3D11Texture2D* gTex = nullptr;
	ID3D11ShaderResourceView* gSrv = nullptr;
	uint32_t gTexW = 0;
	uint32_t gTexH = 0;
	uint32_t gLastFrameSeq = 0;

	std::wstring Utf8ToWide(const std::string& utf8)
	{
		if (utf8.empty())
			return L"";
		int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		std::wstring out(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
		if (n > 0)
			MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), n);
		return out;
	}

	void SetLocalStatus(const std::string& s)
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gStatus = s;
		if (G::API && G::API->Log)
			G::API->Log(LOGL_INFO, ADDON_NAME, s.c_str());
	}

	std::wstring GameDir()
	{
		if (G::API && G::API->Paths_GetGameDirectory)
		{
			const char* d = G::API->Paths_GetGameDirectory();
			if (d && d[0])
				return Utf8ToWide(d);
		}
		return L"";
	}

	std::wstring AddonDir()
	{
		return AddonPaths::DataDir();
	}

	std::wstring CefDir()
	{
		const std::wstring game = GameDir();
		if (game.empty())
			return L"";
		return game + L"\\bin64\\cef";
	}

	std::wstring HelperPath()
	{
		return AddonDir() + L"\\GW2HelperBrowser.exe";
	}

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
			L"\\settings.ini",
		};
		for (const wchar_t* name : stale)
		{
			const std::wstring p = root + name;
			DeleteFileW(p.c_str());
		}
	}

	void ReleaseGpu()
	{
		if (gSrv) { gSrv->Release(); gSrv = nullptr; }
		if (gTex) { gTex->Release(); gTex = nullptr; }
		gTexW = gTexH = 0;
		gLastFrameSeq = 0;
	}

	void ReleaseDevice()
	{
		ReleaseGpu();
		if (gContext) { gContext->Release(); gContext = nullptr; }
		if (gDevice) { gDevice->Release(); gDevice = nullptr; }
	}

	bool EnsureDevice()
	{
		if (gDevice && gContext)
			return true;
		if (!G::API || !G::API->SwapChain)
			return false;
		auto* swap = static_cast<IDXGISwapChain*>(G::API->SwapChain);
		ID3D11Device* dev = nullptr;
		if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev))) || !dev)
			return false;
		ID3D11DeviceContext* ctx = nullptr;
		dev->GetImmediateContext(&ctx);
		if (!ctx)
		{
			dev->Release();
			return false;
		}
		gDevice = dev;
		gContext = ctx;
		return true;
	}

	bool EnsureTexture(uint32_t w, uint32_t h)
	{
		if (!EnsureDevice() || w == 0 || h == 0)
			return false;
		if (gTex && gSrv && gTexW == w && gTexH == h)
			return true;

		ReleaseGpu();

		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DYNAMIC;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(gDevice->CreateTexture2D(&td, nullptr, &gTex)) || !gTex)
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = td.Format;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = 1;
		if (FAILED(gDevice->CreateShaderResourceView(gTex, &sd, &gSrv)) || !gSrv)
		{
			ReleaseGpu();
			return false;
		}
		gTexW = w;
		gTexH = h;
		return true;
	}

	void KillStrayHelpers()
	{
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snap == INVALID_HANDLE_VALUE)
			return;
		PROCESSENTRY32W pe{};
		pe.dwSize = sizeof(pe);
		if (Process32FirstW(snap, &pe))
		{
			do
			{
				if (_wcsicmp(pe.szExeFile, L"GW2HelperBrowser.exe") == 0)
				{
					HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
					if (proc)
					{
						TerminateProcess(proc, 0);
						CloseHandle(proc);
					}
				}
			} while (Process32NextW(snap, &pe));
		}
		CloseHandle(snap);
	}

	bool ExtractHelper()
	{
		const unsigned char* begin = _binary_build_helper_blob_exe_start;
		const unsigned char* end = _binary_build_helper_blob_exe_end;
		if (end <= begin)
			return false;
		const size_t size = static_cast<size_t>(end - begin);
		const std::wstring path = HelperPath();

		/* Fast path: reuse existing extract if size matches (avoids ~3MB write hitch). */
		HANDLE existing = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (existing != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER li{};
			const bool same = GetFileSizeEx(existing, &li) && static_cast<size_t>(li.QuadPart) == size;
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
		return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool EnsureIpc()
	{
		if (gIpc && gFramePixels)
			return true;

		if (!gMap)
		{
			gMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
				0, sizeof(WikiIpcState), kWikiIpcMapName);
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
			std::snprintf(gIpc->status, sizeof(gIpc->status), "Idle");
		}

		if (!gFrameMap)
		{
			gFrameMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
				0, kWikiFrameBytes, kWikiFrameMapName);
			if (!gFrameMap)
			{
				SetLocalStatus("Failed to create frame mapping");
				return false;
			}
		}
		if (!gFramePixels)
		{
			gFramePixels = static_cast<uint8_t*>(MapViewOfFile(
				gFrameMap, FILE_MAP_ALL_ACCESS, 0, 0, kWikiFrameBytes));
			if (!gFramePixels)
			{
				SetLocalStatus("Failed to map frame buffer");
				return false;
			}
			/* Don't memset ~9MB here — that hitch froze the game on open. */
		}
		return true;
	}

	bool HelperAlive()
	{
		if (!gProcess)
			return false;
		return WaitForSingleObject(gProcess, 0) == WAIT_TIMEOUT;
	}

	void StopHelper()
	{
		if (gIpc && HelperAlive())
		{
			const uint32_t w = gIpc->cmd_write;
			const uint32_t next = (w + 1u) % kWikiCmdQueueSize;
			if (next != gIpc->cmd_read)
			{
				WikiCmdEvent& ev = gIpc->cmd_q[w % kWikiCmdQueueSize];
				ev.cmd = WIKI_CMD_QUIT;
				ev.a = 0;
				ev.arg[0] = 0;
				gIpc->cmd_write = next;
			}
			gIpc->cmd = WIKI_CMD_QUIT;
			gIpc->cmd_a = 0;
			gIpc->cmd_arg[0] = 0;
			++gIpc->cmd_seq;
			/* Never block the game render thread for long — brief poll then kill. */
			WaitForSingleObject(gProcess, 50);
		}
		if (gProcess)
		{
			if (HelperAlive())
				TerminateProcess(gProcess, 0);
			CloseHandle(gProcess);
			gProcess = nullptr;
			gProcessId = 0;
		}
		if (gJob)
		{
			CloseHandle(gJob);
			gJob = nullptr;
		}
		if (gIpc)
		{
			gIpc->ready = 0;
			gIpc->alive = 0;
			gIpc->frame_seq = 0;
			std::snprintf(gIpc->status, sizeof(gIpc->status), "Stopped");
		}
		gLastFrameSeq = 0;
		gStarting.store(false);
		ReleaseGpu();
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

		const std::wstring cef = CefDir();
		if (cef.empty() || GetFileAttributesW((cef + L"\\libcef.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
		{
			SetLocalStatus("GW2 bin64/cef/libcef.dll not found");
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

			/* Keep cmdline identical to the working Snowcrow form — extra quoted
			   --start-url= args broke CEF launch under Wine/Proton. */
			std::wstring cmdLine = L"\"" + helper + L"\" --cef-dir=\"" + cef + L"\"";
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
		/* Helper readiness is polled via IPC next frames — never Sleep on this thread. */
		return true;
	}

	void PostCmd(WikiIpcCmd cmd, const char* arg = "", int32_t a = 0)
	{
		if (!gIpc || !HelperAlive())
			return;

		const uint32_t w = gIpc->cmd_write;
		const uint32_t next = (w + 1u) % kWikiCmdQueueSize;
		if (next != gIpc->cmd_read)
		{
			WikiCmdEvent& ev = gIpc->cmd_q[w % kWikiCmdQueueSize];
			ev.cmd = cmd;
			ev.a = a;
			std::snprintf(ev.arg, sizeof(ev.arg), "%s", arg ? arg : "");
			gIpc->cmd_write = next;
		}

		/* Legacy single-slot — still written for older helpers / debug. */
		std::snprintf(gIpc->cmd_arg, sizeof(gIpc->cmd_arg), "%s", arg ? arg : "");
		gIpc->cmd_a = a;
		gIpc->cmd = cmd;
		++gIpc->cmd_seq;
	}

	/* Map built-in about: URLs to file:/// before CEF sees them (CreateTab
	   used to pass about:raid-food raw → blank white page). */
	std::string ResolveNavigateUrl(const std::string& url)
	{
		if (url.empty())
			return {};
		if (url == "about:helper-home")
		{
			const std::string fileUrl = HomePage::EnsureFileUrl(AddonDir());
			if (fileUrl.empty())
				SetLocalStatus("Failed to write helper-home.html");
			return fileUrl;
		}
		if (url == "about:raid-food")
		{
			const std::string fileUrl = RaidFood::EnsureFileUrl(AddonDir());
			if (fileUrl.empty())
				SetLocalStatus("Failed to write raid-food.html");
			return fileUrl;
		}
		{
			const std::string fileUrl = CheatSheets::ResolveAboutUrl(AddonDir(), url);
			if (!fileUrl.empty())
				return fileUrl;
			if (CheatSheets::FindByAbout(url.c_str()))
				SetLocalStatus("Failed to write cheat sheet HTML");
		}
		return url;
	}

	/* Single IPC command slot — queue navigates until the helper is ready, then
	   flush once so SET_VISIBLE / SET_BOUNDS cannot wipe the URL. */
	void QueueNavigate(const std::string& url)
	{
		if (url.empty())
			return;
		const std::string resolved = ResolveNavigateUrl(url);
		if (resolved.empty())
			return;
		gPendingNavigate = resolved;
	}

	void FlushPendingNavigate()
	{
		if (gPendingNavigate.empty() || !gIpc || !HelperAlive())
			return;
		if (!gIpc->ready)
			return;
		PostCmd(WIKI_CMD_NAVIGATE, gPendingNavigate.c_str());
		if (G::API && G::API->Log)
		{
			char buf[256];
			std::snprintf(buf, sizeof(buf), "Navigate: %.200s", gPendingNavigate.c_str());
			G::API->Log(LOGL_INFO, ADDON_NAME, buf);
		}
		gPendingNavigate.clear();
	}

	void PushInput(const WikiInputEvent& ev)
	{
		if (!gIpc || !HelperAlive())
			return;
		const uint32_t w = gIpc->input_write;
		const uint32_t next = (w + 1u) % kWikiInputQueueSize;
		if (next == gIpc->input_read)
			return; /* queue full — drop (clicks shouldn't pile up that deep) */
		gIpc->input_q[w % kWikiInputQueueSize] = ev;
		gIpc->input_write = next;
	}
}

std::string WikiBrowser::UrlEncode(const std::string& value)
{
	std::string out;
	out.reserve(value.size() * 3);
	for (unsigned char c : value)
	{
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~')
			out.push_back(static_cast<char>(c));
		else if (c == ' ')
			out.push_back('+');
		else
		{
			char buf[8];
			std::snprintf(buf, sizeof(buf), "%%%02X", c);
			out += buf;
		}
	}
	return out;
}

void WikiBrowser::Init()
{
	/* Don't scan/kill processes at load — that can hitch startup. */
	CleanupStaleAddonRootFiles();
	gLaunchDisabled.store(false);
	gStarting.store(false);
	SetLocalStatus("Closed — press Ctrl+Shift+H to open");
}

void WikiBrowser::Shutdown()
{
	gWantVisible.store(false);
	StopHelper();
	KillStrayHelpers();
	ReleaseDevice();
	if (gFramePixels)
	{
		UnmapViewOfFile(gFramePixels);
		gFramePixels = nullptr;
	}
	if (gIpc)
	{
		UnmapViewOfFile(gIpc);
		gIpc = nullptr;
	}
	if (gFrameMap)
	{
		CloseHandle(gFrameMap);
		gFrameMap = nullptr;
	}
	if (gMap)
	{
		CloseHandle(gMap);
		gMap = nullptr;
	}
}

void WikiBrowser::SetVisible(bool visible)
{
	if (!visible)
	{
		const bool was = gWantVisible.exchange(false);
		gPendingNavigate.clear();
		if (was || HelperAlive())
		{
			PostCmd(WIKI_CMD_SET_VISIBLE, "0");
			if (G::KeepHelperWarm && HelperAlive())
			{
				SetLocalStatus("Ready");
			}
			else
			{
				StopHelper();
				SetLocalStatus("Closed — press Ctrl+Shift+H to open");
			}
		}
		return;
	}

	const bool wasWanted = gWantVisible.exchange(true);
	if (!EnsureIpc())
		return;
	const bool alreadyAlive = HelperAlive();
	if (!alreadyAlive && !StartHelper())
		return;
	/* Only notify on show/start — posting SET_VISIBLE every frame stomps
	   pending NAVIGATE/BACK/FORWARD/RELOAD commands (single IPC slot). */
	if (!wasWanted || !alreadyAlive)
		PostCmd(WIKI_CMD_SET_VISIBLE, "1");
	/* Tab URLs are loaded by BrowserTabs::NavigateActive / ready-resync —
	   do not force about:helper-home here (that fought live-tab restore). */
	FlushPendingNavigate();
}

void WikiBrowser::SetBounds(float, float, float width, float height)
{
	if (!gWantVisible.load() || !gIpc)
		return;
	if (width < 32.f || height < 32.f)
		return;

	uint32_t w = static_cast<uint32_t>(width);
	uint32_t h = static_cast<uint32_t>(height);
	if (w > kWikiFrameMaxW) w = kWikiFrameMaxW;
	if (h > kWikiFrameMaxH) h = kWikiFrameMaxH;

	if (gIpc->view_w == w && gIpc->view_h == h)
		return;
	gIpc->view_w = w;
	gIpc->view_h = h;
	/* Don't stomp a queued navigate — view size is already in IPC for GetViewRect. */
	if (gPendingNavigate.empty())
		PostCmd(WIKI_CMD_SET_BOUNDS);
}

void WikiBrowser::PresentFrame()
{
	if (!gWantVisible.load() || !gIpc || !gFramePixels)
		return;

	FlushPendingNavigate();

	if (!gIpc->ready || gIpc->frame_seq == 0)
		return;

	/* Snapshot + clamp — never trust IPC sizes blindly (avoids overruns). */
	const uint32_t seq = gIpc->frame_seq;
	uint32_t w = gIpc->frame_w;
	uint32_t h = gIpc->frame_h;
	if (w == 0 || h == 0 || w > kWikiFrameMaxW || h > kWikiFrameMaxH)
		return;
	if (seq == gLastFrameSeq)
		return;
	if (!EnsureTexture(w, h) || !gContext || !gTex)
		return;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(gContext->Map(gTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return;
	if (!mapped.pData)
	{
		gContext->Unmap(gTex, 0);
		return;
	}

	const uint8_t* src = gFramePixels;
	uint8_t* dst = static_cast<uint8_t*>(mapped.pData);
	const size_t rowBytes = static_cast<size_t>(w) * 4;
	for (uint32_t y = 0; y < h; ++y)
		std::memcpy(dst + y * mapped.RowPitch, src + y * kWikiFrameStride, rowBytes);
	gContext->Unmap(gTex, 0);
	gLastFrameSeq = seq;
}

void WikiBrowser::FeedMouseMove(int x, int y, bool leave, unsigned mods)
{
	if (!gIpc || !HelperAlive())
		return;
	if (gIpc->mouse_x == x && gIpc->mouse_y == y &&
		gIpc->mouse_mods == mods && gIpc->mouse_leave == (leave ? 1u : 0u))
		return;
	gIpc->mouse_x = x;
	gIpc->mouse_y = y;
	gIpc->mouse_mods = mods;
	gIpc->mouse_leave = leave ? 1u : 0u;
	++gIpc->mouse_seq;
}

void WikiBrowser::FeedMouseClick(int x, int y, int button, bool up, int clicks, unsigned mods)
{
	WikiInputEvent ev{};
	ev.type = WIKI_IN_MOUSE_CLICK;
	ev.x = x;
	ev.y = y;
	ev.a = button;
	ev.b = up ? 1 : 0;
	ev.c = clicks > 0 ? clicks : 1;
	ev.character = mods;
	PushInput(ev);
	/* Keep live mouse in sync. */
	FeedMouseMove(x, y, false, mods);
}

void WikiBrowser::FeedMouseWheel(int x, int y, int dx, int dy, unsigned mods)
{
	WikiInputEvent ev{};
	ev.type = WIKI_IN_MOUSE_WHEEL;
	ev.x = x;
	ev.y = y;
	ev.a = dx;
	ev.b = dy;
	ev.character = mods;
	PushInput(ev);
}

void WikiBrowser::FeedKey(int cefKeyType, int windowsVk, unsigned mods, unsigned character)
{
	WikiInputEvent ev{};
	ev.type = WIKI_IN_KEY;
	ev.a = cefKeyType;
	ev.b = windowsVk;
	ev.c = static_cast<int32_t>(mods);
	ev.character = character;
	PushInput(ev);
}

void WikiBrowser::FeedFocus(bool focused)
{
	WikiInputEvent ev{};
	ev.type = WIKI_IN_FOCUS;
	ev.a = focused ? 1 : 0;
	PushInput(ev);
}

void WikiBrowser::Navigate(const std::string& url)
{
	if (url.empty())
		return;
	gWantVisible.store(true);
	if (!EnsureIpc())
		return;
	if (!HelperAlive() && !StartHelper())
		return;
	QueueNavigate(url);
	FlushPendingNavigate();
}

void WikiBrowser::NavigateHome()
{
	Sites::SetActiveById("home");
	std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
	Settings::SetDirty();
	Navigate("about:helper-home");
}

void WikiBrowser::NavigateActiveSite()
{
	Navigate(Sites::ResolveUrl(Sites::Active()));
}

void WikiBrowser::Search(const std::string& query)
{
	const std::string url = Sites::SearchUrl(query);
	if (url.empty() || url == "about:helper-home")
		NavigateHome();
	else
		Navigate(url);
}

void WikiBrowser::GoBack() { PostCmd(WIKI_CMD_BACK); }
void WikiBrowser::GoForward() { PostCmd(WIKI_CMD_FORWARD); }
void WikiBrowser::Reload() { PostCmd(WIKI_CMD_RELOAD); }

void WikiBrowser::CreateTab(int slot, const char* url)
{
	if (slot < 0 || slot >= kWikiMaxTabs)
		return;
	const std::string resolved = ResolveNavigateUrl(url ? url : "about:blank");
	if (resolved.empty())
		return;
	PostCmd(WIKI_CMD_CREATE_TAB, resolved.c_str(), slot);
}

void WikiBrowser::ActivateTab(int slot)
{
	if (slot < 0 || slot >= kWikiMaxTabs)
		return;
	PostCmd(WIKI_CMD_ACTIVATE_TAB, "", slot);
}

void WikiBrowser::CloseTab(int slot)
{
	if (slot < 0 || slot >= kWikiMaxTabs)
		return;
	PostCmd(WIKI_CMD_CLOSE_TAB, "", slot);
}

void WikiBrowser::Find(const char* text, bool forward, bool matchCase, bool findNext)
{
	if (!text || !text[0])
		return;
	int32_t flags = 0;
	if (forward) flags |= 1;
	if (matchCase) flags |= 2;
	if (findNext) flags |= 4;
	PostCmd(WIKI_CMD_FIND, text, flags);
}

void WikiBrowser::StopFind(bool clearSelection)
{
	PostCmd(WIKI_CMD_STOP_FIND, "", clearSelection ? 1 : 0);
}

bool WikiBrowser::IsReady()
{
	return gIpc && gIpc->ready && HelperAlive();
}

bool WikiBrowser::HasTab(int slot)
{
	if (!gIpc || !HelperAlive() || slot < 0 || slot >= kWikiMaxTabs)
		return false;
	return (gIpc->tab_mask & (1u << slot)) != 0;
}

bool WikiBrowser::HasFrame()
{
	return gSrv && gIpc && gIpc->frame_seq > 0 && gTexW > 0 && gTexH > 0;
}

ID3D11ShaderResourceView* WikiBrowser::FrameSrv()
{
	return gSrv;
}

int WikiBrowser::FrameWidth() { return static_cast<int>(gTexW); }
int WikiBrowser::FrameHeight() { return static_cast<int>(gTexH); }

bool WikiBrowser::CanGoBack() { return gIpc && gIpc->can_back; }
bool WikiBrowser::CanGoForward() { return gIpc && gIpc->can_forward; }

std::string WikiBrowser::CurrentUrl()
{
	return gIpc ? std::string(gIpc->url) : std::string{};
}

std::string WikiBrowser::CurrentTitle()
{
	return gIpc ? std::string(gIpc->title) : std::string{};
}

std::string WikiBrowser::Status()
{
	if (gIpc && gIpc->status[0] && HelperAlive())
		return gIpc->status;
	std::lock_guard<std::mutex> lock(gMutex);
	return gStatus;
}

uint32_t WikiBrowser::FindCount()
{
	return gIpc ? gIpc->find_count : 0;
}

uint32_t WikiBrowser::FindOrdinal()
{
	return gIpc ? gIpc->find_ordinal : 0;
}
