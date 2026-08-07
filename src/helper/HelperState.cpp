/* Helper shared globals + CEF load/string/IPC helpers — HelperDetail. */
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "WikiIpc.h"
#include "HelperInternal.h"

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"
#include "include/cef_api_hash.h"

namespace HelperDetail
{
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
	Fn_cef_api_hash g_api_hash = nullptr;

	WikiIpcState* gIpc = nullptr;
	HANDLE gMap = nullptr;
	HANDLE gFrameMap = nullptr;
	HANDLE gWakeEvent = nullptr;
	uint8_t* gFramePixels = nullptr;
	HWND gHelperWnd = nullptr;
	std::wstring gCefDir;
	std::string gStartUrl = "about:blank";
	std::atomic<bool> gRunning{true};
	cef_browser_t* gBrowsers[kWikiMaxTabs] = {};
	int gActiveSlot = 0;
	/* Serialized creates: only one create_browser in flight. Parallel creates
	   can complete out of order and FIFO-assign browsers to the wrong slots
	   (close tab 2 then kills tab 1's page). */
	int gCreateQueueSlots[kWikiMaxTabs] = {};
	std::string gCreateQueueUrls[kWikiMaxTabs];
	int gCreateQueueHead = 0;
	int gCreateQueueCount = 0;
	int gCreateInFlightSlot = -1;
	bool gCreateInFlightDiscard = false;
	int gPendingActivateSlot = -1;

	cef_app_t gApp{};
	cef_client_t gClient{};
	cef_life_span_handler_t gLife{};
	cef_load_handler_t gLoad{};
	cef_display_handler_t gDisplay{};
	cef_render_handler_t gRender{};
	cef_find_handler_t gFind{};
	cef_request_handler_t gRequest{};
	cef_resource_request_handler_t gResourceRequest{};
	cef_context_menu_handler_t gContextMenu{};

	/* OSR <select> / combobox popup — PET_POPUP must be composited onto the view
	   or dropdowns are invisible and clicks miss (breaks account Save too). */
	bool gPopupShow = false;
	cef_rect_t gPopupRect{};
	std::vector<uint8_t> gViewCache;
	std::vector<uint8_t> gPopupCache;
	int gViewCacheW = 0;
	int gViewCacheH = 0;
	int gPopupCacheW = 0;
	int gPopupCacheH = 0;
	/* One-shot: ask CEF for the first PET_POPUP after show — never invalidate
	   from every OnPaint (that lock-loops the helper under Proton). */
	bool gPopupInvalidateOnce = false;
	/* After PET_POPUP hides, the trailing mouse-up often lands on the page under
	   the former list (no popup offset) and triggers a link/form "refresh". */
	bool gSwallowPopupMouseUp = false;
	cef_rect_t gPopupSwallowRect{};

	cef_browser_t* ActiveBrowser()
	{
		if (gActiveSlot < 0 || gActiveSlot >= kWikiMaxTabs)
			return nullptr;
		return gBrowsers[gActiveSlot];
	}

	bool IsActiveBrowser(cef_browser_t* browser)
	{
		cef_browser_t* active = ActiveBrowser();
		return active && browser && active->is_same(active, browser);
	}

	void UpdateTabMask()
	{
		if (!gIpc)
			return;
		uint32_t mask = 0;
		for (int i = 0; i < kWikiMaxTabs; ++i)
		{
			if (gBrowsers[i])
				mask |= (1u << i);
		}
		gIpc->tab_mask = mask;
	}

	void SetStatus(const char* text)
	{
		if (!gIpc || !text)
			return;
		std::snprintf(gIpc->status, sizeof(gIpc->status), "%s", text);
	}

	/* Odd seq = write in progress; even = stable. DLL retries until even+unchanged. */
	void PublishFencedString(char* dst, size_t dstCap, uint32_t* lenOut, uint32_t* seq, const char* text)
	{
		++(*seq); /* odd — readers retry */
		const int n = std::snprintf(dst, dstCap, "%s", text ? text : "");
		*lenOut = (n > 0) ? static_cast<uint32_t>(n) : 0u;
		if (*lenOut >= dstCap)
			*lenOut = static_cast<uint32_t>(dstCap - 1);
		++(*seq); /* even — stable */
	}

	void SetTitleUtf8(const char* text)
	{
		if (!gIpc || !text)
			return;
		PublishFencedString(gIpc->title, sizeof(gIpc->title), &gIpc->title_len, &gIpc->title_seq, text);
	}

	void SetUrlUtf8(const char* text)
	{
		if (!gIpc || !text)
			return;
		PublishFencedString(gIpc->url, sizeof(gIpc->url), &gIpc->url_len, &gIpc->url_seq, text);
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
		g_api_hash = reinterpret_cast<Fn_cef_api_hash>(sym("cef_api_hash"));

		if (!g_execute_process || !g_initialize || !g_shutdown || !g_do_message_loop_work ||
			!g_create_browser || !g_string_from_utf8 || !g_string_clear || !g_api_hash)
		{
			SetStatus("libcef.dll missing required exports");
			return false;
		}

		/* CEF 133+: must configure API version before execute_process/initialize
		   or handlers fatal with "invalid version -1". */
		const char* hash = g_api_hash(CEF_API_VERSION, 0);
		if (!hash || !hash[0])
		{
			SetStatus("cef_api_hash failed");
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
		cef_browser_t* browser = ActiveBrowser();
		if (!gIpc || !browser)
			return;
		gIpc->can_back = browser->can_go_back(browser) ? 1u : 0u;
		gIpc->can_forward = browser->can_go_forward(browser) ? 1u : 0u;
	}

	void UpdateUrlFromBrowser()
	{
		cef_browser_t* browser = ActiveBrowser();
		if (!browser || !g_userfree_free)
			return;
		cef_frame_t* frame = browser->get_main_frame(browser);
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


} // namespace HelperDetail
