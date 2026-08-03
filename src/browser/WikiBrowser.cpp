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

std::string WikiBrowser::UrlEncode(const std::string& value)
{
	std::string out;
	out.reserve(value.size() * 3);
	for (unsigned char c : value)
	{
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~')
			out.push_back(static_cast<char>(c));
		else if (c == ' ')
			out.push_back('+');
		else
		{
			char buf[8];
			std::snprintf(buf, sizeof(buf), "%%%02X", c);
			out += buf;
		}
	}
	return out;
}

void WikiBrowser::Init()
{
	/* Don't scan/kill processes at load — that can hitch startup. */
	CleanupStaleAddonRootFiles();
	/* Ensure addon data folder exists before first open (CEF + helper land here). */
	(void)AddonPaths::DataDir();
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


void WikiBrowser::FeedMouseMove(int x, int y, bool leave, unsigned mods)
{
	if (!gIpc || !HelperAlive())
		return;
	if (gIpc->mouse_x == x && gIpc->mouse_y == y &&
		gIpc->mouse_mods == mods && gIpc->mouse_leave == (leave ? 1u : 0u))
		return;
	gIpc->mouse_x = x;
	gIpc->mouse_y = y;
	gIpc->mouse_mods = mods;
	gIpc->mouse_leave = leave ? 1u : 0u;
	++gIpc->mouse_seq;
	gLastUserInputMs = GetTickCount();
	/* Always update live mouse fields; wake helper at most ~30 Hz. */
	const DWORD now = gLastUserInputMs;
	if (leave || gLastMouseWakeMs == 0 || (now - gLastMouseWakeMs) >= 33u)
	{
		gLastMouseWakeMs = now;
		WakeHelper();
	}
}

void WikiBrowser::FeedMouseClick(int x, int y, int button, bool up, int clicks, unsigned mods)
{
	gLastUserInputMs = GetTickCount();
	WikiInputEvent ev{};
	ev.type = WIKI_IN_MOUSE_CLICK;
	ev.x = x;
	ev.y = y;
	ev.a = button;
	ev.b = up ? 1 : 0;
	ev.c = clicks > 0 ? clicks : 1;
	ev.character = mods;
	PushInput(ev);
	/* Keep live mouse in sync. */
	FeedMouseMove(x, y, false, mods);
}

void WikiBrowser::FeedMouseWheel(int x, int y, int dx, int dy, unsigned mods)
{
	gLastUserInputMs = GetTickCount();
	gLastWheelMs = gLastUserInputMs;
	WikiInputEvent ev{};
	ev.type = WIKI_IN_MOUSE_WHEEL;
	ev.x = x;
	ev.y = y;
	ev.a = dx;
	ev.b = dy;
	ev.character = mods;
	PushInput(ev);
}

void WikiBrowser::FeedKey(int cefKeyType, int windowsVk, unsigned mods, unsigned character,
	int nativeKeyCode, bool systemKey)
{
	gLastUserInputMs = GetTickCount();
	WikiInputEvent ev{};
	ev.type = WIKI_IN_KEY;
	ev.a = cefKeyType;
	ev.b = windowsVk;
	ev.c = static_cast<int32_t>(mods);
	ev.character = character;
	/* x = Win32 lParam (scan/repeat); y = is_system_key for CEF. */
	ev.x = nativeKeyCode;
	ev.y = systemKey ? 1 : 0;
	PushInput(ev);
}

void WikiBrowser::FeedFocus(bool focused)
{
	WikiInputEvent ev{};
	ev.type = WIKI_IN_FOCUS;
	ev.a = focused ? 1 : 0;
	PushInput(ev);
}

void WikiBrowser::Navigate(const std::string& url)
{
	if (url.empty())
		return;
	gWantVisible.store(true);
	if (!EnsureIpc())
		return;
	if (!HelperAlive())
	{
		RequestStartHelper();
		if (gLaunchDisabled.load())
			return;
	}
	QueueNavigate(url);
	FlushPendingNavigate();
}

void WikiBrowser::NavigateHome()
{
	Sites::SetActiveById("home");
	std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
	Settings::SetDirty();
	Navigate("about:helper-home");
}

void WikiBrowser::NavigateActiveSite()
{
	Navigate(Sites::ResolveUrl(Sites::Active()));
}

void WikiBrowser::Search(const std::string& query)
{
	const std::string url = Sites::SearchUrl(query);
	if (url.empty() || url == "about:helper-home")
		NavigateHome();
	else
		Navigate(url);
}

void WikiBrowser::GoBack() { PostCmd(WIKI_CMD_BACK); }
void WikiBrowser::GoForward() { PostCmd(WIKI_CMD_FORWARD); }
void WikiBrowser::Reload() { PostCmd(WIKI_CMD_RELOAD); }

void WikiBrowser::CreateTab(int slot, const char* url)
{
	if (slot < 0 || slot >= kWikiMaxTabs)
		return;
	const std::string resolved = ResolveNavigateUrl(url ? url : "about:blank");
	if (resolved.empty())
		return;
	PostCmd(WIKI_CMD_CREATE_TAB, resolved.c_str(), slot);
}

void WikiBrowser::ActivateTab(int slot)
{
	if (slot < 0 || slot >= kWikiMaxTabs)
		return;
	PostCmd(WIKI_CMD_ACTIVATE_TAB, "", slot);
}

void WikiBrowser::CloseTab(int slot)
{
	if (slot < 0 || slot >= kWikiMaxTabs)
		return;
	PostCmd(WIKI_CMD_CLOSE_TAB, "", slot);
}

void WikiBrowser::Find(const char* text, bool forward, bool matchCase, bool findNext)
{
	if (!text || !text[0])
		return;
	int32_t flags = 0;
	if (forward) flags |= 1;
	if (matchCase) flags |= 2;
	if (findNext) flags |= 4;
	PostCmd(WIKI_CMD_FIND, text, flags);
}

void WikiBrowser::StopFind(bool clearSelection)
{
	PostCmd(WIKI_CMD_STOP_FIND, "", clearSelection ? 1 : 0);
}

bool WikiBrowser::IsReady()
{
	return gIpc && gIpc->ready && HelperAlive();
}

bool WikiBrowser::HasTab(int slot)
{
	if (!gIpc || !HelperAlive() || slot < 0 || slot >= kWikiMaxTabs)
		return false;
	return (gIpc->tab_mask & (1u << slot)) != 0;
}

int WikiBrowser::ActiveTabSlot()
{
	if (!gIpc || !HelperAlive())
		return -1;
	const int slot = gIpc->active_tab;
	if (slot < 0 || slot >= kWikiMaxTabs)
		return -1;
	return slot;
}

bool WikiBrowser::CanGoBack() { return gIpc && gIpc->can_back; }
bool WikiBrowser::CanGoForward() { return gIpc && gIpc->can_forward; }

const char* WikiBrowser::CurrentUrlCStr()
{
	RefreshUrlCache();
	return gUrlCache;
}

const char* WikiBrowser::CurrentTitleCStr()
{
	RefreshTitleCache();
	return gTitleCache;
}

std::string WikiBrowser::CurrentUrl()
{
	RefreshUrlCache();
	return gUrlCache[0] ? std::string(gUrlCache) : std::string{};
}

std::string WikiBrowser::CurrentTitle()
{
	RefreshTitleCache();
	return gTitleCache[0] ? std::string(gTitleCache) : std::string{};
}

std::string WikiBrowser::Status()
{
	return StatusCStr();
}

const char* WikiBrowser::StatusCStr()
{
	if (gIpc && HelperAlive() && gIpc->status[0])
	{
		/* strcmp vs cache — no FNV walk every frame. */
		if (!gStatusCacheFromIpc || std::strcmp(gIpc->status, gStatusCache) != 0)
		{
			std::snprintf(gStatusCache, sizeof(gStatusCache), "%s", gIpc->status);
			gStatusCacheFromIpc = true;
		}
		return gStatusCache;
	}
	std::lock_guard<std::mutex> lock(gMutex);
	if (gStatusCacheFromIpc || std::strcmp(gStatusCache, gStatus.c_str()) != 0)
	{
		std::snprintf(gStatusCache, sizeof(gStatusCache), "%s", gStatus.c_str());
		gStatusCacheFromIpc = false;
	}
	return gStatusCache;
}

uint32_t WikiBrowser::FindCount()
{
	return gIpc ? gIpc->find_count : 0;
}

uint32_t WikiBrowser::FindOrdinal()
{
	return gIpc ? gIpc->find_ordinal : 0;
}
