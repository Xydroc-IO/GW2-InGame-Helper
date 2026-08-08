#include "WatchLinux.h"
#include "WatchLinuxInternal.h"

#include "EiRuntime.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace WatchLinuxDetail;

namespace WatchLinuxDetail
{
	bool     gWsa = false;
	SOCKET   gSock = INVALID_SOCKET;
	uint64_t gTarget = 0;
	std::atomic<bool> gCapturing{ false };
	std::atomic<bool> gStartBusy{ false };
	std::atomic<bool> gWarmBusy{ false };
	std::atomic<bool> gPumpRun{ false };
	HANDLE   gPumpThread = nullptr;
	HANDLE   gStartThread = nullptr;
	HANDLE   gWarmThread = nullptr;
	CRITICAL_SECTION gCs{};
	bool     gCsInit = false;

	HANDLE   gShmFile = INVALID_HANDLE_VALUE;
	HANDLE   gShmMap = nullptr;
	uint8_t* gShmView = nullptr;
	uint32_t gProtoVer = 0;
	uint32_t gPresentSlot = WatchProto::kNoSlot;
	uint32_t gLastPresentedSeq = 0;
	std::string gPumpStatus = "Idle.";
	std::string gUiStatus = "Idle.";

	void EnsureCs()
	{
		if (!gCsInit)
		{
			InitializeCriticalSection(&gCs);
			gCsInit = true;
		}
	}

	void SetUiStatus(const char* s)
	{
		EnsureCs();
		EnterCriticalSection(&gCs);
		gUiStatus = s ? s : "";
		gPumpStatus = gUiStatus;
		LeaveCriticalSection(&gCs);
	}

	void CopyUiStatus(std::string& out)
	{
		EnsureCs();
		EnterCriticalSection(&gCs);
		out = gUiStatus;
		LeaveCriticalSection(&gCs);
	}

	DWORD WINAPI WarmWorker(LPVOID)
	{
		EnsureCs();
		SetUiStatus("Warming watchd…");
		const bool ok = ConnectShared();
		SetUiStatus(ok
			? "Ready — Start opens the desktop share picker."
			: "watchd not ready (portal/PipeWire). Try Start.");
		gWarmBusy = false;
		return 0;
	}

	DWORD WINAPI StartWorker(LPVOID)
	{
		EnsureCs();
		/* Let a warm pass finish so we don't double-spawn. */
		for (int i = 0; i < 40 && gWarmBusy.load(); ++i)
			Sleep(50);
		SetUiStatus("Starting watchd…");
		if (!ConnectShared())
		{
			gCapturing = false;
			gTarget = 0;
			SetUiStatus("Could not start watchd (chmod/spawn). "
				"Need xdg-desktop-portal + PipeWire.");
			gStartBusy = false;
			return 0;
		}
		if (!EnsureShmMapped())
		{
			gCapturing = false;
			gTarget = 0;
			SetUiStatus("Frame map unavailable (/dev/shm).");
			gStartBusy = false;
			return 0;
		}

		EnterCriticalSection(&gCs);
		uint64_t zero = 0;
		const bool sent = (gSock != INVALID_SOCKET) &&
			SendCmd(gSock, WatchProto::CmdStart, &zero, 8);
		std::string status;
		if (sent)
		{
			for (int i = 0; i < 6; ++i)
			{
				u_long avail = 0;
				ioctlsocket(gSock, FIONREAD, &avail);
				if (avail < sizeof(WatchProto::Header))
					break;
				uint32_t type = 0;
				std::vector<uint8_t> body;
				if (!RecvMsg(gSock, type, body))
					break;
				if (type == WatchProto::MsgStatus || type == WatchProto::MsgErr)
					status.assign(reinterpret_cast<char*>(body.data()), body.size());
			}
		}
		LeaveCriticalSection(&gCs);

		if (!sent)
		{
			gCapturing = false;
			gTarget = 0;
			if (gSock != INVALID_SOCKET)
			{
				closesocket(gSock);
				gSock = INVALID_SOCKET;
			}
			SetUiStatus("START failed.");
			gStartBusy = false;
			return 0;
		}

		gTarget = 1;
		gCapturing = true;
		SetUiStatus(status.empty()
			? "Portal picker… then capturing (OOP ~60 FPS)."
			: status.c_str());
		gStartBusy = false;
		return 0;
	}
}

bool WatchLinux::Available()
{
	return EiRuntime::IsWine();
}

void WatchLinux::WarmAsync()
{
	if (!Available())
		return;
	EnsureCs();
	EnsurePump();
	if (gSock != INVALID_SOCKET || gWarmBusy.load() || gStartBusy.load())
		return;
	bool expected = false;
	if (!gWarmBusy.compare_exchange_strong(expected, true))
		return;
	if (gWarmThread)
	{
		CloseHandle(gWarmThread);
		gWarmThread = nullptr;
	}
	gWarmThread = CreateThread(nullptr, 0, WarmWorker, nullptr, 0, nullptr);
	if (!gWarmThread)
		gWarmBusy = false;
	else
		SetThreadPriority(gWarmThread, THREAD_PRIORITY_BELOW_NORMAL);
}

bool WatchLinux::EnsureDaemon()
{
	EnsureCs();
	EnsurePump();
	return ConnectShared();
}

