#include "WikiBrowser.h"

#include "WikiBrowserShared.h"

#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiIpc.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include <windows.h>

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
	if (!Sites::SetActiveById("browse"))
		Sites::SetActiveById("home");
	std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
	Settings::SetDirty();
	Navigate(Sites::IndexOfId("browse") >= 0 ? "about:browse-hub" : "about:helper-home");
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

