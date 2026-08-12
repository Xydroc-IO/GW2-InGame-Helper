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
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

using namespace WatchCaptureDetail;

namespace WatchCaptureDetail
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

