#include "WatchCapture.h"
#include "WatchCaptureInternal.h"
#include "WatchLinux.h"

#include "EiRuntime.h"
#include "Globals.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

using namespace WatchCaptureDetail;

namespace
{
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
		const bool visible = IsWindowVisible(hwnd) != FALSE;
		const bool iconic = IsIconic(hwnd) != FALSE;
		if (!visible && !iconic && !EiRuntime::IsWine())
			return false;
		if (GetWindow(hwnd, GW_OWNER) != nullptr)
			return false;
		const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		if ((ex & WS_EX_TOOLWINDOW) && !EiRuntime::IsWine())
			return false;
		if (IsOwnProcess(hwnd))
			return false;

		RECT rc{};
		if (!GetWindowRect(hwnd, &rc))
			return false;
		const int w = rc.right - rc.left;
		const int h = rc.bottom - rc.top;
		const int minEdge = EiRuntime::IsWine() ? 16 : 64;
		if (w < minEdge || h < minEdge)
			return false;

		wchar_t titleW[4]{};
		wchar_t classW[4]{};
		const bool hasTitle = GetWindowTextW(hwnd, titleW, 4) > 0;
		const bool hasClass = GetClassNameW(hwnd, classW, 4) > 0;
		return hasTitle || hasClass;
	}

	bool TryAdd(HWND hwnd)
	{
		++gRawEnumCount;
		if (!LooksUseful(hwnd) || AlreadyListed(reinterpret_cast<uint64_t>(hwnd)))
			return true;
		WatchCapture::WindowEntry e;
		e.id = reinterpret_cast<uint64_t>(hwnd);
		e.title = WindowLabel(hwnd);
		if (e.title.size() > 120)
			e.title.resize(120);
		gWindows.push_back(std::move(e));
		return true;
	}

	BOOL CALLBACK EnumProc(HWND hwnd, LPARAM)
	{
		TryAdd(hwnd);
		return TRUE;
	}

	BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM)
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

	EnumWindows(EnumProc, 0);
	if (HWND desk = GetDesktopWindow())
		EnumChildWindows(desk, EnumChildProc, 0);
	std::sort(gWindows.begin(), gWindows.end(),
		[](const WindowEntry& a, const WindowEntry& b) { return a.title < b.title; });

	char buf[192]{};
	if (gWindows.empty())
	{
		std::snprintf(buf, sizeof(buf),
			"No windows found (raw %d). Click Refresh after opening the player.",
			gRawEnumCount);
	}
	else
	{
		std::snprintf(buf, sizeof(buf), "Listed %d window(s).",
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

bool WatchCapture::Start(uint64_t id)
{
	Stop();

	if (WatchLinux::Available())
	{
		if (!WatchLinux::Start(id, gStatus))
			return false;
		gTarget = 1;
		gCapturing = true;
		gLastCaptureMs = 0;
		gLastBlank = false;
		return true;
	}

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
	if (WatchLinux::Available() && WatchLinux::IsCapturing())
	{
		std::string st;
		WatchLinux::Stop(st);
		gStatus = st;
	}
	gCapturing = false;
	gTarget = 0;
	gLastBlank = false;
	ResetWinReady();
	ReleaseGpu();
	if (gStatus != "Stopped." && gStatus.find("daemon") == std::string::npos)
		gStatus = "Stopped.";
}

bool WatchCapture::IsCapturing()
{
	if (WatchLinux::Available())
		return WatchLinux::IsCapturing();
	return gCapturing && gTarget != 0;
}

uint64_t WatchCapture::TargetId()
{
	if (WatchLinux::Available())
		return WatchLinux::TargetId();
	return gTarget;
}

void WatchCapture::Tick()
{
	if (!IsCapturing())
		return;

	const DWORD now = GetTickCount();
	if (gLastCaptureMs != 0 && (now - gLastCaptureMs) < kMinFrameMs)
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
	else if (!WatchLinux::Available())
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

const char* WatchCapture::StatusText()
{
	if (WatchLinux::Available())
		WatchLinux::GetStatus(gStatus);
	return gStatus.c_str();
}

bool WatchCapture::LastFrameLookedBlank()
{
	return gLastBlank;
}

void WatchCapture::Shutdown()
{
	Stop();
	StopWinPump();
	WatchLinux::Disconnect();
	gWindows.clear();
	ReleaseDevice();
	gStatus = "Idle — pick a window and Start.";
}
