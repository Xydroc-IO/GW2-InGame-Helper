#include "WatchCapture.h"
#include "WatchCaptureInternal.h"
#include "WatchCaptureWgc.h"
#include "WatchLinux.h"
#include "WatchPadInternal.h"

#include "CrashTrail.h"
#include "Globals.h"
#include "EiRuntime.h"
#include "WinePadOpen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

using namespace WatchCaptureDetail;

namespace
{
	bool gClassicList = false;

	bool ApplyChromeCrop(const uint8_t*& ptr, uint32_t& w, uint32_t& h, uint32_t stride)
	{
		if (!ptr || w < 16 || h < 16 || stride < w * 4)
			return false;
		const float topF = (std::clamp)(G::WatchCropTop, 0.f, 0.45f);
		const float botF = (std::clamp)(G::WatchCropBottom, 0.f, 0.45f);
		const float leftF = (std::clamp)(G::WatchCropLeft, 0.f, 0.45f);
		const float rightF = (std::clamp)(G::WatchCropRight, 0.f, 0.45f);
		uint32_t top = static_cast<uint32_t>(h * topF + 0.5f);
		uint32_t bot = static_cast<uint32_t>(h * botF + 0.5f);
		uint32_t left = static_cast<uint32_t>(w * leftF + 0.5f);
		uint32_t right = static_cast<uint32_t>(w * rightF + 0.5f);
		if (top + bot >= h - 8)
		{
			top = 0;
			bot = 0;
		}
		if (left + right >= w - 8)
		{
			left = 0;
			right = 0;
		}
		if (top == 0 && bot == 0 && left == 0 && right == 0)
			return true;
		ptr += static_cast<size_t>(top) * stride + static_cast<size_t>(left) * 4u;
		w -= left + right;
		h -= top + bot;
		return w >= 8 && h >= 8;
	}

	bool IsOwnProcess(HWND hwnd)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		return pid != 0 && pid == GetCurrentProcessId();
	}

	std::string NarrowUtf8(const wchar_t* w, int n)
	{
		if (!w || n <= 0)
			return {};
		char out[768]{};
		WideCharToMultiByte(CP_UTF8, 0, w, n, out, static_cast<int>(sizeof(out) - 1),
			nullptr, nullptr);
		return out;
	}

	std::string WindowLabel(HWND hwnd)
	{
		wchar_t titleW[512]{};
		const int nt = GetWindowTextW(hwnd, titleW, 512);
		std::string title = NarrowUtf8(titleW, nt);
		wchar_t classW[256]{};
		const int nc = GetClassNameW(hwnd, classW, 256);
		std::string cls = NarrowUtf8(classW, nc);
		if (!title.empty() && !cls.empty())
			return title + "  [" + cls + "]";
		if (!title.empty())
			return title;
		if (!cls.empty())
			return std::string("(") + cls + ")";
		char buf[64]{};
		std::snprintf(buf, sizeof(buf), "(hwnd %p)", static_cast<void*>(hwnd));
		return buf;
	}

	bool AlreadyListed(uint64_t id)
	{
		for (const auto& e : gWindows)
		{
			if (e.id == id)
				return true;
		}
		return false;
	}

	bool LooksUseful(HWND hwnd)
	{
		if (!hwnd || !IsWindow(hwnd))
			return false;
		if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
			return false;
		if (GetWindow(hwnd, GW_OWNER) != nullptr)
			return false;
		const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
		if (!(style & WS_VISIBLE))
			return false;
		if (GetParent(hwnd) != nullptr)
			return false;
		const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		if (ex & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))
			return false;
		if (IsOwnProcess(hwnd))
			return false;

		RECT rc{};
		if (!GetWindowRect(hwnd, &rc))
			return false;
		const int w = rc.right - rc.left;
		const int h = rc.bottom - rc.top;
		if (w < 200 || h < 120)
			return false;

		wchar_t titleW[8]{};
		if (GetWindowTextW(hwnd, titleW, 8) <= 0)
			return false;
		return true;
	}

	bool TryAdd(HWND hwnd)
	{
		++gRawEnumCount;
		if (!LooksUseful(hwnd) || AlreadyListed(reinterpret_cast<uint64_t>(hwnd)))
			return true;
		WatchCapture::WindowEntry e;
		e.id = reinterpret_cast<uint64_t>(hwnd);
		e.title = WindowLabel(hwnd);
		if (e.title.empty())
			return true;
		if (e.title.size() > 100)
			e.title.resize(100);
		gWindows.push_back(std::move(e));
		return true;
	}

	BOOL CALLBACK EnumProc(HWND hwnd, LPARAM)
	{
		TryAdd(hwnd);
		return TRUE;
	}
}

