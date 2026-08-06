/* CEF tab create/activate/close + lifespan callbacks — HelperDetail. */
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "WikiIpc.h"
#include "HelperInternal.h"

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
{
	cef_browser_host_t* Host()
	{
		cef_browser_t* browser = ActiveBrowser();
		return browser ? browser->get_host(browser) : nullptr;
	}

	void NotifyWasResized()
	{
		for (int i = 0; i < kWikiMaxTabs; ++i)
		{
			if (!gBrowsers[i])
				continue;
			cef_browser_host_t* host = gBrowsers[i]->get_host(gBrowsers[i]);
			if (!host)
				continue;
			host->was_resized(host);
			host->invalidate(host, PET_VIEW);
			host->base.release(&host->base);
		}
	}

	void ActivateSlot(int slot)
	{
		if (slot < 0 || slot >= kWikiMaxTabs)
			return;

		/* New-tab race: ACTIVATE often arrives before OnAfterCreated. Remember
		   the intent and finish activate when the browser appears. Do NOT
		   publish active_tab yet — CurrentUrl/Title still belong to the old
		   browser; UI Tick would corrupt the new tab's metadata. */
		if (!gBrowsers[slot])
		{
			gPendingActivateSlot = slot;
			return;
		}

		gPendingActivateSlot = -1;

		if (slot != gActiveSlot && gActiveSlot >= 0 && gActiveSlot < kWikiMaxTabs && gBrowsers[gActiveSlot])
		{
			if (cef_browser_host_t* oldHost = gBrowsers[gActiveSlot]->get_host(gBrowsers[gActiveSlot]))
			{
				oldHost->was_hidden(oldHost, 1);
				oldHost->base.release(&oldHost->base);
			}
		}

		gActiveSlot = slot;
		if (gIpc)
			gIpc->active_tab = slot;

		if (cef_browser_host_t* host = gBrowsers[slot]->get_host(gBrowsers[slot]))
		{
			host->was_hidden(host, 0);
			host->invalidate(host, PET_VIEW);
			host->set_focus(host, 1);
			host->base.release(&host->base);
		}

		if (gIpc)
			gIpc->ready = 1;
		UpdateUrlFromBrowser();
		RefreshNavFlags();
	}

	void AdjustCreateQueueForClose(int closedSlot)
	{
		if (gCreateInFlightSlot == closedSlot)
			gCreateInFlightDiscard = true;
		else if (gCreateInFlightSlot > closedSlot)
			--gCreateInFlightSlot;

		if (gPendingActivateSlot == closedSlot)
			gPendingActivateSlot = -1;
		else if (gPendingActivateSlot > closedSlot)
			--gPendingActivateSlot;

		if (gCreateQueueCount <= 0)
			return;

		int newCount = 0;
		const int oldHead = gCreateQueueHead;
		const int oldCount = gCreateQueueCount;
		std::string urls[kWikiMaxTabs];
		int slots[kWikiMaxTabs];
		for (int i = 0; i < oldCount; ++i)
		{
			const int idx = (oldHead + i) % kWikiMaxTabs;
			int s = gCreateQueueSlots[idx];
			if (s == closedSlot)
				continue;
			if (s > closedSlot)
				--s;
			slots[newCount] = s;
			urls[newCount] = std::move(gCreateQueueUrls[idx]);
			++newCount;
		}
		gCreateQueueHead = 0;
		gCreateQueueCount = newCount;
		for (int i = 0; i < newCount; ++i)
		{
			gCreateQueueSlots[i] = slots[i];
			gCreateQueueUrls[i] = std::move(urls[i]);
		}
	}

	bool EnqueueBrowserCreate(int slot, const char* url)
	{
		if (gCreateQueueCount >= kWikiMaxTabs)
		{
			SetStatus("too many pending browser creates");
			return false;
		}
		const int tail = (gCreateQueueHead + gCreateQueueCount) % kWikiMaxTabs;
		gCreateQueueSlots[tail] = slot;
		gCreateQueueUrls[tail] = url ? url : "about:blank";
		++gCreateQueueCount;
		return StartNextBrowserCreate();
	}

	bool StartNextBrowserCreate()
	{
		if (gCreateInFlightSlot >= 0 || gCreateQueueCount <= 0)
			return true;

		const int slot = gCreateQueueSlots[gCreateQueueHead];
		const std::string url = std::move(gCreateQueueUrls[gCreateQueueHead]);
		gCreateQueueUrls[gCreateQueueHead].clear();
		gCreateQueueHead = (gCreateQueueHead + 1) % kWikiMaxTabs;
		--gCreateQueueCount;

		if (slot < 0 || slot >= kWikiMaxTabs)
			return StartNextBrowserCreate();
		if (gBrowsers[slot])
		{
			NavigateSlot(slot, url.c_str());
			return StartNextBrowserCreate();
		}

		cef_window_info_t info{};
		info.size = sizeof(info);
		info.windowless_rendering_enabled = 1;
		info.shared_texture_enabled = 0;
		info.external_begin_frame_enabled = 0;
		/* CEF 150 requires size; parent helps dialogs/monitor info for OSR. */
		if (gHelperWnd)
			info.parent_window = gHelperWnd;

		cef_browser_settings_t bset{};
		bset.size = sizeof(bset);
		bset.windowless_frame_rate = 60;
		/* Match HelperThemeCss --bg so short pages never flash white/black voids. */
		bset.background_color = CefColorSetARGB(255, 14, 11, 8);

		cef_string_t u{};
		MakeCefString(&u, url.c_str());
		gCreateInFlightSlot = slot;
		gCreateInFlightDiscard = false;
		const int ok = g_create_browser(&info, &gClient, &u, &bset, nullptr, nullptr);
		ClearCefString(&u);
		if (!ok)
		{
			gCreateInFlightSlot = -1;
			SetStatus("cef_browser_host_create_browser failed");
			return StartNextBrowserCreate();
		}
		return true;
	}

	bool CreateBrowserForSlot(int slot, const char* url)
	{
		if (slot < 0 || slot >= kWikiMaxTabs)
			return false;

		const char* startRaw = (url && url[0]) ? url : "about:blank";
		const std::string startResolved = ResolveBuiltinUrl(startRaw);
		const char* start = startResolved.empty() ? "about:blank" : startResolved.c_str();
		if (gBrowsers[slot])
		{
			/* Reload this slot only — do not ActivateSlot (SyncAll would steal focus). */
			NavigateSlot(slot, start);
			return true;
		}

		return EnqueueBrowserCreate(slot, start);
	}

	void CloseSlot(int slot)
	{
		if (slot < 0 || slot >= kWikiMaxTabs)
			return;

		const int oldActive = gActiveSlot;
		AdjustCreateQueueForClose(slot);

		/* Detach before close_browser so a sync OnBeforeClose cannot clear a
		   neighbour after we compact, and so we never shift a live pointer
		   that close_browser still owns. */
		cef_browser_t* closing = gBrowsers[slot];
		gBrowsers[slot] = nullptr;

		if (closing)
		{
			if (cef_browser_host_t* host = closing->get_host(closing))
			{
				host->close_browser(host, 1);
				host->base.release(&host->base);
			}
			closing->base.release(&closing->base);
		}

		/* Always compact — UI already shifted tab indices even if this slot
		   was empty (create race). Skipping the shift desyncs CEF vs UI. */
		for (int i = slot; i < kWikiMaxTabs - 1; ++i)
			gBrowsers[i] = gBrowsers[i + 1];
		gBrowsers[kWikiMaxTabs - 1] = nullptr;

		if (oldActive == slot)
		{
			if (gBrowsers[slot])
				gActiveSlot = slot;
			else if (slot > 0 && gBrowsers[slot - 1])
				gActiveSlot = slot - 1;
			else
			{
				gActiveSlot = 0;
				for (int i = 0; i < kWikiMaxTabs; ++i)
				{
					if (gBrowsers[i])
					{
						gActiveSlot = i;
						break;
					}
				}
			}
			if (ActiveBrowser())
				ActivateSlot(gActiveSlot);
			/* Keep ready=1 with no browsers — host still sends CREATE_TAB. */
		}
		else if (oldActive > slot)
		{
			gActiveSlot = oldActive - 1;
		}

		UpdateTabMask();
		if (gIpc)
			gIpc->active_tab = gActiveSlot;
	}

	void ViewSize(int* outW, int* outH)
	{
		int w = gIpc && gIpc->view_w ? static_cast<int>(gIpc->view_w) : 800;
		int h = gIpc && gIpc->view_h ? static_cast<int>(gIpc->view_h) : 600;
		if (w < 32) w = 32;
		if (h < 32) h = 32;
		if (w > static_cast<int>(kWikiFrameMaxW)) w = static_cast<int>(kWikiFrameMaxW);
		if (h > static_cast<int>(kWikiFrameMaxH)) h = static_cast<int>(kWikiFrameMaxH);
		*outW = w;
		*outH = h;
	}

	void CEF_CALLBACK OnAfterCreated(cef_life_span_handler_t*, cef_browser_t* browser)
	{
		int slot = gCreateInFlightSlot;
		const bool discard = gCreateInFlightDiscard;
		gCreateInFlightSlot = -1;
		gCreateInFlightDiscard = false;

		if (discard || slot < 0 || slot >= kWikiMaxTabs)
		{
			/* Orphan / cancelled create — never dump into slot 0. */
			if (browser)
			{
				if (cef_browser_host_t* host = browser->get_host(browser))
				{
					host->close_browser(host, 1);
					host->base.release(&host->base);
				}
			}
			StartNextBrowserCreate();
			return;
		}

		if (gBrowsers[slot])
		{
			gBrowsers[slot]->base.release(&gBrowsers[slot]->base);
			gBrowsers[slot] = nullptr;
		}
		gBrowsers[slot] = browser;
		gBrowsers[slot]->base.add_ref(&gBrowsers[slot]->base);
		UpdateTabMask();
		/* Reinforce ready (also set at CreateOsRBrowser so CREATE_TAB can start). */
		if (gIpc)
		{
			gIpc->ready = 1;
			gIpc->alive = GetTickCount();
		}

		/* Prefer deferred ACTIVATE from CreateTab+ActivateTab (new tab). */
		if (gPendingActivateSlot == slot)
		{
			ActivateSlot(slot);
			NotifyWasResized();
			StartNextBrowserCreate();
			return;
		}

		if (slot != gActiveSlot)
		{
			if (cef_browser_host_t* host = browser->get_host(browser))
			{
				host->was_hidden(host, 1);
				host->base.release(&host->base);
			}
			StartNextBrowserCreate();
			return;
		}

		if (gIpc)
			gIpc->active_tab = slot;
		SetStatus("Ready");
		UpdateUrlFromBrowser();
		NotifyWasResized();
		if (cef_browser_host_t* host = Host())
		{
			host->set_focus(host, 1);
			host->base.release(&host->base);
		}
		StartNextBrowserCreate();
	}

	void CEF_CALLBACK OnBeforeClose(cef_life_span_handler_t*, cef_browser_t* browser)
	{
		/* CloseSlot detaches first — usually no match. Still clear if a browser
		   closed itself (e.g. discard path) without going through CloseSlot. */
		for (int i = 0; i < kWikiMaxTabs; ++i)
		{
			if (gBrowsers[i] && browser && gBrowsers[i]->is_same(gBrowsers[i], browser))
			{
				gBrowsers[i]->base.release(&gBrowsers[i]->base);
				gBrowsers[i] = nullptr;
				if (gIpc)
					gIpc->tab_mask &= ~(1u << i);
				break;
			}
		}
		/* Keep ready=1 after the last browser closes — CREATE_TAB must still be
		   accepted (same chicken-egg as startup). */
	}

}
