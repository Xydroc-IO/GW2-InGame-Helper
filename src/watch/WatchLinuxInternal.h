#pragma once

#include "WatchCapture.h"
#include "WatchProto.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <winsock2.h>
#include <windows.h>

/* Shared state/helpers for WatchLinux.cpp / WatchLinuxDaemon.cpp / WatchLinuxShm.cpp. */
namespace WatchLinuxDetail
{
	extern bool     gWsa;
	extern SOCKET   gSock;
	extern uint64_t gTarget;
	extern std::atomic<bool> gCapturing;
	extern std::atomic<bool> gStartBusy; /* ConnectShared + portal CmdStart off game thread */
	extern std::atomic<bool> gWarmBusy;
	extern std::atomic<bool> gPumpRun;
	extern HANDLE   gPumpThread;
	extern HANDLE   gStartThread;
	extern HANDLE   gWarmThread;
	extern CRITICAL_SECTION gCs;
	extern bool     gCsInit;
	extern std::string gUiStatus; /* latest status for UI (gCs) */

	extern HANDLE   gShmFile;
	extern HANDLE   gShmMap;
	extern uint8_t* gShmView;
	extern uint32_t gProtoVer;
	extern uint32_t gPresentSlot;
	extern uint32_t gLastPresentedSeq;
	extern std::string gPumpStatus;

	void EnsureCs();
	bool EnsureWsa();
	bool SendAll(SOCKET s, const void* data, int n);
	bool RecvAll(SOCKET s, void* data, int n);
	bool SendCmd(SOCKET s, uint32_t type, const void* payload, uint32_t nbytes);
	bool RecvMsg(SOCKET s, uint32_t& type, std::vector<uint8_t>& body);
	std::string WinToUnixPath(const std::wstring& win);

	bool ExtractWatchd();
	void KillOldDaemon();
	bool TryConnectSock(SOCKET& out);
	bool SpawnDaemon();
	bool ConnectShared(); /* may Sleep — call from worker threads only */
	void SetUiStatus(const char* s);
	void CopyUiStatus(std::string& out);

	void UnmapShm();
	void UnmapShmUnlocked();
	bool EnsureShmMapped();
	bool EnsureShmMappedUnlocked(); /* caller holds gCs */
	void EnsurePump();
	void StopPump();
}
