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
	std::atomic<uint32_t> gStartEpoch{ 0 };
	std::atomic<uint32_t> gQueuedStartEpoch{ 0 };
	std::atomic<bool> gStartRequested{ false };
	std::atomic<bool> gNeedPump{ false };
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
	uint32_t gMinAcceptSeq = 0;
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

	void PinFrameBaselineUnlocked()
	{
		/* Caller holds gCs. Next present must be a newer seq after Start. */
		gMinAcceptSeq = 0;
		gLastPresentedSeq = 0;
		if (gShmView)
		{
			auto* hdr = reinterpret_cast<WatchProto::ShmHeader*>(gShmView);
			if (hdr->magic == WatchProto::kShmMagic)
			{
				gMinAcceptSeq = hdr->seq;
				gLastPresentedSeq = hdr->seq;
			}
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

	void RunQueuedStart(uint32_t epoch)
	{
		EnsureCs();
		/* Let a warm pass finish so we don't double-spawn. */
		for (int i = 0; i < 40 && gWarmBusy.load(); ++i)
			Sleep(50);
		if (gStartEpoch.load() != epoch)
		{
			gStartBusy = false;
			return;
		}
		SetUiStatus("Starting watchd…");
		if (!ConnectShared())
		{
			if (gStartEpoch.load() == epoch)
			{
				gCapturing = false;
				gTarget = 0;
				SetUiStatus("Could not start watchd (chmod/spawn). "
					"Need xdg-desktop-portal + PipeWire.");
			}
			gStartBusy = false;
			return;
		}
		if (gStartEpoch.load() != epoch)
		{
			gStartBusy = false;
			return;
		}
		if (!EnsureShmMapped())
		{
			if (gStartEpoch.load() == epoch)
			{
				gCapturing = false;
				gTarget = 0;
				SetUiStatus("Frame map unavailable (/dev/shm).");
			}
			gStartBusy = false;
			return;
		}

		EnterCriticalSection(&gCs);
		PinFrameBaselineUnlocked();
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

		if (gStartEpoch.load() != epoch)
		{
			if (sent)
			{
				EnterCriticalSection(&gCs);
				if (gSock != INVALID_SOCKET)
					SendCmd(gSock, WatchProto::CmdStop, nullptr, 0);
				LeaveCriticalSection(&gCs);
			}
			gCapturing = false;
			gTarget = 0;
			gStartBusy = false;
			return;
		}

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
			return;
		}

		if (gStartEpoch.load() != epoch)
		{
			EnterCriticalSection(&gCs);
			if (gSock != INVALID_SOCKET)
				SendCmd(gSock, WatchProto::CmdStop, nullptr, 0);
			LeaveCriticalSection(&gCs);
			gCapturing = false;
			gTarget = 0;
			gStartBusy = false;
			return;
		}

		gTarget = 1;
		gCapturing = true;
		SetUiStatus(status.empty()
			? "Portal picker… then capturing."
			: status.c_str());
		gStartBusy = false;
	}

	void JoinStartThread(DWORD waitMs)
	{
		if (!gStartThread)
			return;
		const DWORD r = WaitForSingleObject(gStartThread, waitMs);
		if (r == WAIT_TIMEOUT)
			return; /* still running — do not CloseHandle */
		CloseHandle(gStartThread);
		gStartThread = nullptr;
		gStartBusy = false;
	}

	void PollJoinStartThread()
	{
		JoinStartThread(0);
	}

	DWORD WINAPI StartWorker(LPVOID param)
	{
		const uint32_t epoch = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(param));
		RunQueuedStart(epoch);
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
	gStartEpoch.fetch_add(1u);
	gStartRequested.store(false);
	gQueuedStartEpoch.store(0);
	gNeedPump.store(false);
	gCapturing = false;
	gTarget = 0;
	gStartBusy = false;
	StopPump();
	JoinStartThread(1500);
	if (gStartThread)
	{
		WaitForSingleObject(gStartThread, 2000);
		CloseHandle(gStartThread);
		gStartThread = nullptr;
		gStartBusy = false;
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
	EnsureCs();
	EnterCriticalSection(&gCs);
	gPresentSlot = WatchProto::kNoSlot;
	UnmapShmUnlocked();
	LeaveCriticalSection(&gCs);
	gLastPresentedSeq = 0;
	gMinAcceptSeq = 0xFFFFFFFFu; /* reject until next Start pins baseline */
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
	JoinStartThread(0);
	if (gStartThread)
	{
		status = "Starting…";
		SetUiStatus("Starting…");
		gTarget = 1;
		gCapturing = true;
		return true;
	}

	/* Never CreateThread on Soft Start frame. If pump is missing, ask Tick to
	   create it after reopen cooldown — then Soft Start re-queues. */
	if (!gPumpRun.load())
	{
		gNeedPump.store(true);
		/* Do not set gCapturing yet — that blocked SoftOpenBlocked / pump create. */
		status = "Starting…";
		SetUiStatus("Starting…");
		return true;
	}

	if (gCapturing.load() && gTarget != 0 && !gStartBusy.load() && !gStartRequested.load())
	{
		/* Recover stuck "Starting…" after needPump: pump is up, post CmdStart. */
		bool expected = false;
		if (gStartBusy.compare_exchange_strong(expected, true))
		{
			gQueuedStartEpoch.store(gStartEpoch.load());
			gStartRequested.store(true);
			SetUiStatus("Starting watchd…");
		}
		status = "Starting…";
		CopyUiStatus(status);
		return true;
	}

	bool expected = false;
	if (!gStartBusy.compare_exchange_strong(expected, true))
	{
		status = "Starting…";
		SetUiStatus("Starting…");
		gTarget = 1;
		gCapturing = true;
		return true;
	}

	const uint32_t epoch = gStartEpoch.load();
	gTarget = 1;
	gCapturing = true;
	SetUiStatus("Starting watchd…");
	CopyUiStatus(status);
	gQueuedStartEpoch.store(epoch);
	gStartRequested.store(true);
	return true;
}

void WatchLinux::Stop(std::string& status)
{
	/* Invalidate in-flight Start / queued Start so Soft-stop cannot revive. */
	gStartEpoch.fetch_add(1u);
	gStartRequested.store(false);
	gQueuedStartEpoch.store(0);
	gNeedPump.store(false);
	gCapturing = false;
	gTarget = 0;
	EndPresent();
	EnsureCs();
	EnterCriticalSection(&gCs);
	if (gSock != INVALID_SOCKET)
	{
		SendCmd(gSock, WatchProto::CmdStop, nullptr, 0);
		/* Drop the socket so Soft Start reconnects from the pump thread — stale
		   portal sessions after Soft-stop tipped Wine on reopen. */
		closesocket(gSock);
		gSock = INVALID_SOCKET;
	}
	PinFrameBaselineUnlocked();
	gMinAcceptSeq = 0xFFFFFFFFu;
	LeaveCriticalSection(&gCs);
	gStartBusy = false;
	JoinStartThread(0);
	status = "Stopped.";
	SetUiStatus("Stopped.");
}

bool WatchLinux::IsCapturing()
{
	return gCapturing.load() && gTarget != 0;
}

bool WatchLinux::IsStarting()
{
	return gStartBusy.load() || gWarmBusy.load() || gStartRequested.load() || gNeedPump.load();
}

bool WatchLinux::PumpRunning()
{
	return gPumpRun.load();
}

bool WatchLinux::ConsumeNeedPump()
{
	return gNeedPump.exchange(false);
}

void WatchLinux::EnsurePumpNow()
{
	EnsurePump();
}

void WatchLinux::PollJoinStart()
{
	PollJoinStartThread();
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
	if (!gCapturing.load())
		return false;

	EnsureCs();
	EnterCriticalSection(&gCs);
	if (!EnsureShmMappedUnlocked() || !gShmView)
	{
		LeaveCriticalSection(&gCs);
		return false;
	}

	auto* hdr = reinterpret_cast<WatchProto::ShmHeader*>(gShmView);
	if (hdr->magic != WatchProto::kShmMagic || hdr->capturing == 0 ||
		hdr->seq == 0 || hdr->seq <= gMinAcceptSeq || hdr->seq == gLastPresentedSeq)
	{
		LeaveCriticalSection(&gCs);
		return false;
	}

	const uint32_t slot = hdr->slot & 1u;
	const uint32_t fw = hdr->w;
	const uint32_t fh = hdr->h;
	const uint32_t fs = hdr->stride;
	if (fw == 0 || fh == 0 || fs < fw * 4 || fw > WatchProto::kMaxW || fh > WatchProto::kMaxH ||
		static_cast<size_t>(fs) * fh > WatchProto::kSlotBytes)
	{
		LeaveCriticalSection(&gCs);
		return false;
	}

	hdr->reading = slot;
	__sync_synchronize();
	gPresentSlot = slot;
	gLastPresentedSeq = hdr->seq;
	bgra = WatchProto::ShmSlotPixels(gShmView, slot);
	w = fw;
	h = fh;
	stride = fs;
	LeaveCriticalSection(&gCs);

	status = "Capturing (OOP watchd).";
	SetUiStatus(status.c_str());
	return true;
}

void WatchLinux::EndPresent()
{
	EnsureCs();
	EnterCriticalSection(&gCs);
	if (gShmView)
	{
		auto* hdr = reinterpret_cast<WatchProto::ShmHeader*>(gShmView);
		hdr->reading = WatchProto::kNoSlot;
		__sync_synchronize();
	}
	gPresentSlot = WatchProto::kNoSlot;
	LeaveCriticalSection(&gCs);
}
