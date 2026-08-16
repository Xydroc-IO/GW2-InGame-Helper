#pragma once

/* Internal shared state for GW2HelperBrowser (not public API). */

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

#include "WikiIpc.h"

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_command_line_capi.h"
#include "include/capi/cef_context_menu_handler_capi.h"
#include "include/capi/cef_display_handler_capi.h"
#include "include/capi/cef_find_handler_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_load_handler_capi.h"
#include "include/capi/cef_menu_model_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/capi/cef_request_handler_capi.h"
#include "include/capi/cef_resource_request_handler_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
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
	using Fn_cef_string_utf16_to_utf8 = int (*)(const char16_t*, size_t, cef_string_utf8_t*);
	using Fn_cef_string_utf8_clear = void (*)(cef_string_utf8_t*);
	using Fn_cef_string_userfree_free = void (*)(cef_string_userfree_t);
	using Fn_cef_api_hash = const char* (*)(int version, int entry);

	extern HMODULE gLib;
	extern Fn_cef_execute_process g_execute_process;
	extern Fn_cef_initialize g_initialize;
	extern Fn_cef_shutdown g_shutdown;
	extern Fn_cef_do_message_loop_work g_do_message_loop_work;
	extern Fn_cef_browser_host_create_browser g_create_browser;
	extern Fn_cef_string_from_utf8 g_string_from_utf8;
	extern Fn_cef_string_clear g_string_clear;
	extern Fn_cef_string_utf16_to_utf8 g_utf16_to_utf8;
	extern Fn_cef_string_utf8_clear g_utf8_clear;
	extern Fn_cef_string_userfree_free g_userfree_free;
	extern Fn_cef_api_hash g_api_hash;

	extern WikiIpcState* gIpc;
	extern HANDLE gMap;
	extern HANDLE gFrameMap;
	extern HANDLE gWakeEvent;
	extern uint8_t* gFramePixels;
	extern HWND gHelperWnd;
	extern std::wstring gCefDir;
	extern std::string gStartUrl;
	extern std::atomic<bool> gRunning;
	extern cef_browser_t* gBrowsers[kWikiMaxTabs];
	extern int gActiveSlot;
	extern int gCreateQueueSlots[kWikiMaxTabs];
	extern std::string gCreateQueueUrls[kWikiMaxTabs];
	extern int gCreateQueueHead;
	extern int gCreateQueueCount;
	extern int gCreateInFlightSlot;
	extern bool gCreateInFlightDiscard;
	extern int gPendingActivateSlot;

	extern cef_app_t gApp;
	extern cef_client_t gClient;
	extern cef_life_span_handler_t gLife;
	extern cef_load_handler_t gLoad;
	extern cef_display_handler_t gDisplay;
	extern cef_render_handler_t gRender;
	extern cef_find_handler_t gFind;
	extern cef_request_handler_t gRequest;
	extern cef_resource_request_handler_t gResourceRequest;
	extern cef_context_menu_handler_t gContextMenu;

	extern bool gPopupShow;
	extern cef_rect_t gPopupRect;
	extern std::vector<uint8_t> gViewCache;
	extern std::vector<uint8_t> gPopupCache;
	extern int gViewCacheW;
	extern int gViewCacheH;
	extern int gPopupCacheW;
	extern int gPopupCacheH;
	extern bool gPopupInvalidateOnce;
	extern bool gSwallowPopupMouseUp;
	extern cef_rect_t gPopupSwallowRect;

	/* Shared helpers (HelperState.cpp) */
	cef_browser_t* ActiveBrowser();
	bool IsActiveBrowser(cef_browser_t* browser);
	void UpdateTabMask();
	void SetStatus(const char* text);
	void PublishFencedString(char* dst, size_t dstCap, uint32_t* lenOut, uint32_t* seq, const char* text);
	void SetTitleUtf8(const char* text);
	void SetUrlUtf8(const char* text);
	std::string WideToUtf8(const std::wstring& w);
	bool LoadCef(const std::wstring& cefDir);
	void MakeCefString(cef_string_t* out, const char* utf8);
	void ClearCefString(cef_string_t* s);
	std::string CefStringToUtf8(const cef_string_t* s);
	void InitBase(cef_base_ref_counted_t* base, size_t size);
	void NavigateTo(const char* url);
	void NavigateSlot(int slot, const char* url);
	std::string ResolveBuiltinUrl(const char* url);
	bool ConsumeTpActionUrl(const std::string& url, std::string* outNavigate);
	bool ConsumeLedgerActionUrl(const std::string& url, std::string* outNavigate);
	bool ConsumeBrowseHubActionUrl(const std::string& url, std::string* outNavigate);
	std::string UrlDecodeQueryValue(const std::string& in);
	void RefreshNavFlags();
	void UpdateUrlFromBrowser();
	std::wstring HelperDir();
	std::wstring EnsureHelperUnder(const std::wstring& root, const wchar_t* relative);
	std::wstring HelperPagesDir();
	std::wstring HelperCmdsDir();
	std::string WidePathToFileUrl(const std::wstring& path);
	void AppendCmdLine(const std::wstring& fileName, const std::string& line);
	void QueueTpWatchCmd(const char* op, int id);
	int ParseQueryInt(const std::string& query, const char* key);
	std::string ParseQueryValue(const std::string& query, const char* key);
	std::string AboutFromBrowseFileUrl(const std::string& url);
	bool RecoverMissingLiveFileUrl(const std::string& url);
	void InvalidateHelperBrowseCaches();
	void InvalidateHelperBrowsePage(const std::string& fileUrl);

	/* Tabs / lifespan (HelperTabs.cpp) */
	cef_browser_host_t* Host();
	void NotifyWasResized();
	void ActivateSlot(int slot);
	void AdjustCreateQueueForClose(int closedSlot);
	bool EnqueueBrowserCreate(int slot, const char* url);
	bool StartNextBrowserCreate();
	bool CreateBrowserForSlot(int slot, const char* url);
	void CloseSlot(int slot);
	void ViewSize(int* outW, int* outH);
	void CEF_CALLBACK OnAfterCreated(cef_life_span_handler_t*, cef_browser_t* browser);
	void CEF_CALLBACK OnBeforeClose(cef_life_span_handler_t*, cef_browser_t* browser);

	/* Load/display/resource + wiring (HelperHandlers.cpp) */
	void InjectBootJs(cef_frame_t* frame);
	void CEF_CALLBACK OnLoadingStateChange(
		cef_load_handler_t*, cef_browser_t* browser, int isLoading, int canGoBack, int canGoForward);
	void CEF_CALLBACK OnLoadError(
		cef_load_handler_t*, cef_browser_t* browser, cef_frame_t* frame, cef_errorcode_t errorCode,
		const cef_string_t* errorText, const cef_string_t* failedUrl);
	void CEF_CALLBACK OnLoadEnd(
		cef_load_handler_t*, cef_browser_t* browser, cef_frame_t* frame, int);
	void CEF_CALLBACK OnAddressChange(
		cef_display_handler_t*, cef_browser_t* browser, cef_frame_t* frame, const cef_string_t* url);
	void CEF_CALLBACK OnTitleChange(
		cef_display_handler_t*, cef_browser_t* browser, const cef_string_t* title);
	void CEF_CALLBACK OnFindResult(
		cef_find_handler_t*, cef_browser_t*, int, int count, const cef_rect_t*, int activeMatchOrdinal, int);
	void CEF_CALLBACK OnBeforeCommandLine(
		cef_app_t*, const cef_string_t*, cef_command_line_t* cmd);
	void InitHandlers();

	/* Input + IPC cmds (HelperCommands.cpp) */
	void ProcessCommands();

	/* Entry helpers (main.cpp) */
	LRESULT CALLBACK HelperWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	bool CreateOsRBrowser();
	std::wstring GetArg(const wchar_t* name, int argc, wchar_t** argv);

	/* Nav policy (HelperNavPolicy.cpp / HelperNavPolicyHandlers.cpp) */
	bool IsDiscordProtocolUrl(const std::string& url);
	bool IsLaunchableExternalUrl(const std::string& url);
	void NavLog(const char* fmt, ...);
	void OpenExternalUrl(const std::string& url);
	void QueueOpenInAddonTab(const std::string& url);
	void QueueOpenSiteInAddonTab(const std::string& siteId, bool newTab = false);
	bool ConsumeHelperNewTabUrl(const std::string& url);
	bool TryOpenUrlInNewAddonTab(const std::string& url);
	bool IsNewTabOrWindowDisposition(cef_window_open_disposition_t d);
	bool IsMediaOrCdnUrl(const std::string& url);
	bool IsExternalSignInUrl(const std::string& url);
	bool IsPromotablePopupUrl(const std::string& url);
	bool IsAdFrameUrl(const std::string& url);
	bool IsAdOrConsentUrl(const std::string& url);
	bool IsAdClickUrl(const std::string& url);
	bool IsSameSite(const std::string& a, const std::string& b);
	bool IsGuildjenUrl(const std::string& url);
	bool IsYoutubeHostUrl(const std::string& url);
	bool IsTwitchHostUrl(const std::string& url);
	std::string FrameUrl(cef_frame_t* frame);
	std::string MainFrameUrl(cef_browser_t* browser);
	int CEF_CALLBACK OnBeforePopup(
		cef_life_span_handler_t*, cef_browser_t* browser, cef_frame_t* frame, int popup_id,
		const cef_string_t* target_url, const cef_string_t*, cef_window_open_disposition_t,
		int user_gesture, const cef_popup_features_t*, cef_window_info_t*, cef_client_t**,
		cef_browser_settings_t*, cef_dictionary_value_t**, int*);
	int CEF_CALLBACK OnBeforeBrowse(
		cef_request_handler_t*, cef_browser_t*, cef_frame_t*,
		cef_request_t*, int user_gesture, int is_redirect);
	int CEF_CALLBACK OnOpenUrlFromTab(
		cef_request_handler_t*, cef_browser_t*, cef_frame_t*,
		const cef_string_t* target_url, cef_window_open_disposition_t, int user_gesture);

	/* OSR (HelperOsrRender.cpp) */
	void CEF_CALLBACK GetViewRect(cef_render_handler_t*, cef_browser_t*, cef_rect_t* rect);
	int CEF_CALLBACK GetScreenInfo(cef_render_handler_t*, cef_browser_t*, cef_screen_info_t* info);
	void CEF_CALLBACK OnPopupShow(cef_render_handler_t*, cef_browser_t* browser, int show);
	void CEF_CALLBACK OnPopupSize(cef_render_handler_t*, cef_browser_t*, const cef_rect_t* rect);
	void CEF_CALLBACK OnPaint(
		cef_render_handler_t*, cef_browser_t*, cef_paint_element_type_t type,
		size_t dirtyRectsCount, const cef_rect_t* dirtyRects, const void* buffer,
		int width, int height);
	bool IsOverPopup(int x, int y);
	void ApplyPopupMouseOffset(int& x, int& y);
	void PublishCompositedFrame(int dirtyX, int dirtyY, int dirtyW, int dirtyH);
}
