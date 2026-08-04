/* OSR input drain + IPC command handling — HelperDetail. */
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "WikiIpc.h"
#include "HelperInternal.h"

#include "include/capi/cef_browser_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
{
	void SendMouseMove(int x, int y, int leave, uint32_t mods)
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		ApplyPopupMouseOffset(x, y);
		cef_mouse_event_t ev{};
		ev.x = x;
		ev.y = y;
		ev.modifiers = mods;
		host->send_mouse_move_event(host, &ev, leave ? 1 : 0);
		host->base.release(&host->base);
	}

	void SendMouseClick(int x, int y, int button, int up, int clicks, uint32_t mods)
	{
		/* Drop the trailing mouse-up that lands where PET_POPUP just was — it
		   hits the page underneath and looks like a dropdown-caused refresh. */
		if (up && gSwallowPopupMouseUp)
		{
			gSwallowPopupMouseUp = false;
			if (x >= gPopupSwallowRect.x && y >= gPopupSwallowRect.y &&
				x < gPopupSwallowRect.x + gPopupSwallowRect.width &&
				y < gPopupSwallowRect.y + gPopupSwallowRect.height)
				return;
		}
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		ApplyPopupMouseOffset(x, y);
		cef_mouse_event_t ev{};
		ev.x = x;
		ev.y = y;
		ev.modifiers = mods;
		cef_mouse_button_type_t btn = MBT_LEFT;
		if (button == 1) btn = MBT_MIDDLE;
		else if (button == 2) btn = MBT_RIGHT;
		host->send_mouse_click_event(host, &ev, btn, up ? 1 : 0, clicks > 0 ? clicks : 1);
		host->base.release(&host->base);
	}

	void SendMouseWheel(int x, int y, int dx, int dy, uint32_t mods)
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		ApplyPopupMouseOffset(x, y);
		cef_mouse_event_t ev{};
		ev.x = x;
		ev.y = y;
		ev.modifiers = mods;
		host->send_mouse_wheel_event(host, &ev, dx, dy);
		host->base.release(&host->base);
	}

	void SendKey(int type, int windowsVk, uint32_t mods, uint32_t character, int nativeKeyCode, int isSystemKey)
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		cef_key_event_t ev{};
		ev.size = sizeof(ev);
		ev.type = static_cast<cef_key_event_type_t>(type);
		ev.modifiers = mods;
		/* CHAR events use the character as windows_key_code (cefclient OSR). */
		if (type == KEYEVENT_CHAR)
			ev.windows_key_code = static_cast<int>(character ? character : windowsVk);
		else
			ev.windows_key_code = windowsVk;
		/* Full WM_* lParam — scan code / repeat live here; VK alone drops chars under load. */
		ev.native_key_code = nativeKeyCode ? nativeKeyCode : windowsVk;
		ev.is_system_key = isSystemKey ? 1 : 0;
		ev.character = static_cast<char16_t>(character);
		ev.unmodified_character = static_cast<char16_t>(character);
		ev.focus_on_editable_field = 1;
		host->send_key_event(host, &ev);
		host->base.release(&host->base);
	}

	void SendFocus(int focused)
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		host->set_focus(host, focused ? 1 : 0);
		host->base.release(&host->base);
	}

	uint32_t gLastMouseSeq = 0;

	void DrainInput()
	{
		if (!gIpc)
			return;

		if (gIpc->mouse_seq != gLastMouseSeq)
		{
			gLastMouseSeq = gIpc->mouse_seq;
			SendMouseMove(gIpc->mouse_x, gIpc->mouse_y, static_cast<int>(gIpc->mouse_leave), gIpc->mouse_mods);
		}

		while (gIpc->input_read != gIpc->input_write)
		{
			const WikiInputEvent ev = gIpc->input_q[gIpc->input_read % kWikiInputQueueSize];
			gIpc->input_read = (gIpc->input_read + 1u) % kWikiInputQueueSize;

			switch (ev.type)
			{
			case WIKI_IN_MOUSE_CLICK:
				SendMouseClick(ev.x, ev.y, ev.a, ev.b, ev.c, ev.character);
				break;
			case WIKI_IN_MOUSE_WHEEL:
				SendMouseWheel(ev.x, ev.y, ev.a, ev.b, ev.character);
				break;
			case WIKI_IN_KEY:
				SendKey(ev.a, ev.b, static_cast<uint32_t>(ev.c), ev.character, ev.x, ev.y);
				break;
			case WIKI_IN_FOCUS:
				SendFocus(ev.a);
				break;
			default:
				break;
			}
		}
	}

	void HandleCmd(uint32_t cmd, int32_t a, const char* arg)
	{
		cef_browser_t* browser = ActiveBrowser();
		switch (cmd)
		{
		case WIKI_CMD_NAVIGATE: NavigateTo(arg); break;
		case WIKI_CMD_BACK: if (browser) browser->go_back(browser); break;
		case WIKI_CMD_FORWARD: if (browser) browser->go_forward(browser); break;
		case WIKI_CMD_RELOAD:
			if (browser)
				browser->reload(browser);
			break;
		case WIKI_CMD_HOME: NavigateTo(arg && arg[0] ? arg : gStartUrl.c_str()); break;
		case WIKI_CMD_SET_BOUNDS: NotifyWasResized(); break;
		case WIKI_CMD_SET_VISIBLE:
		{
			const bool visible = arg && arg[0] != '0';
			if (gIpc)
				gIpc->visible = visible ? 1u : 0u;
			for (int i = 0; i < kWikiMaxTabs; ++i)
			{
				if (!gBrowsers[i])
					continue;
				if (cef_browser_host_t* host = gBrowsers[i]->get_host(gBrowsers[i]))
				{
					if (i == gActiveSlot)
						host->was_hidden(host, visible ? 0 : 1);
					else
						host->was_hidden(host, 1);
					host->base.release(&host->base);
				}
			}
			NotifyWasResized();
			break;
		}
		case WIKI_CMD_CREATE_TAB:
			CreateBrowserForSlot(a, arg);
			break;
		case WIKI_CMD_ACTIVATE_TAB:
			ActivateSlot(a);
			break;
		case WIKI_CMD_CLOSE_TAB:
			CloseSlot(a);
			break;
		case WIKI_CMD_FIND:
		{
			cef_browser_host_t* host = Host();
			if (!host || !arg)
				break;
			cef_string_t text{};
			MakeCefString(&text, arg);
			host->find(host, &text,
				(a & 1) ? 1 : 0,
				(a & 2) ? 1 : 0,
				(a & 4) ? 1 : 0);
			ClearCefString(&text);
			host->base.release(&host->base);
			break;
		}
		case WIKI_CMD_STOP_FIND:
		{
			cef_browser_host_t* host = Host();
			if (!host)
				break;
			host->stop_finding(host, a ? 1 : 0);
			host->base.release(&host->base);
			break;
		}
		case WIKI_CMD_QUIT:
			gRunning = false;
			for (int i = 0; i < kWikiMaxTabs; ++i)
			{
				if (!gBrowsers[i])
					continue;
				if (cef_browser_host_t* host = gBrowsers[i]->get_host(gBrowsers[i]))
				{
					host->close_browser(host, 1);
					host->base.release(&host->base);
				}
			}
			PostMessageW(gHelperWnd, WM_QUIT, 0, 0);
			break;
		default: break;
		}
	}

	void ProcessCommands()
	{
		if (!gIpc)
			return;
		gIpc->alive = GetTickCount();
		DrainInput();

		while (gIpc->cmd_read != gIpc->cmd_write)
		{
			const WikiCmdEvent ev = gIpc->cmd_q[gIpc->cmd_read % kWikiCmdQueueSize];
			gIpc->cmd_read = (gIpc->cmd_read + 1u) % kWikiCmdQueueSize;
			HandleCmd(ev.cmd, ev.a, ev.arg);
		}

		/* Legacy single-slot — only when the ring was full (PostCmd bumps cmd_seq).
		   Normal commands must not run twice: a second CLOSE_TAB after compact
		   destroys the tab that shifted into the same index. */
		if (gIpc->cmd_seq != gIpc->last_cmd_seq)
		{
			const uint32_t cmd = gIpc->cmd;
			char arg[sizeof(gIpc->cmd_arg)];
			std::snprintf(arg, sizeof(arg), "%s", gIpc->cmd_arg);
			const int32_t a = gIpc->cmd_a;
			gIpc->last_cmd_seq = gIpc->cmd_seq;
			gIpc->cmd = WIKI_CMD_NONE;
			gIpc->cmd_arg[0] = 0;
			HandleCmd(cmd, a, arg);
		}
	}
}