void WatchLinux::Disconnect()
{
	gCapturing = false;
	gTarget = 0;
	gStartBusy = false;
	StopPump();
	if (gStartThread)
	{
		WaitForSingleObject(gStartThread, 1500);
		CloseHandle(gStartThread);
		gStartThread = nullptr;
	}
	if (gWarmThread)
	{
		WaitForSingleObject(gWarmThread, 500);
		CloseHandle(gWarmThread);
		gWarmThread = nullptr;
	}
	gWarmBusy = false;
	if (gSock != INVALID_SOCKET)
	{
		closesocket(gSock);
		gSock = INVALID_SOCKET;
	}
	UnmapShm();
	EnsureCs();
	gLastPresentedSeq = 0;
	gPresentSlot = WatchProto::kNoSlot;
}

void WatchLinux::RefreshWindowList(std::vector<WatchCapture::WindowEntry>& out, std::string& status)
{
	out.clear();
	EnsureCs();
	EnsurePump();
	/* Never ConnectShared here — that Sleep-loop froze the game when opening Watch. */
	WarmAsync();
	if (gSock != INVALID_SOCKET)
		status = "Ready — Start opens the desktop share picker (portal/PipeWire).";
	else if (gWarmBusy.load() || gStartBusy.load())
		status = "Warming watchd…";
	else
		status = "Ready — Start opens the desktop share picker (portal/PipeWire).";
}

bool WatchLinux::Start(uint64_t id, std::string& status)
{
	(void)id; /* portal picker owns window selection */
	EnsureCs();
	EnsurePump();
	if (gStartBusy.load())
	{
		CopyUiStatus(status);
		if (status.empty())
			status = "Starting…";
		return true;
	}
	if (gCapturing.load() && gTarget != 0)
	{
		CopyUiStatus(status);
		return true;
	}

	bool expected = false;
	if (!gStartBusy.compare_exchange_strong(expected, true))
	{
		status = "Starting…";
		return true;
	}

	gTarget = 1;
	gCapturing = true; /* UI treats as active while portal/daemon comes up */
	SetUiStatus("Starting watchd…");
	CopyUiStatus(status);

	if (gStartThread)
	{
		CloseHandle(gStartThread);
		gStartThread = nullptr;
	}
	gStartThread = CreateThread(nullptr, 0, StartWorker, nullptr, 0, nullptr);
	if (!gStartThread)
	{
		gStartBusy = false;
		gCapturing = false;
		gTarget = 0;
		status = "Could not start worker thread.";
		SetUiStatus(status.c_str());
		return false;
	}
	SetThreadPriority(gStartThread, THREAD_PRIORITY_BELOW_NORMAL);
	return true;
}

void WatchLinux::Stop(std::string& status)
{
	gCapturing = false;
	gTarget = 0;
	EndPresent();
	EnsureCs();
	EnterCriticalSection(&gCs);
	if (gSock != INVALID_SOCKET)
		SendCmd(gSock, WatchProto::CmdStop, nullptr, 0);
	LeaveCriticalSection(&gCs);
	status = "Stopped.";
	SetUiStatus("Stopped.");
}

bool WatchLinux::IsCapturing()
{
	return gCapturing.load() && gTarget != 0;
}

bool WatchLinux::IsStarting()
{
	return gStartBusy.load() || gWarmBusy.load();
}

void WatchLinux::GetStatus(std::string& out)
{
	CopyUiStatus(out);
}

uint64_t WatchLinux::TargetId()
{
	return gTarget;
}

bool WatchLinux::BeginPresent(const uint8_t*& bgra, uint32_t& w, uint32_t& h, uint32_t& stride,
	std::string& status)
{
	/* Game thread present only — capture lives in host watchd (CEF helper pattern). */
	bgra = nullptr;
	w = h = stride = 0;
	CopyUiStatus(status);
	if (!gCapturing.load() || !EnsureShmMapped() || !gShmView)
		return false;

	auto* hdr = reinterpret_cast<WatchProto::ShmHeader*>(gShmView);
	if (hdr->magic != WatchProto::kShmMagic || hdr->seq == 0 || hdr->seq == gLastPresentedSeq)
		return false;

	const uint32_t slot = hdr->slot & 1u;
	const uint32_t fw = hdr->w;
	const uint32_t fh = hdr->h;
	const uint32_t fs = hdr->stride;
	if (fw == 0 || fh == 0 || fs < fw * 4 || fw > WatchProto::kMaxW || fh > WatchProto::kMaxH)
		return false;
	if (static_cast<size_t>(fs) * fh > WatchProto::kSlotBytes)
		return false;

	hdr->reading = slot;
	__sync_synchronize();
	gPresentSlot = slot;
	gLastPresentedSeq = hdr->seq;
	bgra = WatchProto::ShmSlotPixels(gShmView, slot);
	w = fw;
	h = fh;
	stride = fs;
	status = "Capturing (OOP watchd ~60 FPS).";
	SetUiStatus(status.c_str());
	return true;
}

void WatchLinux::EndPresent()
{
	if (!gShmView)
	{
		gPresentSlot = WatchProto::kNoSlot;
		return;
	}
	auto* hdr = reinterpret_cast<WatchProto::ShmHeader*>(gShmView);
	hdr->reading = WatchProto::kNoSlot;
	__sync_synchronize();
	gPresentSlot = WatchProto::kNoSlot;
}
