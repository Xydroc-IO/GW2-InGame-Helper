#pragma once

/* Internal shared state for WikiBrowser / Helper / Ipc / Present (not public API). */

#include "WikiIpc.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <d3d11.h>
#include <windows.h>

namespace WikiBrowserDetail
{
	constexpr int kPendingCmdMax = 16;
	constexpr uint32_t kMaxCopyRowsPerFrame = 180;
	constexpr uint32_t kMaxCopyRowsInteractive = 360;

	struct PendingCmd
	{
		WikiIpcCmd cmd;
		int32_t a;
		char arg[1536];
	};

	/* Process / IPC */
	extern HANDLE gMap;
	extern HANDLE gFrameMap;
	extern HANDLE gWakeEvent;
	extern WikiIpcState* gIpc;
	extern uint8_t* gFramePixels;
	extern HANDLE gProcess;
	extern HANDLE gJob;
	extern DWORD gProcessId;
	extern DWORD gHostPid;
	extern char gIpcName[96];
	extern char gFrameName[96];
	extern char gWakeName[96];

	/* Launch / visibility */
	extern std::mutex gMutex;
	extern std::string gStatus;
	extern std::string gPendingNavigate;
	extern std::atomic<bool> gWantVisible;
	extern std::atomic<bool> gLaunchDisabled;
	extern std::atomic<bool> gStarting;
	extern std::atomic<bool> gLaunchRequested;
	extern std::atomic<bool> gLaunchWorkerBusy;
	extern std::atomic<bool> gShuttingDown;
	extern HANDLE gLaunchThread;
	extern std::atomic<bool> gRelaunchAfterQuit;
	extern std::atomic<bool> gQuitPending;
	extern DWORD gQuitStartedMs;
	extern DWORD gLastStartAttemptMs;
	extern DWORD gHelperSpawnMs;
	extern int gQuickDeathCount;

	/* Status / caches / deferred cmds */
	extern char gStatusCache[256];
	extern bool gStatusCacheFromIpc;
	extern PendingCmd gPendingCmds[kPendingCmdMax];
	extern int gPendingCmdCount;
	extern char gUrlCache[2048];
	extern uint32_t gUrlCacheSeq;
	extern char gTitleCache[128];
	extern uint32_t gTitleCacheSeq;
	extern uint32_t gLastOpenExtSeq;
	extern uint32_t gLastOpenTabSeq;

	/* D3D / present */
	extern bool gTexHasContent;
	extern uint32_t gPartialCopySeq;
	extern uint32_t gPartialCopyFront;
	extern uint32_t gPartialCopyY;
	extern std::vector<uint8_t> gStagingFrame;
	extern uint32_t gStagingSeq;
	extern uint32_t gStagingW;
	extern uint32_t gStagingH;
	extern bool gStagingReady;
	extern ID3D11Device* gDevice;
	extern ID3D11DeviceContext* gContext;
	extern ID3D11Texture2D* gTex;
	extern ID3D11Texture2D* gStagingTex;
	extern ID3D11ShaderResourceView* gSrv;
	extern uint32_t gTexW;
	extern uint32_t gTexH;
	extern uint32_t gContentW;
	extern uint32_t gContentH;
	extern uint32_t gLastFrameSeq;
	extern DWORD gLastPresentMs;
	extern DWORD gLastMouseWakeMs;
	extern DWORD gLastUserInputMs;
	extern DWORD gLastWheelMs;
	extern HRESULT gLastMapHr;
	extern uint32_t gMapFailCount;
	extern HRESULT gLastTexHr;
	extern DWORD gLastPaintKickMs;
	extern char gPaintWaitReason[192];

	/* WikiBrowserIpc.cpp */
	void WakeHelper();
	void RefreshUrlCache();
	void RefreshTitleCache();
	std::wstring Utf8ToWide(const std::string& utf8);
	void SetLocalStatus(const std::string& s);
	std::wstring AddonDir();
	std::wstring CefDir();
	std::wstring HelperPath();
	void QueuePendingCmd(WikiIpcCmd cmd, const char* arg, int32_t a);
	bool TryPostCmdImmediate(WikiIpcCmd cmd, const char* arg, int32_t a);
	void FlushPendingCmds();
	void PostCmd(WikiIpcCmd cmd, const char* arg = "", int32_t a = 0);
	std::string NormalizeBuiltinNavigateUrl(const std::string& url);
	std::string ResolveNavigateUrl(const std::string& url);
	void QueueNavigate(const std::string& url);
	void FlushPendingNavigate();
	void PushInput(const WikiInputEvent& ev);

	/* WikiBrowserPresent.cpp */
	void ReleaseGpu();
	void ReleaseDevice();
	bool EnsureDevice();
	bool EnsureTexture(uint32_t w, uint32_t h);

	/* WikiBrowserHelper.cpp */
	void CleanupStaleAddonRootFiles();
	void MigrateLegacyAddonDataLayout();
	void KillHelperByPid(DWORD pid);
	void ResetIpcQueues();
	bool ExtractHelper();
	void EnsureIpcNames();
	bool EnsureIpc();
	bool HelperAlive();
	void NoteHelperDied();
	void NoteHelperSpawned();
	void PostQuitCmd();
	void FinishStopHelper(bool terminateIfAlive);
	void RequestStopHelper();
	void DrainOpenExtRequests();
	void DrainOpenTabRequests();
	void TickQuitPending();
	void StopHelper();
	HANDLE EnsureJob();
	bool StartHelper();
	void RequestStartHelper();
	void TickLaunchPending();
}
