/* GW2HelperBrowser.exe — windowless CEF helper using Guild Wars 2's
   bin64/cef/libcef.dll. Paints BGRA frames into shared memory for the addon.
   Rewrites Tailwind v4 CSS (oklch / color-mix) so Chromium 103 can render
   snowcrows.com. */

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>

#include "BootJs.h"
#include "CssCompat.h"
#include "CssProxy.h"
#include "WikiIpc.h"

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_callback_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_command_line_capi.h"
#include "include/capi/cef_display_handler_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_load_handler_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/capi/cef_request_capi.h"
#include "include/capi/cef_request_handler_capi.h"
#include "include/capi/cef_resource_request_handler_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace
{
	using Fn_cef_execute_process = int (*)(const cef_main_args_t*, cef_app_t*, void*);
	using Fn_cef_initialize = int (*)(const cef_main_args_t*, const cef_settings_t*, cef_app_t*, void*);
	using Fn_cef_shutdown = void (*)();
	using Fn_cef_do_message_loop_work = void (*)();
	using Fn_cef_browser_host_create_browser = int (*)(
		const cef_window_info_t*, cef_client_t*, const cef_string_t*,
		const cef_browser_settings_t*, cef_dictionary_value_t*, cef_request_context_t*);
	using Fn_cef_string_from_utf8 = int (*)(const char*, size_t, cef_string_utf16_t*);
	using Fn_cef_string_clear = void (*)(cef_string_t*);
	using Fn_cef_string_utf16_to_utf8 = int (*)(const char16*, size_t, cef_string_utf8_t*);
	using Fn_cef_string_utf8_clear = void (*)(cef_string_utf8_t*);
	using Fn_cef_string_userfree_free = void (*)(cef_string_userfree_t);

	HMODULE gLib = nullptr;
	Fn_cef_execute_process g_execute_process = nullptr;
	Fn_cef_initialize g_initialize = nullptr;
	Fn_cef_shutdown g_shutdown = nullptr;
	Fn_cef_do_message_loop_work g_do_message_loop_work = nullptr;
	Fn_cef_browser_host_create_browser g_create_browser = nullptr;
	Fn_cef_string_from_utf8 g_string_from_utf8 = nullptr;
	Fn_cef_string_clear g_string_clear = nullptr;
	Fn_cef_string_utf16_to_utf8 g_utf16_to_utf8 = nullptr;
	Fn_cef_string_utf8_clear g_utf8_clear = nullptr;
	Fn_cef_string_userfree_free g_userfree_free = nullptr;

	WikiIpcState* gIpc = nullptr;
	HANDLE gMap = nullptr;
	HANDLE gFrameMap = nullptr;
	uint8_t* gFramePixels = nullptr;
	HWND gHelperWnd = nullptr;
	std::wstring gCefDir;
	std::string gStartUrl = "about:blank";
	std::atomic<bool> gRunning{true};
	cef_browser_t* gBrowser = nullptr;

	cef_app_t gApp{};
	cef_client_t gClient{};
	cef_life_span_handler_t gLife{};
	cef_load_handler_t gLoad{};
	cef_display_handler_t gDisplay{};
	cef_render_handler_t gRender{};
	cef_request_handler_t gRequest{};
	cef_resource_request_handler_t gResourceRequest{};


	void SetStatus(const char* text)
	{
		if (!gIpc || !text)
			return;
		std::snprintf(gIpc->status, sizeof(gIpc->status), "%s", text);
	}

	void SetUrlUtf8(const char* text)
	{
		if (!gIpc || !text)
			return;
		std::snprintf(gIpc->url, sizeof(gIpc->url), "%s", text);
	}

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
		if (n > 0)
			WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	bool LoadCef(const std::wstring& cefDir)
	{
		const std::wstring path = cefDir + L"\\libcef.dll";
		gLib = LoadLibraryW(path.c_str());
		if (!gLib)
		{
			SetStatus("Failed to LoadLibrary libcef.dll");
			return false;
		}

		auto sym = [&](const char* name) -> FARPROC {
			return GetProcAddress(gLib, name);
		};

		g_execute_process = reinterpret_cast<Fn_cef_execute_process>(sym("cef_execute_process"));
		g_initialize = reinterpret_cast<Fn_cef_initialize>(sym("cef_initialize"));
		g_shutdown = reinterpret_cast<Fn_cef_shutdown>(sym("cef_shutdown"));
		g_do_message_loop_work = reinterpret_cast<Fn_cef_do_message_loop_work>(sym("cef_do_message_loop_work"));
		g_create_browser = reinterpret_cast<Fn_cef_browser_host_create_browser>(sym("cef_browser_host_create_browser"));
		g_string_from_utf8 = reinterpret_cast<Fn_cef_string_from_utf8>(sym("cef_string_utf8_to_utf16"));
		g_string_clear = reinterpret_cast<Fn_cef_string_clear>(sym("cef_string_utf16_clear"));
		g_utf16_to_utf8 = reinterpret_cast<Fn_cef_string_utf16_to_utf8>(sym("cef_string_utf16_to_utf8"));
		g_utf8_clear = reinterpret_cast<Fn_cef_string_utf8_clear>(sym("cef_string_utf8_clear"));
		g_userfree_free = reinterpret_cast<Fn_cef_string_userfree_free>(sym("cef_string_userfree_utf16_free"));

		if (!g_execute_process || !g_initialize || !g_shutdown || !g_do_message_loop_work ||
			!g_create_browser || !g_string_from_utf8 || !g_string_clear)
		{
			SetStatus("libcef.dll missing required exports");
			return false;
		}
		return true;
	}

	void MakeCefString(cef_string_t* out, const char* utf8)
	{
		std::memset(out, 0, sizeof(*out));
		if (utf8 && utf8[0])
			g_string_from_utf8(utf8, std::strlen(utf8), out);
	}

	void ClearCefString(cef_string_t* s)
	{
		if (s)
			g_string_clear(s);
	}

	std::string CefStringToUtf8(const cef_string_t* s)
	{
		if (!s || !s->str || s->length == 0 || !g_utf16_to_utf8)
			return {};
		cef_string_utf8_t u8{};
		g_utf16_to_utf8(s->str, s->length, &u8);
		std::string out = u8.str ? std::string(u8.str, u8.length) : std::string{};
		if (g_utf8_clear)
			g_utf8_clear(&u8);
		return out;
	}

	void CEF_CALLBACK BaseAddRef(cef_base_ref_counted_t*) {}
	int CEF_CALLBACK BaseRelease(cef_base_ref_counted_t*) { return 1; }
	int CEF_CALLBACK BaseHasOneRef(cef_base_ref_counted_t*) { return 1; }
	int CEF_CALLBACK BaseHasAtLeastOneRef(cef_base_ref_counted_t*) { return 1; }

	void InitBase(cef_base_ref_counted_t* base, size_t size)
	{
		base->size = size;
		base->add_ref = BaseAddRef;
		base->release = BaseRelease;
		base->has_one_ref = BaseHasOneRef;
		base->has_at_least_one_ref = BaseHasAtLeastOneRef;
	}

	void RefreshNavFlags()
	{
		if (!gIpc || !gBrowser)
			return;
		gIpc->can_back = gBrowser->can_go_back(gBrowser) ? 1u : 0u;
		gIpc->can_forward = gBrowser->can_go_forward(gBrowser) ? 1u : 0u;
	}

	void UpdateUrlFromBrowser()
	{
		if (!gBrowser || !g_userfree_free)
			return;
		cef_frame_t* frame = gBrowser->get_main_frame(gBrowser);
		if (!frame)
			return;
		cef_string_userfree_t uf = frame->get_url(frame);
		if (uf)
		{
			SetUrlUtf8(CefStringToUtf8(uf).c_str());
			g_userfree_free(uf);
		}
		frame->base.release(&frame->base);
		RefreshNavFlags();
	}

	void NavigateTo(const char* url)
	{
		if (!gBrowser || !url || !url[0])
			return;
		cef_frame_t* frame = gBrowser->get_main_frame(gBrowser);
		if (!frame)
			return;
		cef_string_t u{};
		MakeCefString(&u, url);
		frame->load_url(frame, &u);
		ClearCefString(&u);
		frame->base.release(&frame->base);
		SetStatus("Navigating…");
	}

	cef_browser_host_t* Host()
	{
		return gBrowser ? gBrowser->get_host(gBrowser) : nullptr;
	}

	void NotifyWasResized()
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		host->was_resized(host);
		host->invalidate(host, PET_VIEW);
		host->base.release(&host->base);
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
		if (gBrowser)
			gBrowser->base.release(&gBrowser->base);
		gBrowser = browser;
		gBrowser->base.add_ref(&gBrowser->base);
		if (gIpc)
		{
			gIpc->ready = 1;
			gIpc->alive = GetTickCount();
		}
		SetStatus("Ready");
		UpdateUrlFromBrowser();
		NotifyWasResized();
		if (cef_browser_host_t* host = Host())
		{
			host->set_focus(host, 1);
			host->base.release(&host->base);
		}
	}

	void CEF_CALLBACK OnBeforeClose(cef_life_span_handler_t*, cef_browser_t* browser)
	{
		if (gBrowser && browser && gBrowser->is_same(gBrowser, browser))
		{
			gBrowser->base.release(&gBrowser->base);
			gBrowser = nullptr;
		}
		if (gIpc)
			gIpc->ready = 0;
	}

	int CEF_CALLBACK OnBeforePopup(
		cef_life_span_handler_t*, cef_browser_t*, cef_frame_t*, const cef_string_t* target_url,
		const cef_string_t*, cef_window_open_disposition_t, int, const cef_popup_features_t*,
		cef_window_info_t*, cef_client_t**, cef_browser_settings_t*, cef_dictionary_value_t**, int*)
	{
		if (target_url)
			NavigateTo(CefStringToUtf8(target_url).c_str());
		return 1; /* cancel popup — open in same browser */
	}

	void InjectBootJs(cef_frame_t* frame)
	{
		if (!frame || !frame->is_main(frame) || !frame->execute_java_script)
			return;
		cef_string_t code{};
		MakeCefString(&code, kSnowcrowBootJs);
		frame->execute_java_script(frame, &code, nullptr, 0);
		ClearCefString(&code);
	}

	void CEF_CALLBACK OnLoadingStateChange(
		cef_load_handler_t*, cef_browser_t* browser, int isLoading, int canGoBack, int canGoForward)
	{
		if (!gBrowser || !browser || !gBrowser->is_same(gBrowser, browser))
			return;
		if (gIpc)
		{
			gIpc->can_back = canGoBack ? 1u : 0u;
			gIpc->can_forward = canGoForward ? 1u : 0u;
		}
		SetStatus(isLoading ? "Loading…" : "Ready");
		if (!isLoading)
		{
			UpdateUrlFromBrowser();
			if (cef_frame_t* frame = gBrowser->get_main_frame(gBrowser))
			{
				InjectBootJs(frame);
				frame->base.release(&frame->base);
			}
		}
	}

	void CEF_CALLBACK OnLoadError(
		cef_load_handler_t*, cef_browser_t* browser, cef_frame_t* frame,
		cef_errorcode_t errorCode, const cef_string_t* errorText, const cef_string_t*)
	{
		if (!gBrowser || !browser || !gBrowser->is_same(gBrowser, browser))
			return;
		if (frame && !frame->is_main(frame))
			return;
		/* ERR_ABORTED (-3) is normal: redirects, replaced navigations, cancelled
		   subloads. Do not surface it as a hard failure. */
		if (errorCode == ERR_ABORTED || errorCode == ERR_NONE)
			return;
		char buf[256];
		std::snprintf(buf, sizeof(buf), "Load error %d: %s",
			static_cast<int>(errorCode), CefStringToUtf8(errorText).c_str());
		SetStatus(buf);
	}

	void CEF_CALLBACK OnLoadEnd(
		cef_load_handler_t*, cef_browser_t* browser, cef_frame_t* frame, int)
	{
		if (!gBrowser || !browser || !gBrowser->is_same(gBrowser, browser))
			return;
		InjectBootJs(frame);
	}

	void CEF_CALLBACK OnAddressChange(
		cef_display_handler_t*, cef_browser_t* browser, cef_frame_t* frame, const cef_string_t* url)
	{
		if (gBrowser && browser && gBrowser->is_same(gBrowser, browser) && frame && frame->is_main(frame) && url)
			SetUrlUtf8(CefStringToUtf8(url).c_str());
	}

	void CEF_CALLBACK GetViewRect(cef_render_handler_t*, cef_browser_t*, cef_rect_t* rect)
	{
		int w = 800, h = 600;
		ViewSize(&w, &h);
		rect->x = 0;
		rect->y = 0;
		rect->width = w;
		rect->height = h;
	}

	int CEF_CALLBACK GetScreenInfo(cef_render_handler_t*, cef_browser_t*, cef_screen_info_t* info)
	{
		int w = 800, h = 600;
		ViewSize(&w, &h);
		info->device_scale_factor = 1.0f;
		info->depth = 32;
		info->depth_per_component = 8;
		info->is_monochrome = 0;
		info->rect.x = 0;
		info->rect.y = 0;
		info->rect.width = w;
		info->rect.height = h;
		info->available_rect = info->rect;
		return 1;
	}

	void CEF_CALLBACK OnPaint(
		cef_render_handler_t*, cef_browser_t*, cef_paint_element_type_t type,
		size_t, cef_rect_t const*, const void* buffer, int width, int height)
	{
		if (type != PET_VIEW || !buffer || !gFramePixels || !gIpc || width <= 0 || height <= 0)
			return;
		if (width > static_cast<int>(kWikiFrameMaxW) || height > static_cast<int>(kWikiFrameMaxH))
			return;

		const size_t rowBytes = static_cast<size_t>(width) * 4;
		const uint8_t* src = static_cast<const uint8_t*>(buffer);
		for (int y = 0; y < height; ++y)
		{
			std::memcpy(
				gFramePixels + static_cast<size_t>(y) * kWikiFrameStride,
				src + static_cast<size_t>(y) * rowBytes,
				rowBytes);
		}
		gIpc->frame_w = static_cast<uint32_t>(width);
		gIpc->frame_h = static_cast<uint32_t>(height);
		++gIpc->frame_seq;
	}

	void CEF_CALLBACK OnBeforeCommandLine(
		cef_app_t*, const cef_string_t*, cef_command_line_t* cmd)
	{
		if (!cmd || !cmd->append_switch)
			return;
		/* Software OSR — avoids fighting GW2's D3D device on Wine. */
		const char* switches[] = {
			"disable-gpu",
			"disable-gpu-compositing",
			"disable-gpu-vsync",
			"disable-d3d11",
			"disable-direct-composition",
			"no-sandbox",
			"disable-extensions",
			"disable-pdf-extension",
			"allow-file-access-from-files",
			"allow-file-access",
		};
		for (const char* sw : switches)
		{
			cef_string_t s{};
			MakeCefString(&s, sw);
			cmd->append_switch(cmd, &s);
			ClearCefString(&s);
		}
	}

	cef_return_value_t CEF_CALLBACK OnBeforeResourceLoad(
		cef_resource_request_handler_t*, cef_browser_t*, cef_frame_t*,
		cef_request_t* request, cef_callback_t*)
	{
		if (!request || !request->get_url || !g_userfree_free)
			return RV_CONTINUE;

		/* Never cancel the top-level document — that shows up as ERR_ABORTED. */
		if (request->get_resource_type &&
			request->get_resource_type(request) == RT_MAIN_FRAME)
			return RV_CONTINUE;

		cef_string_userfree_t uf = request->get_url(request);
		if (!uf)
			return RV_CONTINUE;
		const std::string url = CefStringToUtf8(uf);
		g_userfree_free(uf);
		if (ShouldBlockUrl(url))
			return RV_CANCEL;
		return RV_CONTINUE;
	}

	cef_resource_handler_t* CEF_CALLBACK GetResourceHandler(
		cef_resource_request_handler_t*, cef_browser_t*, cef_frame_t*,
		cef_request_t*)
	{
		/* Do not proxy CSS via WinHTTP — under Wine/Proton it often fails and
		   replaces working (if incomplete) sheets with empty stubs. Styles are
		   fixed in OnLoadEnd via fetch()+JS downlevel using CEF's network. */
		return nullptr;
	}

	cef_resource_request_handler_t* CEF_CALLBACK GetResourceRequestHandler(
		cef_request_handler_t*, cef_browser_t*, cef_frame_t*, cef_request_t*,
		int, int, const cef_string_t*, int*)
	{
		return &gResourceRequest;
	}

	cef_life_span_handler_t* CEF_CALLBACK GetLifeSpan(cef_client_t*) { return &gLife; }
	cef_load_handler_t* CEF_CALLBACK GetLoad(cef_client_t*) { return &gLoad; }
	cef_display_handler_t* CEF_CALLBACK GetDisplay(cef_client_t*) { return &gDisplay; }
	cef_render_handler_t* CEF_CALLBACK GetRender(cef_client_t*) { return &gRender; }
	cef_request_handler_t* CEF_CALLBACK GetRequest(cef_client_t*) { return &gRequest; }

	void InitHandlers()
	{
		std::memset(&gLife, 0, sizeof(gLife));
		InitBase(&gLife.base, sizeof(gLife));
		gLife.on_after_created = OnAfterCreated;
		gLife.on_before_close = OnBeforeClose;
		gLife.on_before_popup = OnBeforePopup;

		std::memset(&gLoad, 0, sizeof(gLoad));
		InitBase(&gLoad.base, sizeof(gLoad));
		gLoad.on_loading_state_change = OnLoadingStateChange;
		gLoad.on_load_error = OnLoadError;
		gLoad.on_load_end = OnLoadEnd;

		std::memset(&gDisplay, 0, sizeof(gDisplay));
		InitBase(&gDisplay.base, sizeof(gDisplay));
		gDisplay.on_address_change = OnAddressChange;

		std::memset(&gRender, 0, sizeof(gRender));
		InitBase(&gRender.base, sizeof(gRender));
		gRender.get_view_rect = GetViewRect;
		gRender.get_screen_info = GetScreenInfo;
		gRender.on_paint = OnPaint;

		std::memset(&gResourceRequest, 0, sizeof(gResourceRequest));
		InitBase(&gResourceRequest.base, sizeof(gResourceRequest));
		gResourceRequest.on_before_resource_load = OnBeforeResourceLoad;
		gResourceRequest.get_resource_handler = GetResourceHandler;

		std::memset(&gRequest, 0, sizeof(gRequest));
		InitBase(&gRequest.base, sizeof(gRequest));
		gRequest.get_resource_request_handler = GetResourceRequestHandler;

		std::memset(&gClient, 0, sizeof(gClient));
		InitBase(&gClient.base, sizeof(gClient));
		gClient.get_life_span_handler = GetLifeSpan;
		gClient.get_load_handler = GetLoad;
		gClient.get_display_handler = GetDisplay;
		gClient.get_render_handler = GetRender;
		gClient.get_request_handler = GetRequest;

		std::memset(&gApp, 0, sizeof(gApp));
		InitBase(&gApp.base, sizeof(gApp));
		gApp.on_before_command_line_processing = OnBeforeCommandLine;
	}

	void SendMouseMove(int x, int y, int leave, uint32_t mods)
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		cef_mouse_event_t ev{};
		ev.x = x;
		ev.y = y;
		ev.modifiers = mods;
		host->send_mouse_move_event(host, &ev, leave ? 1 : 0);
		host->base.release(&host->base);
	}

	void SendMouseClick(int x, int y, int button, int up, int clicks, uint32_t mods)
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
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
		cef_mouse_event_t ev{};
		ev.x = x;
		ev.y = y;
		ev.modifiers = mods;
		host->send_mouse_wheel_event(host, &ev, dx, dy);
		host->base.release(&host->base);
	}

	void SendKey(int type, int windowsVk, uint32_t mods, uint32_t character)
	{
		cef_browser_host_t* host = Host();
		if (!host)
			return;
		cef_key_event_t ev{};
		ev.type = static_cast<cef_key_event_type_t>(type);
		ev.modifiers = mods;
		/* CHAR events use the character as windows_key_code (cefclient OSR). */
		if (type == KEYEVENT_CHAR)
			ev.windows_key_code = static_cast<int>(character ? character : windowsVk);
		else
			ev.windows_key_code = windowsVk;
		ev.native_key_code = windowsVk;
		ev.is_system_key = 0;
		ev.character = static_cast<char16>(character);
		ev.unmodified_character = static_cast<char16>(character);
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
				SendKey(ev.a, ev.b, static_cast<uint32_t>(ev.c), ev.character);
				break;
			case WIKI_IN_FOCUS:
				SendFocus(ev.a);
				break;
			default:
				break;
			}
		}
	}

	void ProcessCommands()
	{
		if (!gIpc)
			return;
		gIpc->alive = GetTickCount();
		DrainInput();

		if (gIpc->cmd_seq == gIpc->last_cmd_seq)
			return;
		const uint32_t cmd = gIpc->cmd;
		char arg[sizeof(gIpc->cmd_arg)];
		std::snprintf(arg, sizeof(arg), "%s", gIpc->cmd_arg);
		gIpc->last_cmd_seq = gIpc->cmd_seq;
		gIpc->cmd = WIKI_CMD_NONE;
		gIpc->cmd_arg[0] = 0;

		switch (cmd)
		{
		case WIKI_CMD_NAVIGATE: NavigateTo(arg); break;
		case WIKI_CMD_BACK: if (gBrowser) gBrowser->go_back(gBrowser); break;
		case WIKI_CMD_FORWARD: if (gBrowser) gBrowser->go_forward(gBrowser); break;
		case WIKI_CMD_RELOAD:
			if (gBrowser)
				gBrowser->reload(gBrowser);
			break;
		case WIKI_CMD_HOME: NavigateTo(arg[0] ? arg : gStartUrl.c_str()); break;
		case WIKI_CMD_SET_BOUNDS: NotifyWasResized(); break;
		case WIKI_CMD_SET_VISIBLE:
			if (gIpc)
				gIpc->visible = (arg[0] != '0');
			NotifyWasResized();
			break;
		case WIKI_CMD_QUIT:
			gRunning = false;
			if (gBrowser)
			{
				cef_browser_host_t* host = Host();
				if (host)
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

	LRESULT CALLBACK HelperWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		if (msg == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wp, lp);
	}

	bool CreateOsRBrowser()
	{
		cef_window_info_t info{};
		info.windowless_rendering_enabled = 1;
		info.shared_texture_enabled = 0;

		cef_browser_settings_t bset{};
		bset.size = sizeof(bset);
		bset.windowless_frame_rate = 30;
		/* Light clear color so sites without body{background} stay readable. */
		bset.background_color = CefColorSetARGB(255, 255, 255, 255);

		cef_string_t url{};
		/* Start blank; the addon queues the real site URL once CEF is ready.
		   Do not take --start-url on the command line — that broke launches on Wine. */
		MakeCefString(&url, "about:blank");

		const int ok = g_create_browser(&info, &gClient, &url, &bset, nullptr, nullptr);
		ClearCefString(&url);
		if (!ok)
		{
			SetStatus("cef_browser_host_create_browser failed");
			return false;
		}
		SetStatus("Creating browser…");
		return true;
	}

	std::wstring GetArg(const wchar_t* name, int argc, wchar_t** argv)
	{
		const std::wstring key = std::wstring(L"--") + name + L"=";
		for (int i = 1; i < argc; ++i)
		{
			std::wstring a = argv[i];
			if (a.rfind(key, 0) == 0)
				return a.substr(key.size());
		}
		return {};
	}
}

int APIENTRY wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv)
		return 1;

	gCefDir = GetArg(L"cef-dir", argc, argv);
	{
		const std::wstring start = GetArg(L"start-url", argc, argv);
		if (!start.empty())
			gStartUrl = WideToUtf8(start);
	}
	LocalFree(argv);

	if (gCefDir.empty())
	{
		wchar_t env[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"GW2_HELPER_CEF_DIR", env, MAX_PATH) > 0)
			gCefDir = env;
	}
	if (gCefDir.empty())
		return 2;

	SetDllDirectoryW(gCefDir.c_str());
	if (!LoadCef(gCefDir))
		return 6;

	InitHandlers();

	cef_main_args_t mainArgs{};
	mainArgs.instance = hi;

	const int exitCode = g_execute_process(&mainArgs, &gApp, nullptr);
	if (exitCode >= 0)
		return exitCode;

	gMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kWikiIpcMapName);
	if (!gMap)
		return 4;
	gIpc = static_cast<WikiIpcState*>(MapViewOfFile(gMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(WikiIpcState)));
	if (!gIpc || gIpc->magic != kWikiIpcMagic)
		return 5;

	gFrameMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kWikiFrameMapName);
	if (!gFrameMap)
	{
		SetStatus("Failed to open frame mapping");
		return 9;
	}
	gFramePixels = static_cast<uint8_t*>(MapViewOfFile(gFrameMap, FILE_MAP_ALL_ACCESS, 0, 0, kWikiFrameBytes));
	if (!gFramePixels)
	{
		SetStatus("Failed to map frame buffer");
		return 10;
	}

	SetStatus("Initializing CEF…");

	cef_settings_t settings{};
	settings.size = sizeof(settings);
	settings.no_sandbox = 1;
	/* Single-threaded CEF UI loop so send_key/mouse/focus from DrainInput
	   run on the browser UI thread. multi_threaded_message_loop=1 made
	   OSR input silently fail (page painted but typing/clicks did nothing). */
	settings.multi_threaded_message_loop = 0;
	settings.external_message_pump = 0;
	settings.log_severity = LOGSEVERITY_DISABLE;
	settings.command_line_args_disabled = 0;
	settings.windowless_rendering_enabled = 1;

	wchar_t selfPath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, selfPath, MAX_PATH);

	const std::string cefDirUtf8 = WideToUtf8(gCefDir);
	const std::string selfUtf8 = WideToUtf8(selfPath);
	MakeCefString(&settings.browser_subprocess_path, selfUtf8.c_str());
	MakeCefString(&settings.resources_dir_path, cefDirUtf8.c_str());
	MakeCefString(&settings.locales_dir_path, cefDirUtf8.c_str());
	/* HTTP cache in %TEMP% only — never under Guild Wars 2/addons. */
	wchar_t tmp[MAX_PATH]{};
	GetTempPathW(MAX_PATH, tmp);
	const std::wstring cache = std::wstring(tmp) + L"GW2-InGame-Helper-cef";
	CreateDirectoryW(cache.c_str(), nullptr);
	const std::string cacheUtf8 = WideToUtf8(cache);
	MakeCefString(&settings.cache_path, cacheUtf8.c_str());
	MakeCefString(&settings.root_cache_path, cacheUtf8.c_str());

	if (!g_initialize(&mainArgs, &settings, &gApp, nullptr))
	{
		SetStatus("cef_initialize failed");
		ClearCefString(&settings.browser_subprocess_path);
		ClearCefString(&settings.resources_dir_path);
		ClearCefString(&settings.locales_dir_path);
		ClearCefString(&settings.cache_path);
		ClearCefString(&settings.root_cache_path);
		return 7;
	}
	ClearCefString(&settings.browser_subprocess_path);
	ClearCefString(&settings.resources_dir_path);
	ClearCefString(&settings.locales_dir_path);
	ClearCefString(&settings.cache_path);
	ClearCefString(&settings.root_cache_path);

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = HelperWndProc;
	wc.hInstance = hi;
	wc.lpszClassName = L"GW2InGameHelper_CefHelperWnd";
	RegisterClassExW(&wc);
	gHelperWnd = CreateWindowExW(0, wc.lpszClassName, L"GW2HelperBrowserWnd",
		WS_OVERLAPPED, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hi, nullptr);

	if (!CreateOsRBrowser())
	{
		g_shutdown();
		return 8;
	}

	MSG msg{};
	while (gRunning)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				gRunning = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		/* Must run on this thread with multi_threaded_message_loop=0. */
		g_do_message_loop_work();
		ProcessCommands();
		Sleep(1);
	}

	if (gBrowser)
	{
		gBrowser->base.release(&gBrowser->base);
		gBrowser = nullptr;
	}
	g_shutdown();
	if (gIpc)
	{
		gIpc->ready = 0;
		gIpc->alive = 0;
		UnmapViewOfFile(gIpc);
	}
	if (gFramePixels)
		UnmapViewOfFile(gFramePixels);
	if (gMap)
		CloseHandle(gMap);
	if (gFrameMap)
		CloseHandle(gFrameMap);
	return 0;
}
