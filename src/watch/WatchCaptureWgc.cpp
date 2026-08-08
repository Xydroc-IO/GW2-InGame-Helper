#include "WatchCaptureWgc.h"
#include "WatchCaptureWgcInternal.h"

#include "EiRuntime.h"

#include <mutex>
#include <string>

using namespace WatchCaptureWgcDetail;

bool WatchCaptureWgc::Available()
{
	if (EiRuntime::IsWine())
		return false;
	static int s = -1;
	if (s < 0)
	{
		if (!InteropOk())
		{
			s = 0;
			return false;
		}
		const HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
		s = SessionSupported() ? 1 : 0;
		if (SUCCEEDED(hr))
			RoUninitialize();
	}
	return s != 0;
}

bool WatchCaptureWgc::IsCapturing() { return gCapturing.load(); }
bool WatchCaptureWgc::IsPickerOpen() { return gPickerOpen.load(); }

bool WatchCaptureWgc::StartPicker(std::string& status)
{
	if (!Available())
	{
		status = "Windows Graphics Capture not supported.";
		return false;
	}
	if (gPickerOpen.load() || gCapturing.load())
	{
		status = gPickerOpen.load() ? "Picker already open…" : "Already capturing.";
		return gCapturing.load();
	}
	gStop = false;
	SetStatus("Opening system picker…");
	HANDLE th = CreateThread(nullptr, 0, PickerThread, nullptr, 0, nullptr);
	if (!th)
	{
		status = "Failed to start picker thread.";
		return false;
	}
	CloseHandle(th);
	status = "Opening system picker…";
	return true;
}

void WatchCaptureWgc::Stop()
{
	gStop = true;
	gCapturing = false;
	std::lock_guard<std::recursive_mutex> lock(gMu);
	TearDownSession_NoLock();
	gStatus = "Stopped.";
}

void WatchCaptureWgc::GetStatus(std::string& status)
{
	std::lock_guard<std::recursive_mutex> lock(gMu);
	status = gStatus;
}
