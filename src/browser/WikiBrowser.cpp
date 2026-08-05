#include "WikiBrowser.h"

#include "WikiBrowserShared.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "LivePanels.h"
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
#include <windows.h>

namespace WikiBrowserDetail
{
	HANDLE gMap = nullptr;
	HANDLE gFrameMap = nullptr;
	HANDLE gWakeEvent = nullptr;
	WikiIpcState* gIpc = nullptr;
	uint8_t* gFramePixels = nullptr;
	HANDLE gProcess = nullptr;
	HANDLE gJob = nullptr;
	DWORD gProcessId = 0;
	DWORD gHostPid = 0; /* GW2 PID — scopes IPC names for multi-client */
	char gIpcName[96]{};
	char gFrameName[96]{};
	char gWakeName[96]{};
	std::mutex gMutex;
	std::string gStatus = "Closed — press Ctrl+Shift+H to open";
	std::string gPendingNavigate;
	std::atomic<bool> gWantVisible{false};
	std::atomic<bool> gLaunchDisabled{false}; /* set if helper launch fails hard */
	std::atomic<bool> gStarting{false};       /* StartHelper in progress (avoid re-entry) */
	std::atomic<bool> gLaunchRequested{false}; /* CreateProcess deferred off RT_Render */
	std::atomic<bool> gLaunchWorkerBusy{false}; /* worker thread running StartHelper */
	std::atomic<bool> gShuttingDown{false};   /* unload started — no new launches */
	HANDLE gLaunchThread = nullptr;           /* joined on Shutdown (DLL unload safety) */
	std::atomic<bool> gRelaunchAfterQuit{false}; /* open again after graceful quit finishes */
	std::atomic<bool> gQuitPending{false};    /* QUIT posted — finish across frames */
	DWORD gQuitStartedMs = 0;
	DWORD gLastStartAttemptMs = 0;
	DWORD gHelperSpawnMs = 0;                 /* when CreateProcess last succeeded */
	int gQuickDeathCount = 0;                 /* helper died soon after spawn — lockup guard */
	bool gTexHasContent = false; /* false → next present must full-upload */
	/* Full-frame GPU copy can be split across frames (~1.4 MB / tick). */
	uint32_t gPartialCopySeq = 0;
	uint32_t gPartialCopyFront = 0;
	uint32_t gPartialCopyY = 0;
	/* CPU staging for split full uploads — release shared-mem pin before GPU Map. */
	std::vector<uint8_t> gStagingFrame;
	uint32_t gStagingSeq = 0;
	uint32_t gStagingW = 0;
	uint32_t gStagingH = 0;
	bool gStagingReady = false;

	ID3D11Device* gDevice = nullptr;
	ID3D11DeviceContext* gContext = nullptr;
	/* Display texture is DEFAULT (never Mapped). CPU writes go to STAGING then Copy. */
	ID3D11Texture2D* gTex = nullptr;
	ID3D11Texture2D* gStagingTex = nullptr;
	ID3D11ShaderResourceView* gSrv = nullptr;
	uint32_t gTexW = 0;
	uint32_t gTexH = 0;
	uint32_t gContentW = 0; /* last uploaded CEF frame size (may be < gTexW/H) */
	uint32_t gContentH = 0;
	uint32_t gLastFrameSeq = 0;
	DWORD gLastPresentMs = 0;
	DWORD gLastMouseWakeMs = 0;
	DWORD gLastUserInputMs = 0; /* PresentFrame: high-rate while interacting */
	DWORD gLastWheelMs = 0;     /* wheel keeps present path in “smooth scroll” mode */
	HRESULT gLastMapHr = S_OK;
	uint32_t gMapFailCount = 0;
	HRESULT gLastTexHr = S_OK;
	DWORD gLastPaintKickMs = 0;
	char gPaintWaitReason[192]{};
	uint32_t gLastOpenExtSeq = 0;
	uint32_t gLastOpenTabSeq = 0;

	/* Local status mirror for StatusCStr (avoids std::string every frame). */
	char gStatusCache[256] = "Closed — press Ctrl+Shift+H to open";
	bool gStatusCacheFromIpc = false;

	/* Tab lifecycle cmds dropped when the ring is full — retry next frame. */
	PendingCmd gPendingCmds[kPendingCmdMax]{};
	int gPendingCmdCount = 0;

	/* Fenced IPC string caches — refreshed only when *_seq changes. */
	char gUrlCache[2048]{};
	uint32_t gUrlCacheSeq = 0xFFFFFFFFu;
	char gTitleCache[128]{};
	uint32_t gTitleCacheSeq = 0xFFFFFFFFu;
}