void WatchCapture::RefreshWindowList()
{
	gWindows.clear();
	gRawEnumCount = 0;

	if (WatchLinux::Available())
	{
		WatchLinux::RefreshWindowList(gWindows, gStatus);
		gRawEnumCount = static_cast<int>(gWindows.size());
		return;
	}

	/* Top-level titled windows only — EnumChildWindows(desktop) floods ImGui. */
	EnumWindows(EnumProc, 0);
	std::sort(gWindows.begin(), gWindows.end(),
		[](const WindowEntry& a, const WindowEntry& b) { return a.title < b.title; });
	constexpr size_t kMaxList = 80;
	if (gWindows.size() > kMaxList)
		gWindows.resize(kMaxList);

	char buf[192]{};
	if (gWindows.empty())
	{
		std::snprintf(buf, sizeof(buf),
			"No titled windows found (raw %d). Open the player, then Refresh.",
			gRawEnumCount);
	}
	else
	{
		std::snprintf(buf, sizeof(buf), "Listed %d window(s). Select one, then Start.",
			static_cast<int>(gWindows.size()));
	}
	gStatus = buf;
}

const std::vector<WatchCapture::WindowEntry>& WatchCapture::Windows()
{
	return gWindows;
}

int WatchCapture::RawEnumCount()
{
	return gRawEnumCount;
}

bool WatchCapture::WgcAvailable()
{
	return !WatchLinux::Available() && WatchCaptureWgc::Available();
}

bool WatchCapture::ClassicListMode()
{
	return gClassicList;
}

void WatchCapture::SetClassicListMode(bool on)
{
	gClassicList = on;
}

bool WatchCapture::StartWgcPicker()
{
	Stop();
	if (!WgcAvailable())
	{
		gClassicList = true;
		gStatus = "Windows Graphics Capture unavailable — use Classic list.";
		return false;
	}
	gClassicList = false;
	gTarget = 1;
	gCapturing = true;
	gLastCaptureMs = 0;
	gLastBlank = false;
	if (!WatchCaptureWgc::StartPicker(gStatus))
	{
		gCapturing = false;
		gTarget = 0;
		gClassicList = true;
		if (gStatus.empty())
			gStatus = "System picker failed — use Classic list.";
		return false;
	}
	return true;
}

bool WatchCapture::Start(uint64_t id)
{
	/* Avoid Stop→RequestGpuRelease when already idle — reopen was parking
	   every Soft Start and tipping Wine under load. */
	if (IsCapturing() || IsStreaming() || WatchCaptureWgc::IsPickerOpen())
		Stop();
	/* Do not WatchLinux::Stop when merely IsStarting — that SendCmd on Soft Start
	   after Soft-stop tipped Wine. Wait for pump / cooldown instead. */

	if (WatchLinux::Available())
	{
		if (!WatchLinux::Start(id, gStatus))
			return false;
		/* Capturing flag lives in WatchLinux — only set DLL mirror after Start
		   actually armed the pump request (not needPump-only). */
		if (WatchLinux::IsCapturing() || WatchLinux::IsStarting())
		{
			gTarget = 1;
			gCapturing = true;
		}
		gLastCaptureMs = 0;
		gLastBlank = false;
		gContentW = gContentH = 0;
		return true;
	}

	/* id == 0 → system GraphicsCapturePicker when available. */
	if (id == 0 && WatchCaptureWgc::Available() && !gClassicList)
		return StartWgcPicker();

	if (id == 0)
	{
		gStatus = "Invalid window.";
		return false;
	}
	HWND hwnd = reinterpret_cast<HWND>(id);
	if (!IsWindow(hwnd))
	{
		gStatus = "Invalid window.";
		return false;
	}
	gClassicList = true;
	gTarget = id;
	gCapturing = true;
	gLastCaptureMs = 0;
	gLastBlank = false;
	gStatus = "Capturing…";
	EnsureWinPump();
	return true;
}

void WatchCapture::Stop()
{
	if (WatchLinux::Available())
	{
		/* Always CmdStop on Wine — IsCapturing alone missed sticky sessions and
		   left Start disabled / Soft-open Soft-stop-looping. */
		std::string st;
		WatchLinux::Stop(st);
		gStatus = st;
	}
	WatchCaptureWgc::Stop();
	gCapturing = false;
	gTarget = 0;
	gLastBlank = false;
	ResetWinReady();
	RequestGpuRelease();
	if (gStatus != "Stopped." && gStatus.find("daemon") == std::string::npos
		&& gStatus.find("Picker") == std::string::npos)
		gStatus = "Stopped.";
}

