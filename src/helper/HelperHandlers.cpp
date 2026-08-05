/* CEF load/display/resource handlers + InitHandlers — HelperDetail. */
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "BootJs.h"
#include "CssCompat.h"
#include "CssProxy.h"
#include "WikiIpc.h"
#include "HelperInternal.h"

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_callback_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_command_line_capi.h"
#include "include/capi/cef_display_handler_capi.h"
#include "include/capi/cef_find_handler_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_load_handler_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/capi/cef_request_capi.h"
#include "include/capi/cef_request_handler_capi.h"
#include "include/capi/cef_resource_request_handler_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
{
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
		const bool active = IsActiveBrowser(browser);
		if (active && gIpc)
		{
			gIpc->can_back = canGoBack ? 1u : 0u;
			gIpc->can_forward = canGoForward ? 1u : 0u;
			SetStatus(isLoading ? "Loading…" : "Ready");
		}
		if (!isLoading && active)
		{
			UpdateUrlFromBrowser();
			/* Native Windows OSR sometimes never paints until was_resized after
			   the first document finishes — kick CEF so the DLL can leave
			   "Waiting for first paint…". */
			if (gIpc && gIpc->frame_seq == 0)
				NotifyWasResized();
		}
		/* BootJs injected from OnLoadEnd only — avoid double parse/exec. */
	}

	void CEF_CALLBACK OnLoadError(
		cef_load_handler_t*, cef_browser_t* browser, cef_frame_t* frame,
		cef_errorcode_t errorCode, const cef_string_t* errorText, const cef_string_t*)
	{
		if (!IsActiveBrowser(browser))
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
		(void)browser;
		InjectBootJs(frame);
	}

	void CEF_CALLBACK OnAddressChange(
		cef_display_handler_t*, cef_browser_t* browser, cef_frame_t* frame, const cef_string_t* url)
	{
		if (!IsActiveBrowser(browser) || !frame || !frame->is_main(frame) || !url)
			return;
		const std::string u = CefStringToUtf8(url);
		/* Skip leftover about:blank in history (e.g. older helpers) when Back lands on it. */
		if ((u == "about:blank" || u == "about:blank/") && browser->can_go_back && browser->can_go_back(browser))
		{
			browser->go_back(browser);
			return;
		}
		/* History / in-page leftovers — map about: and TP query actions. */
		{
			std::string navTo;
			if (ConsumeTpActionUrl(u, &navTo))
			{
				if (!navTo.empty())
					NavigateTo(navTo.c_str());
				return;
			}
			if (ConsumeLedgerActionUrl(u, &navTo))
			{
				if (!navTo.empty())
					NavigateTo(navTo.c_str());
				return;
			}
		}
		if (u.rfind("about:", 0) == 0 && u != "about:blank")
		{
			const std::string resolved = ResolveBuiltinUrl(u.c_str());
			if (!resolved.empty() && resolved != u)
			{
				NavigateTo(resolved.c_str());
				return;
			}
		}
		SetUrlUtf8(u.c_str());
	}

	void CEF_CALLBACK OnTitleChange(
		cef_display_handler_t*, cef_browser_t* browser, const cef_string_t* title)
	{
		if (!IsActiveBrowser(browser) || !title)
			return;
		SetTitleUtf8(CefStringToUtf8(title).c_str());
	}


	void CEF_CALLBACK OnFindResult(
		cef_find_handler_t*, cef_browser_t*, int, int count,
		const cef_rect_t*, int activeMatchOrdinal, int)
	{
		if (!gIpc)
			return;
		gIpc->find_count = static_cast<uint32_t>(count > 0 ? count : 0);
		gIpc->find_ordinal = static_cast<uint32_t>(activeMatchOrdinal > 0 ? activeMatchOrdinal : 0);
	}

	void CEF_CALLBACK OnBeforeCommandLine(
		cef_app_t*, const cef_string_t*, cef_command_line_t* cmd)
	{
		if (!cmd || !cmd->append_switch)
			return;
		/* Software OSR — avoids fighting GW2's D3D device on Wine/Proton.
		   Cap Chromium process fan-out — CEF 150 can spawn enough children to
		   lock the host under Proton if left unrestricted. */
		const char* switches[] = {
			"disable-gpu",
			"disable-gpu-compositing",
			"disable-gpu-vsync",
			"disable-d3d11",
			"disable-direct-composition",
			"in-process-gpu",
			"no-sandbox",
			"disable-extensions",
			"disable-pdf-extension",
			"disable-site-isolation-trials",
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
		if (cmd->append_switch_with_value)
		{
			cef_string_t key{};
			cef_string_t val{};
			MakeCefString(&key, "renderer-process-limit");
			MakeCefString(&val, "1");
			cmd->append_switch_with_value(cmd, &key, &val);
			ClearCefString(&key);
			ClearCefString(&val);
			/* Chrome 141+ Local Network Access / older Private Network Access block
			   discord.com → 127.0.0.1:6463 (Discord desktop RPC). That probe is what
			   unlocks the “Continue to Discord” button instead of a web login form.
			   Game CEF 103 never had this restriction. */
			MakeCefString(&key, "disable-features");
			MakeCefString(&val,
				"LocalNetworkAccessChecks,"
				"BlockInsecurePrivateNetworkRequests,"
				"PrivateNetworkAccessSendPreflights,"
				"PrivateNetworkAccessRespectPreflightResults,"
				"PrivateNetworkAccessNonSecureContextAllowedDeprecationTrial");
			cmd->append_switch_with_value(cmd, &key, &val);
			ClearCefString(&key);
			ClearCefString(&val);
			/* Let discord.com talk to Discord’s local RPC (http://127.0.0.1:6463). */
			MakeCefString(&key, "unsafely-treat-insecure-origin-as-secure");
			MakeCefString(&val,
				"http://127.0.0.1,http://127.0.0.1:6463,"
				"http://localhost,http://localhost:6463,"
				"http://[::1],http://[::1]:6463");
			cmd->append_switch_with_value(cmd, &key, &val);
			ClearCefString(&key);
			ClearCefString(&val);
		}
	}

	cef_return_value_t CEF_CALLBACK OnBeforeResourceLoad(
		cef_resource_request_handler_t*, cef_browser_t* browser, cef_frame_t*,
		cef_request_t* request, cef_callback_t*)
	{
		if (!request || !request->get_url || !g_userfree_free)
			return RV_CONTINUE;

		/* Never cancel the top-level document — that shows up as ERR_ABORTED. */
		const cef_resource_type_t rtype = request->get_resource_type
			? request->get_resource_type(request)
			: RT_MAIN_FRAME;
		if (rtype == RT_MAIN_FRAME)
			return RV_CONTINUE;

		cef_string_userfree_t uf = request->get_url(request);
		if (!uf)
			return RV_CONTINUE;
		const std::string url = CefStringToUtf8(uf);
		g_userfree_free(uf);
		if (ShouldBlockUrl(url))
			return RV_CANCEL;

		const std::string cur = MainFrameUrl(browser);

		/* Never cancel ad / consent / analytics. */
		if (IsAdOrConsentUrl(url))
			return RV_CONTINUE;

		/* Guildjen only: cancel YouTube / googlevideo subframes that steal the
		   guide after Play. Do NOT apply site-wide — that blocks video ads and
		   embeds on Snow Crows / MetaBattle / etc. */
		if (IsGuildjenUrl(cur) &&
			(IsYoutubeHostUrl(url) ||
				url.find("googlevideo.com") != std::string::npos ||
				url.find("ytimg.com/an_webp") != std::string::npos))
		{
			if (rtype == RT_SUB_FRAME || rtype == RT_MEDIA ||
				url.find("/embed/") != std::string::npos)
				return RV_CANCEL;
		}

		return RV_CONTINUE;
	}

	/* CEF 150 blocks unknown schemes unless we opt in — allow Discord app deep links. */
	void CEF_CALLBACK OnProtocolExecution(
		cef_resource_request_handler_t*, cef_browser_t*, cef_frame_t*,
		cef_request_t* request, int* allow_os_execution)
	{
		if (!allow_os_execution)
			return;
		*allow_os_execution = 0;
		if (!request || !request->get_url || !g_userfree_free)
			return;
		cef_string_userfree_t uf = request->get_url(request);
		if (!uf)
			return;
		const std::string url = CefStringToUtf8(uf);
		g_userfree_free(uf);
		if (!IsDiscordProtocolUrl(url))
			return;
		/* Prefer OS handler; do not ShellExecute here — OnBeforeBrowse already
		   opens discord:// via the DLL when the main frame would navigate. */
		*allow_os_execution = 1;
	}

	cef_resource_handler_t* CEF_CALLBACK GetResourceHandler(
		cef_resource_request_handler_t*, cef_browser_t*, cef_frame_t*,
		cef_request_t*)
	{
		/* Do not proxy CSS via WinHTTP — under Wine/Proton it often fails and
		   replaces working (if incomplete) sheets with empty stubs. Styles are
		   fixed via response filter + OnLoadEnd BootJs using CEF's network. */
		return nullptr;
	}

	cef_response_filter_t* CEF_CALLBACK GetResourceResponseFilter(
		cef_resource_request_handler_t*, cef_browser_t*, cef_frame_t*,
		cef_request_t* request, cef_response_t* response)
	{
		if (!request || !response || !request->get_url || !response->get_mime_type ||
			!g_userfree_free)
			return nullptr;
		cef_string_userfree_t ufUrl = request->get_url(request);
		if (!ufUrl)
			return nullptr;
		const std::string url = CefStringToUtf8(ufUrl);
		g_userfree_free(ufUrl);
		cef_string_userfree_t ufMime = response->get_mime_type(response);
		std::string mime;
		if (ufMime)
		{
			mime = CefStringToUtf8(ufMime);
			g_userfree_free(ufMime);
		}
		if (!ShouldDownlevelResponse(url, mime))
			return nullptr;
		const bool isHtml = mime.find("html") != std::string::npos ||
			mime.find("HTML") != std::string::npos;
		return CreateCssDownlevelFilter(isHtml);
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
	cef_find_handler_t* CEF_CALLBACK GetFind(cef_client_t*) { return &gFind; }
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
		gDisplay.on_title_change = OnTitleChange;

		std::memset(&gRender, 0, sizeof(gRender));
		InitBase(&gRender.base, sizeof(gRender));
		gRender.get_view_rect = GetViewRect;
		gRender.get_screen_info = GetScreenInfo;
		gRender.on_popup_show = OnPopupShow;
		gRender.on_popup_size = OnPopupSize;
		gRender.on_paint = OnPaint;

		std::memset(&gFind, 0, sizeof(gFind));
		InitBase(&gFind.base, sizeof(gFind));
		gFind.on_find_result = OnFindResult;

		std::memset(&gResourceRequest, 0, sizeof(gResourceRequest));
		InitBase(&gResourceRequest.base, sizeof(gResourceRequest));
		gResourceRequest.on_before_resource_load = OnBeforeResourceLoad;
		gResourceRequest.get_resource_handler = GetResourceHandler;
		gResourceRequest.get_resource_response_filter = GetResourceResponseFilter;
		gResourceRequest.on_protocol_execution = OnProtocolExecution;

		std::memset(&gRequest, 0, sizeof(gRequest));
		InitBase(&gRequest.base, sizeof(gRequest));
		gRequest.on_before_browse = OnBeforeBrowse;
		gRequest.on_open_urlfrom_tab = OnOpenUrlFromTab;
		gRequest.get_resource_request_handler = GetResourceRequestHandler;

		std::memset(&gClient, 0, sizeof(gClient));
		InitBase(&gClient.base, sizeof(gClient));
		gClient.get_life_span_handler = GetLifeSpan;
		gClient.get_load_handler = GetLoad;
		gClient.get_display_handler = GetDisplay;
		gClient.get_render_handler = GetRender;
		gClient.get_find_handler = GetFind;
		gClient.get_request_handler = GetRequest;

		std::memset(&gApp, 0, sizeof(gApp));
		InitBase(&gApp.base, sizeof(gApp));
		gApp.on_before_command_line_processing = OnBeforeCommandLine;
	}
}