using namespace WikiBrowserDetail;

void WikiBrowser::Init()
{
	/* Don't scan/kill processes at load — that can hitch startup. */
	CleanupStaleAddonRootFiles();
	/* Ensure addon data folder exists before first open (CEF + helper land here). */
	(void)AddonPaths::DataDir();
	MigrateLegacyAddonDataLayout();
	gLaunchDisabled.store(false);
	gStarting.store(false);
	gLaunchRequested.store(false);
	gLaunchWorkerBusy.store(false);
	gShuttingDown.store(false);
	gRelaunchAfterQuit.store(false);
	gQuitPending.store(false);
	gHelperSpawnMs = 0;
	gQuickDeathCount = 0;
	EnsureIpcNames();
	/* Kick URL-match index build (chunked in UI_Render via TickWarmUrlKeys). */
	Sites::WarmUrlKeys();
	SetLocalStatus("Closed — press Ctrl+Shift+H to open");
}

void WikiBrowser::Tick()
{
	TickQuitPending();
	TickLaunchPending();
	DrainOpenExtRequests();
	DrainOpenTabRequests();
	LivePanels::Tick();
}

void WikiBrowser::Shutdown()
{
	gWantVisible.store(false);
	LivePanels::Shutdown();
	/* Stop new launches, then join the worker BEFORE freeing shared state.
	   Unloading the DLL under a live worker hung the game on exit. */
	gShuttingDown.store(true);
	gLaunchRequested.store(false);
	if (gLaunchThread)
	{
		/* Bounded: CreateProcess/extract on Wine can be slow, but never block
		   game exit forever — the job object kills any helper we lose. */
		WaitForSingleObject(gLaunchThread, 3000);
		CloseHandle(gLaunchThread);
		gLaunchThread = nullptr;
	}
	StopHelper();
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
	if (gWakeEvent)
	{
		CloseHandle(gWakeEvent);
		gWakeEvent = nullptr;
	}
}

void WikiBrowser::SetVisible(bool visible, bool keepProcessAlive)
{
	if (!visible)
	{
		const bool was = gWantVisible.exchange(false);
		/* Full close drops a queued URL. Occlusion (collapse/tiny/off-screen) must
		   not — a resize flicker would cancel an in-flight Navigate. */
		if (!keepProcessAlive)
			gPendingNavigate.clear();
		/* Already hidden — do not PostCmd/Wake every RT_Render (KeepHelperWarm spam). */
		if (!was)
		{
			if (!keepProcessAlive && !G::KeepHelperWarm && HelperAlive() && !gQuitPending.load())
				RequestStopHelper();
			return;
		}
		if (HelperAlive())
			PostCmd(WIKI_CMD_SET_VISIBLE, "0");
		/* Collapse / tiny / off-screen: was_hidden only — killing CEF here caused
		   hitch-on-expand and was the old reason collapse skipped SetVisible. */
		if (keepProcessAlive || G::KeepHelperWarm)
		{
			if (HelperAlive() && !gQuitPending.load())
				SetLocalStatus(keepProcessAlive ? "Paused (not viewable)" : "Ready");
		}
		else
			RequestStopHelper();
		return;
	}

	/* Re-open during graceful quit — never TerminateProcess here; Tick finishes quit. */
	if (gQuitPending.load())
	{
		gWantVisible.store(true);
		gRelaunchAfterQuit.store(true);
		SetLocalStatus("Restarting browser…");
		return;
	}

	const bool wasWanted = gWantVisible.exchange(true);
	if (!EnsureIpc())
		return;
	const bool alreadyAlive = HelperAlive();
	if (!alreadyAlive)
	{
		/* Defer CreateProcess/extract to a worker via Tick — keep RT_Render short. */
		RequestStartHelper();
		if (gLaunchDisabled.load())
			return;
	}
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
	/* Always publish size for GetViewRect; throttle SET_BOUNDS wake during drag. */
	gIpc->view_w = w;
	gIpc->view_h = h;
	static DWORD sLastBoundsCmdMs = 0;
	const DWORD now = GetTickCount();
	if (gPendingNavigate.empty() &&
		(sLastBoundsCmdMs == 0 || (now - sLastBoundsCmdMs) >= 100u))
	{
		sLastBoundsCmdMs = now;
		PostCmd(WIKI_CMD_SET_BOUNDS);
	}
}