void WatchCapture::SoftStopCapture()
{
	CrashTrail::NoteF("cap:SoftStop enter linux=%d wgc=%d",
		WatchLinux::Available() ? 1 : 0,
		WatchCaptureWgc::IsCapturing() ? 1 : 0);
	if (WatchLinux::Available())
	{
		CrashTrail::Note("cap:pre WatchLinux::Stop");
		std::string st;
		WatchLinux::Stop(st);
		gStatus = st;
		CrashTrail::Note("cap:post WatchLinux::Stop");
	}
	CrashTrail::Note("cap:pre Wgc::Stop");
	WatchCaptureWgc::Stop();
	CrashTrail::Note("cap:post Wgc::Stop");
	gCapturing = false;
	gTarget = 0;
	gLastBlank = false;
	ResetWinReady();
	HideContent();
	/* Intentionally no RequestGpuRelease — Soft-open after Soft-stop tipped Wine. */
	if (gStatus != "Stopped." && gStatus.find("daemon") == std::string::npos
		&& gStatus.find("Picker") == std::string::npos)
		gStatus = "Stopped.";
	CrashTrail::Note("cap:SoftStop leave");
}

bool WatchCapture::IsCapturing()
{
	if (WatchLinux::Available())
		return WatchLinux::IsCapturing();
	if (WatchCaptureWgc::IsPickerOpen())
		return true;
	if (WatchCaptureWgc::IsCapturing())
		return true;
	return gCapturing && gTarget != 0;
}

bool WatchCapture::IsStreaming()
{
	/* Real pixels uploaded — not portal/picker wait (IsCapturing alone is too early). */
	return IsCapturing() && HasContent();
}

bool WatchCapture::HasContent()
{
	return gSrv != nullptr && gContentW > 0 && gContentH > 0;
}

uint64_t WatchCapture::TargetId()
{
	if (WatchLinux::Available())
		return WatchLinux::TargetId();
	return gTarget;
}

void WatchCapture::Tick()
{
	/* Soft-stop quiet: no uploads and do not park/release SRVs mid Soft-stop. */
	if (WatchPadDetail::gSoftStopPhase > 0)
		return;

	/* Wine: trail tipped right after softstop:defer_done — first Tick/Flush
	   after defer often Release()'s parked GPU while Events ImGui is live.
	   Hold all GPU flush/upload until soft-stop cooldown drains . */
	if (EiRuntime::IsWine()
		&& (WatchPadDetail::gDeferStopFrames > 0
			|| WatchPadDetail::gPostStopCooldown > 0
			|| WatchPadDetail::gReopenGateFrames > 0))
	{
		if (CrashTrail::DetailArmed())
			CrashTrail::NoteF("cap:Tick skip poststop defer=%d cool=%d gate=%d",
				WatchPadDetail::gDeferStopFrames,
				WatchPadDetail::gPostStopCooldown,
				WatchPadDetail::gReopenGateFrames);
		return;
	}

	if (CrashTrail::DetailArmed())
		CrashTrail::Note("cap:pre FlushDeferredGpuRelease");
	FlushDeferredGpuRelease();
	if (CrashTrail::DetailArmed())
		CrashTrail::Note("cap:post FlushDeferredGpuRelease");

	if (!IsCapturing())
		return;

	/* Soft-stop in flight — no uploads after long sessions (click tipped Wine). */
	if (WatchPadDetail::gDeferStopFrames > 0 || WatchPadDetail::gSoftStopPhase > 0)
		return;
	if (WatchPadDetail::gUploadHoldFrames > 0)
		return;

	/* Soft companion/rail: never upload while SoftWorkBusy (Present is paused). */
	if (WinePadOpen::SoftWorkBusy())
		return;
	if (WinePadOpen::WatchMirrorQuietFrames() > 0 || WinePadOpen::WatchSoftOpenFired())
		return;
	/* Live Mirror being dragged/clicked — skip upload (prior-frame flag). */
	if (WatchPadDetail::gMirrorInputBusy)
		return;
	/* Upload for visible Mirror, Soft-open defer, or silent first-frame wait. */
	if (!G::ShowWatchMirror && !WatchPadDetail::gWantMirrorWhenReady
		&& WatchPadDetail::gDeferMirrorOpenFrames <= 0)
		return;

	const DWORD now = GetTickCount();
	const DWORD minMs = EiRuntime::IsWine() ? kMinFrameMsWine : kMinFrameMsNative;
	if (gLastCaptureMs != 0 && (now - gLastCaptureMs) < minMs)
		return;

	std::vector<uint8_t> bgra;
	uint32_t w = 0, h = 0, stride = 0;
	bool linuxPinned = false;

	if (WatchLinux::Available())
	{
		const uint8_t* ptr = nullptr;
		if (!WatchLinux::BeginPresent(ptr, w, h, stride, gStatus) || !ptr)
			return;
		linuxPinned = true;
		if (!ApplyChromeCrop(ptr, w, h, stride))
		{
			WatchLinux::EndPresent();
			gStatus = "Crop left no content — reduce Crop chrome.";
			return;
		}
		gLastBlank = SampleLooksBlank(ptr, w, h, stride);
		if (!UploadBgra(ptr, w, h, stride))
		{
			WatchLinux::EndPresent();
			gStatus = "GPU upload failed.";
			return;
		}
		WatchLinux::EndPresent();
	}
	else if (WatchCaptureWgc::IsCapturing() || WatchCaptureWgc::IsPickerOpen())
	{
		WatchCaptureWgc::GetStatus(gStatus);
		if (WatchCaptureWgc::IsPickerOpen() && !WatchCaptureWgc::IsCapturing())
			return;
		if (!WatchCaptureWgc::IsCapturing())
		{
			/* Picker finished without a session (cancel / fail). */
			gCapturing = false;
			gTarget = 0;
			if (gStatus.find("Classic") != std::string::npos
				|| gStatus.find("failed") != std::string::npos
				|| gStatus.find("unavailable") != std::string::npos)
				gClassicList = true;
			return;
		}
		if (!WatchCaptureWgc::TakeFrame(bgra, w, h, stride, kMaxCaptureW, kMaxCaptureH))
			return;
		const uint8_t* ptr = bgra.data();
		if (!ApplyChromeCrop(ptr, w, h, stride))
		{
			gStatus = "Crop left no content — reduce Crop chrome.";
			return;
		}
		gLastBlank = SampleLooksBlank(ptr, w, h, stride);
		if (!UploadBgra(ptr, w, h, stride))
		{
			gStatus = "GPU upload failed.";
			return;
		}
		WatchCaptureWgc::GetStatus(gStatus);
	}
	else
	{
		HWND hwnd = reinterpret_cast<HWND>(gTarget);
		if (!IsWindow(hwnd))
		{
			gStatus = "Target window closed.";
			Stop();
			return;
		}
		if (!TakeWinFrame(bgra, w, h, stride))
			return;
		const uint8_t* ptr = bgra.data();
		if (!ApplyChromeCrop(ptr, w, h, stride))
		{
			gStatus = "Crop left no content — reduce Crop chrome.";
			return;
		}
		gStatus = "Capturing.";
		gLastBlank = SampleLooksBlank(ptr, w, h, stride);
		if (!UploadBgra(ptr, w, h, stride))
		{
			gStatus = "GPU upload failed.";
			return;
		}
	}

	gLastCaptureMs = now;
	(void)linuxPinned;

	if (gLastBlank)
		gStatus = "Capturing — frame looks black (DRM / protected overlay?).";
	else if (!WatchLinux::Available() && !WatchCaptureWgc::IsCapturing())
		gStatus = "Capturing.";
}

