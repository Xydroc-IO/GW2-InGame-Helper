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