ID3D11ShaderResourceView* WatchCapture::Srv()
{
	return gSrv;
}

uint32_t WatchCapture::ContentW()
{
	return gContentW;
}

uint32_t WatchCapture::ContentH()
{
	return gContentH;
}

float WatchCapture::ContentU()
{
	return (gTexW > 0 && gContentW > 0)
		? static_cast<float>(gContentW) / static_cast<float>(gTexW) : 1.f;
}

float WatchCapture::ContentV()
{
	return (gTexH > 0 && gContentH > 0)
		? static_cast<float>(gContentH) / static_cast<float>(gTexH) : 1.f;
}

bool WatchCapture::GpuParkBusy()
{
	/* Dead/deferred GPU only — do not fold in IsStarting (that blocked Soft-open
	   Watch forever after a Start, and raced reopen Begin). */
	return gDeferGpuRelease || gDeadSrv != nullptr || gDeadMirrorTex != nullptr;
}

bool WatchCapture::DedicatedMirrorDevice()
{
	return WatchCaptureDetail::gDedicatedMirror;
}

const char* WatchCapture::MirrorGpuPathText()
{
	return WatchCaptureDetail::gMirrorGpuPath;
}

void WatchCapture::HideContent()
{
	WatchCaptureDetail::HideContent();
}

const char* WatchCapture::StatusText()
{
	if (WatchLinux::Available())
		WatchLinux::GetStatus(gStatus);
	else if (WatchCaptureWgc::IsCapturing() || WatchCaptureWgc::IsPickerOpen())
		WatchCaptureWgc::GetStatus(gStatus);
	return gStatus.c_str();
}

bool WatchCapture::LastFrameLookedBlank()
{
	return gLastBlank;
}

void WatchCapture::Shutdown()
{
	Stop();
	FlushDeferredGpuRelease();
	StopWinPump();
	WatchLinux::Disconnect();
	gWindows.clear();
	ReleaseDevice();
	gStatus = "Idle — Start opens the system picker.";
}
