/* GW2HelperBrowser.exe — windowless CEF helper using private CEF 150 under
   addons/GW2-InGame-Helper/cef/libcef.dll. Paints BGRA frames into shared
   memory for the addon. CSS downlevel is gated off (native oklch / color-mix). */

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>
#include <vector>

#include "BootJs.h"
#include "CssCompat.h"
#include "CssProxy.h"
#include "WikiIpc.h"

#include "include/cef_api_hash.h"
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
	using Fn_cef_string_utf16_to_utf8 = int (*)(const char16_t*, size_t, cef_string_utf8_t*);
	using Fn_cef_string_utf8_clear = void (*)(cef_string_utf8_t*);
	using Fn_cef_string_userfree_free = void (*)(cef_string_userfree_t);
	using Fn_cef_api_hash = const char* (*)(int version, int entry);

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

	std::wstring HelperDir()
	{
		wchar_t path[MAX_PATH]{};
		if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
			return {};
		std::wstring full = path;
		const size_t slash = full.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return {};
		return full.substr(0, slash);
	}

	std::string WidePathToFileUrl(const std::wstring& path)
	{
		std::string utf8 = WideToUtf8(path);
		for (char& c : utf8)
		{
			if (c == '\\')
				c = '/';
		}
		if (utf8.size() >= 2 && utf8[1] == ':')
			return std::string("file:///") + utf8;
		if (!utf8.empty() && utf8[0] == '/')
			return std::string("file://") + utf8;
		return std::string("file:///") + utf8;
	}

	/* Queue TP watchlist mutate for the DLL (LivePanels::Tick applies + regenerates). */
	void QueueTpWatchCmd(const char* op, int id)
	{
		if (!op || id <= 0)
			return;
		const std::wstring dir = HelperDir();
		if (dir.empty())
			return;
		const std::wstring path = dir + L"\\live-tp-cmd.txt";
		char line[64];
		std::snprintf(line, sizeof(line), "%s %d\n", op, id);
		HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return;
		DWORD written = 0;
		WriteFile(h, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
		CloseHandle(h);
		/* Drop ready stamp so DLL rebuilds the list after applying the cmd. */
		DeleteFileW((dir + L"\\live-tp.ok").c_str());
		DeleteFileW((dir + L"\\live-tp.ver").c_str());
	}

	int ParseQueryInt(const std::string& query, const char* key)
	{
		std::string pat = key;
		pat += '=';
		size_t p = query.find(pat);
		if (p == std::string::npos)
			return 0;
		p += pat.size();
		int id = 0;
		while (p < query.size() && query[p] >= '0' && query[p] <= '9')
		{
			id = id * 10 + (query[p] - '0');
			++p;
		}
		return id;
	}

	/* Addon normally rewrites these before IPC; keep a local fallback so
	   about:helper-home / about:raid-food / cheat sheets never hit CEF blank. */
	std::string ResolveBuiltinUrl(const char* url)
	{
		if (!url || !url[0])
			return {};
		/* In-page TP watchlist add/remove — DLL picks up live-tp-cmd.txt next frame. */
		if (std::strncmp(url, "about:live-tp-add-", 18) == 0)
		{
			int id = 0;
			for (const char* p = url + 18; *p >= '0' && *p <= '9'; ++p)
				id = id * 10 + (*p - '0');
			QueueTpWatchCmd("add", id);
			url = "about:live-tp";
		}
		else if (std::strncmp(url, "about:live-tp-remove-", 21) == 0)
		{
			int id = 0;
			for (const char* p = url + 21; *p >= '0' && *p <= '9'; ++p)
				id = id * 10 + (*p - '0');
			QueueTpWatchCmd("remove", id);
			url = "about:live-tp";
		}
		const wchar_t* fileNameW = nullptr;
		if (std::strcmp(url, "about:helper-home") == 0)
			fileNameW = L"helper-home.html";
		else if (std::strcmp(url, "about:raid-food") == 0)
			fileNameW = L"raid-food.html";
		else if (std::strcmp(url, "about:raid-utilities") == 0)
			fileNameW = L"raid-utilities.html";
		else if (std::strcmp(url, "about:fractal-consumables") == 0)
			fileNameW = L"fractal-consumables.html";
		else if (std::strcmp(url, "about:sigils-runes") == 0)
			fileNameW = L"sigils-runes.html";
		else if (std::strcmp(url, "about:relics") == 0)
			fileNameW = L"relics-guide.html";
		else if (std::strcmp(url, "about:boon-checklist") == 0)
			fileNameW = L"boon-checklist.html";
		else if (std::strcmp(url, "about:cc-defiance") == 0)
			fileNameW = L"cc-defiance.html";
		else if (std::strcmp(url, "about:raid-wings") == 0)
			fileNameW = L"raid-wings.html";
		else if (std::strcmp(url, "about:home-garden") == 0)
			fileNameW = L"home-garden.html";
		else if (std::strcmp(url, "about:ubers-aio") == 0)
			fileNameW = L"ubers-all-in-one.html";
		else if (std::strcmp(url, "about:strike-missions") == 0)
			fileNameW = L"strike-missions.html";
		else if (std::strcmp(url, "about:fractal-cm") == 0)
			fileNameW = L"fractal-cm-list.html";
		else if (std::strcmp(url, "about:squad-template") == 0)
			fileNameW = L"squad-template.html";
		else if (std::strcmp(url, "about:stability-cleanse") == 0)
			fileNameW = L"stability-cleanse.html";
		else if (std::strcmp(url, "about:material-conversions") == 0)
			fileNameW = L"material-conversions.html";
		else if (std::strcmp(url, "about:legendary-paths") == 0)
			fileNameW = L"legendary-paths.html";
		else if (std::strcmp(url, "about:mount-unlock") == 0)
			fileNameW = L"mount-unlock.html";
		else if (std::strcmp(url, "about:daily-weekly") == 0)
			fileNameW = L"daily-weekly.html";
		else if (std::strcmp(url, "about:live-dailies") == 0)
			fileNameW = L"live-dailies.html";
		else if (std::strcmp(url, "about:live-news") == 0)
			fileNameW = L"live-news.html";
		else if (std::strcmp(url, "about:live-fashion") == 0)
			fileNameW = L"live-fashion.html";
		else if (std::strcmp(url, "about:live-tp") == 0)
			fileNameW = L"live-tp.html";
		else if (std::strcmp(url, "about:live-progress") == 0)
			fileNameW = L"live-progress.html";
		else if (std::strcmp(url, "about:currency-sinks") == 0)
			fileNameW = L"currency-sinks.html";
		else if (std::strcmp(url, "about:ascended-start") == 0)
			fileNameW = L"ascended-start.html";
		else if (std::strcmp(url, "about:portals-pulls") == 0)
			fileNameW = L"portals-pulls.html";
		else if (std::strcmp(url, "about:homestead") == 0)
			fileNameW = L"homestead-extras.html";
		else if (std::strcmp(url, "about:wvw-consumables") == 0)
			fileNameW = L"wvw-consumables.html";
		else
			return url;

		const std::wstring dir = HelperDir();
		if (dir.empty())
			return url;
		const std::wstring path = dir + L"\\" + fileNameW;
		if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
			return url;
		return WidePathToFileUrl(path);
	}

	/* Handle TP add/remove from about: or file://?gw2igh-tp-add=N before CEF sees them. */
	bool ConsumeTpActionUrl(const std::string& url, std::string* outNavigate)
	{
		if (!outNavigate)
			return false;
		outNavigate->clear();

		if (url.rfind("about:live-tp-add-", 0) == 0 ||
			url.rfind("about:live-tp-remove-", 0) == 0)
		{
			*outNavigate = ResolveBuiltinUrl(url.c_str());
			return !outNavigate->empty();
		}

		/* file:///.../live-tp.html?gw2igh-tp-add=19721 */
		if (url.find("live-tp.html") == std::string::npos)
			return false;
		size_t q = url.find('?');
		if (q == std::string::npos)
			return false;
		std::string query = url.substr(q + 1);
		const size_t hash = query.find('#');
		if (hash != std::string::npos)
			query.resize(hash);
		const int addId = ParseQueryInt(query, "gw2igh-tp-add");
		const int remId = ParseQueryInt(query, "gw2igh-tp-remove");
		if (addId <= 0 && remId <= 0)
			return false;
		if (addId > 0)
			QueueTpWatchCmd("add", addId);
		if (remId > 0)
			QueueTpWatchCmd("remove", remId);
		*outNavigate = url.substr(0, q);
		return true;
	}

	void NavigateSlot(int slot, const char* url)
	{
		if (slot < 0 || slot >= kWikiMaxTabs || !gBrowsers[slot] || !url || !url[0])
			return;
		const std::string resolved = ResolveBuiltinUrl(url);
		if (resolved.empty())
			return;
		cef_frame_t* frame = gBrowsers[slot]->get_main_frame(gBrowsers[slot]);
		if (!frame)
			return;
		cef_string_t u{};
		MakeCefString(&u, resolved.c_str());
		frame->load_url(frame, &u);
		ClearCefString(&u);
		frame->base.release(&frame->base);
		if (slot == gActiveSlot)
			SetStatus("Navigating…");
	}

	void NavigateTo(const char* url)
	{
		NavigateSlot(gActiveSlot, url);
	}

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

	bool StartNextBrowserCreate();

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
		bset.background_color = CefColorSetARGB(255, 255, 255, 255);

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

	bool IsDiscordProtocolUrl(const std::string& url)
	{
		/* discord://… — Discord desktop app deep link (OAuth handoff). */
		return url.rfind("discord:", 0) == 0 || url.rfind("Discord:", 0) == 0;
	}

	bool IsLaunchableExternalUrl(const std::string& url)
	{
		return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0 ||
			IsDiscordProtocolUrl(url);
	}

	/* Navigation decision trace. Silent unless a "navlog.on" marker file sits
	   next to the helper exe, so shipped builds never touch the disk. */
	void NavLog(const char* fmt, ...)
	{
		static int enabled = -1;
		static FILE* out = nullptr;
		if (enabled < 0)
		{
			wchar_t exe[MAX_PATH]{};
			GetModuleFileNameW(nullptr, exe, MAX_PATH);
			std::wstring dir(exe);
			const size_t slash = dir.find_last_of(L"\\/");
			dir = (slash == std::wstring::npos) ? std::wstring(L".") : dir.substr(0, slash);
			enabled = (GetFileAttributesW((dir + L"\\navlog.on").c_str()) !=
				INVALID_FILE_ATTRIBUTES) ? 1 : 0;
			if (enabled == 1)
				out = _wfopen((dir + L"\\navlog.txt").c_str(), L"a");
		}
		if (enabled != 1 || !out)
			return;
		va_list ap;
		va_start(ap, fmt);
		std::vfprintf(out, fmt, ap);
		va_end(ap);
		std::fputc('\n', out);
		std::fflush(out);
	}

	void OpenExternalUrl(const std::string& url)
	{
		if (!IsLaunchableExternalUrl(url))
		{
			NavLog("  -> DROPPED (not launchable) %s", url.c_str());
			return;
		}
		NavLog("  -> EXTERNAL len=%zu %s", url.size(), url.c_str());
		/* Prefer DLL-side ShellExecute (Proton/Wine: helper process often no-ops). */
		if (gIpc)
		{
			/* Half a click tracker opens a blank error page, which reads as "ads are
			   broken" — refuse the handoff instead of sending a truncated URL. */
			if (url.size() >= sizeof(gIpc->open_ext_url))
			{
				NavLog("  -> REFUSED (too long, %zu bytes)", url.size());
				SetStatus("Link too long to open externally");
				return;
			}
			std::snprintf(gIpc->open_ext_url, sizeof(gIpc->open_ext_url), "%s", url.c_str());
			MemoryBarrier();
			++gIpc->open_ext_seq;
			SetStatus(IsDiscordProtocolUrl(url)
				? "Opening Discord app…"
				: "Opened in system browser (Open Ext)");
			return;
		}
		ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	/* Ask the DLL to open a URL in a new helper tab (keeps the current Live page). */
	void QueueOpenInAddonTab(const std::string& url)
	{
		if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
			return;
		if (!gIpc)
			return;
		if (url.size() >= sizeof(gIpc->open_tab_url))
		{
			SetStatus("Link too long for a new tab");
			return;
		}
		NavLog("  -> ADDON-TAB %s", url.c_str());
		std::snprintf(gIpc->open_tab_url, sizeof(gIpc->open_tab_url), "%s", url.c_str());
		MemoryBarrier();
		++gIpc->open_tab_seq;
		SetStatus("Opening in a new tab…");
	}

	std::string UrlDecodeQueryValue(const std::string& in)
	{
		std::string out;
		out.reserve(in.size());
		for (size_t i = 0; i < in.size(); ++i)
		{
			if (in[i] == '+' )
			{
				out.push_back(' ');
				continue;
			}
			if (in[i] == '%' && i + 2 < in.size())
			{
				auto hex = [](char c) -> int {
					if (c >= '0' && c <= '9') return c - '0';
					if (c >= 'a' && c <= 'f') return c - 'a' + 10;
					if (c >= 'A' && c <= 'F') return c - 'A' + 10;
					return -1;
				};
				const int hi = hex(in[i + 1]);
				const int lo = hex(in[i + 2]);
				if (hi >= 0 && lo >= 0)
				{
					out.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
					continue;
				}
			}
			out.push_back(in[i]);
		}
		return out;
	}

	/* Character → gw2efficiency: file://…?gw2igh-newtab=https%3A%2F%2F…
	   (about: is blocked from file:// pages — same lesson as TP watchlist). */
	bool ConsumeHelperNewTabUrl(const std::string& url)
	{
		static const char kAbout[] = "about:helper-newtab:";
		if (url.rfind(kAbout, 0) == 0)
		{
			QueueOpenInAddonTab(url.substr(sizeof(kAbout) - 1));
			return true;
		}

		const size_t mark = url.find("gw2igh-newtab=");
		if (mark == std::string::npos)
			return false;
		std::string enc = url.substr(mark + 14);
		const size_t cut = enc.find_first_of("&#");
		if (cut != std::string::npos)
			enc.resize(cut);
		const std::string target = UrlDecodeQueryValue(enc);
		if (target.rfind("https://", 0) != 0 && target.rfind("http://", 0) != 0)
			return false;
		QueueOpenInAddonTab(target);
		return true;
	}

	/* Media / CDN / account URLs must never become the main-frame document —
	   promoting them after an embed Play looks like the guide refreshed. */
	bool IsMediaOrCdnUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return has("googlevideo.com") || has("ytimg.com") || has("ggpht.com") ||
			has("googleusercontent.com") || has("youtube-nocookie.com/embed") ||
			has("youtube.com/embed") || has("youtube.com/live_chat") ||
			has("youtube.com/watch") || has("youtu.be/") ||
			has("accounts.google.com") || has("accounts.youtube.com") ||
			has("consent.youtube.com") || has("consent.google.com") ||
			has("vimeo.com") || has("player.vimeo.com");
	}

	/* Google sign-in, consent, and the /sorry "unusual traffic" captcha cannot be
	   completed in embedded OSR (Google blocks the UA and reCAPTCHA can't be
	   solved). These previously matched IsMediaOrCdnUrl and were cancelled with no
	   action, so the button looked dead. Hand them to the system browser instead. */
	bool IsExternalSignInUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return has("accounts.google.com") || has("accounts.youtube.com") ||
			has("consent.google.com") || has("consent.youtube.com") ||
			has("google.com/sorry") || has("google.com/recaptcha");
	}

	bool IsYoutubeHostUrl(const std::string& url)
	{
		return url.find("youtube.com") != std::string::npos ||
			url.find("youtu.be") != std::string::npos ||
			url.find("youtube-nocookie.com") != std::string::npos;
	}

	/* Official CEF binaries ship without the proprietary codecs (H.264 / AAC)
	   Twitch streams with, so its player always ends at "Error #4000 — resource
	   format not supported". Enabling them means building Chromium from source
	   and licensing the codecs, so route Twitch to the system browser instead. */
	bool IsTwitchHostUrl(const std::string& url)
	{
		return url.find("twitch.tv") != std::string::npos ||
			url.find("ttvnw.net") != std::string::npos;
	}

	bool IsGuildjenUrl(const std::string& url)
	{
		return url.find("guildjen.com") != std::string::npos;
	}

	bool IsAdFrameUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return has("nitropay.com") || has("s.nitropay.com") ||
			has("googlesyndication.com") || has("doubleclick.net") ||
			has("googleadservices.com") || has("pagead2.googlesyndication") ||
			has("adservice.google") || has("adnxs.com") ||
			has("amazon-adsystem.com") || has("ads-twitter.com") ||
			has("facebook.net") || has("connect.facebook") ||
			has("securepubads.g.doubleclick") || has("pagead") ||
			has("adsystem") || has("advertising");
	}

	/* Never cancel ad / consent / analytics hosts — ads must load for site partners. */
	bool IsAdOrConsentUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return IsAdFrameUrl(url) ||
			has("cookieinformation.com") || has("policy.app.cookieinformation") ||
			has("consent.cookiebot") || has("onetrust.com") ||
			has("cookielaw.org") || has("fundingchoicesmessages") ||
			has("consent.google.com");
	}

	bool HasQueryParam(const std::string& url, const char* key)
	{
		const size_t q = url.find('?');
		if (q == std::string::npos)
			return false;
		const std::string k(key);
		return url.find("?" + k + "=", q) != std::string::npos ||
			url.find("&" + k + "=", q) != std::string::npos;
	}

	/* Ad network click identifiers. A landing page only carries one of these when
	   a network billed the click, so it belongs to the advertiser even when the
	   creative reports the publisher as its referrer (seen on Google display). */
	bool HasAdClickId(const std::string& url)
	{
		static const char* const kClickIds[] = {
			"gclid", "dclid", "gbraid", "wbraid", "gad_source", "gad_campaignid",
			"msclkid", "fbclid", "ttclid", "twclid",
		};
		for (const char* key : kClickIds)
		{
			if (HasQueryParam(url, key))
				return true;
		}
		return false;
	}

	/* Ad click-through navigations must leave the OSR browser. Loading ad
	   resources stays in CEF; only explicit tracker/click URLs are handed off. */
	bool IsAdClickUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return HasAdClickId(url) ||
			has("adclick.g.doubleclick.net/") ||
			has("googleadservices.com/pagead/aclk") ||
			has("googlesyndication.com/pagead/aclk") ||
			has("googlesyndication.com/pagead/clk") ||
			has("amazon-adsystem.com/x/c/") ||
			has("adnxs.com/click") ||
			(IsAdFrameUrl(url) &&
				(has("/pcs/click") || has("/click?") ||
					has("/click/") || has("/clickthrough")));
	}

	std::string UrlHost(const std::string& url)
	{
		const size_t scheme = url.find("://");
		const size_t start = (scheme == std::string::npos) ? 0 : scheme + 3;
		const size_t end = url.find_first_of("/?#", start);
		std::string host = (end == std::string::npos)
			? url.substr(start)
			: url.substr(start, end - start);
		const size_t at = host.rfind('@');
		if (at != std::string::npos)
			host.erase(0, at + 1);
		const size_t colon = host.find(':');
		if (colon != std::string::npos)
			host.erase(colon);
		for (char& c : host)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return host;
	}

	/* Last two labels — enough to tell "same publisher" from "third-party ad".
	   wiki.guildwars2.com and www.guildwars2.com both reduce to guildwars2.com. */
	std::string BaseDomain(const std::string& host)
	{
		const size_t last = host.rfind('.');
		if (last == std::string::npos || last == 0)
			return host;
		const size_t prev = host.rfind('.', last - 1);
		if (prev == std::string::npos)
			return host;
		return host.substr(prev + 1);
	}

	bool IsSameSite(const std::string& a, const std::string& b)
	{
		const std::string da = BaseDomain(UrlHost(a));
		const std::string db = BaseDomain(UrlHost(b));
		return !da.empty() && da == db;
	}

	bool IsPromotablePopupUrl(const std::string& url)
	{
		if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
			return false;
		if (IsMediaOrCdnUrl(url) || IsYoutubeHostUrl(url) || IsTwitchHostUrl(url))
			return false;
		return true;
	}

	std::string FrameUrl(cef_frame_t* frame)
	{
		if (!frame || !frame->get_url || !g_userfree_free)
			return {};
		cef_string_userfree_t uf = frame->get_url(frame);
		if (!uf)
			return {};
		const std::string out = CefStringToUtf8(uf);
		g_userfree_free(uf);
		return out;
	}

	std::string MainFrameUrl(cef_browser_t* browser)
	{
		if (!browser || !g_userfree_free)
			return {};
		cef_frame_t* frame = browser->get_main_frame(browser);
		if (!frame)
			return {};
		const std::string out = FrameUrl(frame);
		frame->base.release(&frame->base);
		return out;
	}

	int CEF_CALLBACK OnBeforePopup(
		cef_life_span_handler_t*, cef_browser_t* browser, cef_frame_t* frame, int /*popup_id*/,
		const cef_string_t* target_url, const cef_string_t*, cef_window_open_disposition_t,
		int user_gesture, const cef_popup_features_t*, cef_window_info_t*, cef_client_t**,
		cef_browser_settings_t*, cef_dictionary_value_t**, int*)
	{
		/* Always cancel native popup windows (OSR has no place for them). */
		if (!target_url)
			return 1;

		const std::string url = CefStringToUtf8(target_url);
		const std::string cur = MainFrameUrl(browser);

		NavLog("POPUP gesture=%d fromMain=%d adclick=%d promo=%d\n  url=%s\n  frame=%s\n  page=%s",
			user_gesture,
			frame && frame->is_main && frame->is_main(frame) ? 1 : 0,
			IsAdClickUrl(url) ? 1 : 0, IsPromotablePopupUrl(url) ? 1 : 0,
			url.c_str(), FrameUrl(frame).c_str(), cur.c_str());

		/* Some ad wrappers report the popup as main-frame. The tracker URL is the
		   reliable signal; always preserve it and hand it to the system browser. */
		if (user_gesture && IsAdClickUrl(url))
		{
			OpenExternalUrl(url);
			return 1;
		}

		/* YouTube cannot stay in OSR — open the system browser instead of
		   replacing the guide (that looked like a mid-play refresh). */
		if (IsYoutubeHostUrl(url))
		{
			if (user_gesture)
				OpenExternalUrl(url);
			return 1;
		}

		if (IsTwitchHostUrl(url))
		{
			if (user_gesture)
			{
				OpenExternalUrl(url);
				SetStatus("Twitch needs codecs CEF omits — opened in your browser");
			}
			return 1;
		}

		/* Discord desktop deep links — launch the app (same as game CEF). */
		if (IsDiscordProtocolUrl(url))
		{
			OpenExternalUrl(url);
			return 1;
		}

		/* Google sign-in / consent / captcha — open in the real browser so the
		   login can actually complete (OSR cannot). */
		if (IsExternalSignInUrl(url))
		{
			OpenExternalUrl(url);
			SetStatus("Opening sign-in in your browser (Open Ext)");
			return 1;
		}

		if (IsMediaOrCdnUrl(url) || IsYoutubeHostUrl(cur))
			return 1;

		const bool fromMain = frame && frame->is_main && frame->is_main(frame);
		if (user_gesture && IsPromotablePopupUrl(url))
		{
			/* Ads always target a third-party domain, and some wrappers report the
			   popup as main-frame, so domain is the reliable test rather than which
			   frame asked. Only our own bundled pages and a publisher's own
			   new-window link stay in-tab; everything third-party leaves so the
			   site is credited for the click. */
			const bool localPage = cur.rfind("file://", 0) == 0;
			if (fromMain && (localPage || IsSameSite(url, cur)))
			{
				NavLog("  -> IN-TAB (local=%d sameSite=%d)", localPage ? 1 : 0,
					IsSameSite(url, cur) ? 1 : 0);
				NavigateTo(url.c_str());
			}
			else
			{
				OpenExternalUrl(url);
			}
		}
		else
		{
			NavLog("  -> IGNORED (gesture=%d promo=%d)", user_gesture,
				IsPromotablePopupUrl(url) ? 1 : 0);
		}
		return 1;
	}

	/* Block main-frame navigations that steal the guide after an embed starts. */
	int CEF_CALLBACK OnBeforeBrowse(
		cef_request_handler_t*, cef_browser_t* browser, cef_frame_t* frame,
		cef_request_t* request, int user_gesture, int /*is_redirect*/)
	{
		if (!request || !request->get_url || !g_userfree_free)
			return 0;

		cef_string_userfree_t uf = request->get_url(request);
		if (!uf)
			return 0;
		const std::string url = CefStringToUtf8(uf);
		g_userfree_free(uf);

		const bool isMain = frame && frame->is_main && frame->is_main(frame);
		const bool fromAdFrame = !isMain && IsAdFrameUrl(FrameUrl(frame));

		std::string referrer;
		if (request->get_referrer_url)
		{
			cef_string_userfree_t ruf = request->get_referrer_url(request);
			if (ruf)
			{
				referrer = CefStringToUtf8(ruf);
				g_userfree_free(ruf);
			}
		}
		NavLog("BROWSE gesture=%d isMain=%d adFrame=%d adclick=%d\n  url=%s\n  frame=%s\n  ref=%s",
			user_gesture, isMain ? 1 : 0, fromAdFrame ? 1 : 0, IsAdClickUrl(url) ? 1 : 0,
			url.c_str(), FrameUrl(frame).c_str(), referrer.c_str());

		/* Ads may navigate either their own iframe or the top-level document.
		   A creative targeting _top arrives as a main-frame request, so the frame
		   no longer identifies it as an ad — the referrer still names the ad host,
		   and a publisher's own links never carry one. Anything a tracker, an ad
		   network, or an ad frame is behind leaves with its click URL intact. */
		const bool viaAdReferrer = isMain && IsAdFrameUrl(referrer);
		if (user_gesture &&
			(IsAdClickUrl(url) ||
				(isMain && IsAdFrameUrl(url)) ||
				viaAdReferrer ||
				(fromAdFrame && IsPromotablePopupUrl(url))))
		{
			OpenExternalUrl(url);
			return 1;
		}

		if (!isMain)
			return 0; /* allow iframe / media subloads */

		/* TP watchlist add/remove + any about: builtin — never let Chromium load
		   raw about:live-* (blocked white page). Rewrite to file:// first. */
		{
			if (ConsumeHelperNewTabUrl(url))
				return 1;
			std::string navTo;
			if (ConsumeTpActionUrl(url, &navTo))
			{
				if (!navTo.empty())
					NavigateTo(navTo.c_str());
				return 1;
			}
			if (url.rfind("about:", 0) == 0 && url != "about:blank")
			{
				const std::string resolved = ResolveBuiltinUrl(url.c_str());
				if (!resolved.empty() && resolved != url)
				{
					NavigateTo(resolved.c_str());
					return 1;
				}
				/* Never let Chromium show its white “blocked about:” page. */
				return 1;
			}
		}

		/* Google sign-in / consent / captcha — cannot complete in OSR. Open the
		   real browser (login) or bounce the user out of a /sorry captcha wall.
		   /sorry often arrives as a redirect (no user_gesture), so route it too. */
		if (IsExternalSignInUrl(url))
		{
			OpenExternalUrl(url);
			SetStatus(url.find("/sorry") != std::string::npos
				? "Google blocked the in-game browser — opened in your browser"
				: "Opening sign-in in your browser (Open Ext)");
			return 1;
		}

		if (IsMediaOrCdnUrl(url))
			return 1;

		/* discord:// cannot render in CEF — open the Discord app and CANCEL so the
		   https authorize page stays (navigating here = black page / web login). */
		if (IsDiscordProtocolUrl(url))
		{
			OpenExternalUrl(url);
			return 1;
		}

		const std::string cur = MainFrameUrl(browser);
		if (!IsYoutubeHostUrl(cur) && IsYoutubeHostUrl(url))
		{
			/* "Watch on YouTube" cards use a normal <a href> — open externally. */
			if (user_gesture)
				OpenExternalUrl(url);
			return 1;
		}

		/* Same for the "Watch on Twitch" card — playback can never work here. */
		if (!IsTwitchHostUrl(cur) && IsTwitchHostUrl(url))
		{
			if (user_gesture)
			{
				OpenExternalUrl(url);
				SetStatus("Twitch needs codecs CEF omits — opened in your browser");
			}
			return 1;
		}

		NavLog("  -> IN-TAB (main-frame navigation allowed)");
		return 0;
	}

	int CEF_CALLBACK OnOpenUrlFromTab(
		cef_request_handler_t*, cef_browser_t*, cef_frame_t*,
		const cef_string_t* target_url, cef_window_open_disposition_t, int user_gesture)
	{
		if (target_url && user_gesture)
		{
			const std::string url = CefStringToUtf8(target_url);
			NavLog("OPENFROMTAB gesture=%d promo=%d\n  url=%s", user_gesture,
				IsPromotablePopupUrl(url) ? 1 : 0, url.c_str());
			if (IsPromotablePopupUrl(url))
				OpenExternalUrl(url);
		}
		return 1; /* no new tabs in OSR */
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

	void CEF_CALLBACK GetViewRect(cef_render_handler_t*, cef_browser_t*, cef_rect_t* rect)
	{
		int w = 800, h = 600;
		ViewSize(&w, &h);
		rect->x = 0;
		rect->y = 0;
		rect->width = w;
		rect->height = h;
	}

	/* Desktop monitor + work area for OSR screen metrics. Keep separate from
	   GetViewRect (ImGui panel size) so JS screen.width/height look like a real
	   display — matching view size is a common anti-bot / non-billable signal.
	   device_scale_factor stays 1.0: IPC mouse + OnPaint are already view pixels. */
	void FillDesktopScreenRects(cef_rect_t* monitor, cef_rect_t* work)
	{
		int screenW = GetSystemMetrics(SM_CXSCREEN);
		int screenH = GetSystemMetrics(SM_CYSCREEN);
		if (screenW < 800) screenW = 800;
		if (screenH < 600) screenH = 600;

		monitor->x = 0;
		monitor->y = 0;
		monitor->width = screenW;
		monitor->height = screenH;
		*work = *monitor;

		RECT wa{};
		if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0))
		{
			const int ww = wa.right - wa.left;
			const int wh = wa.bottom - wa.top;
			if (ww >= 640 && wh >= 480)
			{
				work->x = wa.left;
				work->y = wa.top;
				work->width = ww;
				work->height = wh;
			}
		}
	}

	int CEF_CALLBACK GetScreenInfo(cef_render_handler_t*, cef_browser_t*, cef_screen_info_t* info)
	{
		if (!info)
			return 0;
		info->size = sizeof(*info);
		/* Do not use ViewSize here — that is the browser viewport only. */
		info->device_scale_factor = 1.0f;
		info->depth = 32;
		info->depth_per_component = 8;
		info->is_monochrome = 0;
		FillDesktopScreenRects(&info->rect, &info->available_rect);
		return 1;
	}

	bool IsOverPopup(int x, int y)
	{
		return gPopupShow &&
			x >= gPopupRect.x && y >= gPopupRect.y &&
			x < gPopupRect.x + gPopupRect.width &&
			y < gPopupRect.y + gPopupRect.height;
	}

	void ApplyPopupMouseOffset(int& x, int& y)
	{
		if (!IsOverPopup(x, y))
			return;
		x -= gPopupRect.x;
		y -= gPopupRect.y;
	}

	void PublishCompositedFrame(int dirtyX, int dirtyY, int dirtyW, int dirtyH)
	{
		if (!gFramePixels || !gIpc || gViewCacheW <= 0 || gViewCacheH <= 0)
			return;
		if (gViewCacheW > static_cast<int>(kWikiFrameMaxW) ||
			gViewCacheH > static_cast<int>(kWikiFrameMaxH))
			return;

		const uint32_t front = gIpc->frame_front & 1u;
		const uint32_t back = 1u - front;
		if (gIpc->frame_reading == back)
			return;

		uint8_t* dstBase = gFramePixels + static_cast<size_t>(back) * kWikiFrameBytes;
		const size_t rowBytes = static_cast<size_t>(gViewCacheW) * 4;
		for (int y = 0; y < gViewCacheH; ++y)
		{
			std::memcpy(
				dstBase + static_cast<size_t>(y) * kWikiFrameStride,
				gViewCache.data() + static_cast<size_t>(y) * rowBytes,
				rowBytes);
		}

		int ux = dirtyX, uy = dirtyY, uw = dirtyW, uh = dirtyH;
		if (gPopupShow && gPopupCacheW > 0 && gPopupCacheH > 0 &&
			!gPopupCache.empty() && gPopupRect.width > 0 && gPopupRect.height > 0)
		{
			const int pw = gPopupCacheW < gPopupRect.width ? gPopupCacheW : gPopupRect.width;
			const int ph = gPopupCacheH < gPopupRect.height ? gPopupCacheH : gPopupRect.height;
			const size_t popupRow = static_cast<size_t>(gPopupCacheW) * 4;
			const size_t copyBytes = static_cast<size_t>(pw) * 4;
			for (int y = 0; y < ph; ++y)
			{
				const int dy = gPopupRect.y + y;
				if (dy < 0 || dy >= gViewCacheH)
					continue;
				int x0 = 0;
				int dx0 = gPopupRect.x;
				if (dx0 < 0) { x0 = -dx0; dx0 = 0; }
				int span = pw - x0;
				if (dx0 + span > gViewCacheW)
					span = gViewCacheW - dx0;
				if (span <= 0)
					continue;
				std::memcpy(
					dstBase + static_cast<size_t>(dy) * kWikiFrameStride + static_cast<size_t>(dx0) * 4,
					gPopupCache.data() + static_cast<size_t>(y) * popupRow + static_cast<size_t>(x0) * 4,
					static_cast<size_t>(span) * 4);
				(void)copyBytes;
			}
			const int px0 = gPopupRect.x;
			const int py0 = gPopupRect.y;
			const int px1 = gPopupRect.x + gPopupRect.width;
			const int py1 = gPopupRect.y + gPopupRect.height;
			const int vx1 = ux + uw;
			const int vy1 = uy + uh;
			const int nx = px0 < ux ? px0 : ux;
			const int ny = py0 < uy ? py0 : uy;
			uw = (px1 > vx1 ? px1 : vx1) - nx;
			uh = (py1 > vy1 ? py1 : vy1) - ny;
			ux = nx;
			uy = ny;
		}

		if (ux < 0) { uw += ux; ux = 0; }
		if (uy < 0) { uh += uy; uy = 0; }
		if (ux + uw > gViewCacheW) uw = gViewCacheW - ux;
		if (uy + uh > gViewCacheH) uh = gViewCacheH - uy;
		if (uw <= 0 || uh <= 0)
		{
			ux = 0;
			uy = 0;
			uw = gViewCacheW;
			uh = gViewCacheH;
		}

		gIpc->frame_w = static_cast<uint32_t>(gViewCacheW);
		gIpc->frame_h = static_cast<uint32_t>(gViewCacheH);
		gIpc->dirty_x = static_cast<uint32_t>(ux);
		gIpc->dirty_y = static_cast<uint32_t>(uy);
		gIpc->dirty_w = static_cast<uint32_t>(uw);
		gIpc->dirty_h = static_cast<uint32_t>(uh);
		gIpc->frame_front = back;
		MemoryBarrier();
		++gIpc->frame_seq;
	}

	void UnionDirty(int* ux, int* uy, int* uw, int* uh, const cef_rect_t* dirtyRects, size_t dirtyCount, int width, int height)
	{
		*ux = 0;
		*uy = 0;
		*uw = width;
		*uh = height;
		if (dirtyCount == 0 || !dirtyRects)
			return;
		*ux = dirtyRects[0].x;
		*uy = dirtyRects[0].y;
		*uw = dirtyRects[0].width;
		*uh = dirtyRects[0].height;
		for (size_t i = 1; i < dirtyCount; ++i)
		{
			const int x0 = dirtyRects[i].x;
			const int y0 = dirtyRects[i].y;
			const int x1 = x0 + dirtyRects[i].width;
			const int y1 = y0 + dirtyRects[i].height;
			const int rx1 = *ux + *uw;
			const int ry1 = *uy + *uh;
			const int nx = x0 < *ux ? x0 : *ux;
			const int ny = y0 < *uy ? y0 : *uy;
			*uw = (x1 > rx1 ? x1 : rx1) - nx;
			*uh = (y1 > ry1 ? y1 : ry1) - ny;
			*ux = nx;
			*uy = ny;
		}
		if (*ux < 0) { *uw += *ux; *ux = 0; }
		if (*uy < 0) { *uh += *uy; *uy = 0; }
		if (*ux + *uw > width) *uw = width - *ux;
		if (*uy + *uh > height) *uh = height - *uy;
		if (*uw <= 0 || *uh <= 0)
		{
			*ux = 0;
			*uy = 0;
			*uw = width;
			*uh = height;
		}
	}

	void CEF_CALLBACK OnPopupShow(cef_render_handler_t*, cef_browser_t* browser, int show)
	{
		if (!IsActiveBrowser(browser))
			return;
		const bool wasShown = gPopupShow;
		gPopupShow = show != 0;
		if (!gPopupShow)
		{
			/* Arm one-shot swallow for the ghost mouse-up after option pick. */
			if (wasShown && gPopupRect.width > 0 && gPopupRect.height > 0)
			{
				gSwallowPopupMouseUp = true;
				gPopupSwallowRect = gPopupRect;
			}
			gPopupInvalidateOnce = false;
			gPopupCache.clear();
			gPopupCacheW = 0;
			gPopupCacheH = 0;
			gPopupRect = {};
			if (gViewCacheW > 0)
				PublishCompositedFrame(0, 0, gViewCacheW, gViewCacheH);
			return;
		}
		gSwallowPopupMouseUp = false;
		/* Request the first popup paint once — do not invalidate every frame. */
		gPopupInvalidateOnce = true;
	}

	void CEF_CALLBACK OnPopupSize(cef_render_handler_t*, cef_browser_t* browser, const cef_rect_t* rect)
	{
		if (!IsActiveBrowser(browser) || !rect)
			return;
		gPopupRect = *rect;
		if (gPopupRect.width < 0) gPopupRect.width = 0;
		if (gPopupRect.height < 0) gPopupRect.height = 0;
	}

	void CEF_CALLBACK OnPaint(
		cef_render_handler_t*, cef_browser_t* browser, cef_paint_element_type_t type,
		size_t dirtyCount, cef_rect_t const* dirtyRects, const void* buffer, int width, int height)
	{
		if (!buffer || !gFramePixels || !gIpc || width <= 0 || height <= 0)
			return;
		if (!IsActiveBrowser(browser))
			return;

		if (type == PET_POPUP)
		{
			const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
			if (bytes > 8u * 1024u * 1024u)
				return; /* absurd popup — ignore rather than allocate forever */
			gPopupCache.resize(bytes);
			std::memcpy(gPopupCache.data(), buffer, bytes);
			gPopupCacheW = width;
			gPopupCacheH = height;
			if (gPopupShow && gViewCacheW > 0)
			{
				PublishCompositedFrame(
					gPopupRect.x, gPopupRect.y,
					gPopupRect.width > 0 ? gPopupRect.width : width,
					gPopupRect.height > 0 ? gPopupRect.height : height);
			}
			return;
		}

		if (type != PET_VIEW)
			return;
		if (width > static_cast<int>(kWikiFrameMaxW) || height > static_cast<int>(kWikiFrameMaxH))
			return;

		int ux = 0, uy = 0, uw = width, uh = height;
		UnionDirty(&ux, &uy, &uw, &uh, dirtyRects, dirtyCount, width, height);

		const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
		gViewCache.resize(bytes);
		std::memcpy(gViewCache.data(), buffer, bytes);
		gViewCacheW = width;
		gViewCacheH = height;

		PublishCompositedFrame(ux, uy, uw, uh);

		if (gPopupInvalidateOnce && gPopupShow)
		{
			gPopupInvalidateOnce = false;
			cef_browser_host_t* host = browser->get_host(browser);
			if (host && host->invalidate)
			{
				host->invalidate(host, PET_POPUP);
				host->base.release(&host->base);
			}
		}
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
		gActiveSlot = 0;
		gCreateQueueHead = 0;
		gCreateQueueCount = 0;
		gCreateInFlightSlot = -1;
		gCreateInFlightDiscard = false;
		if (gIpc)
		{
			/* Do not create an about:blank browser here — it stays in CEF history
			   and makes Back after a site change show a white page. The addon
			   CREATE_TAB supplies the real start URL. */
			gIpc->tab_mask = 0;
			gIpc->active_tab = 0;
			/* ready = accepting cmds (CREATE_TAB). Must NOT wait for OnAfterCreated:
			   the host only SyncAllToHelper after ready — that would deadlock. */
			gIpc->ready = 1;
			gIpc->alive = GetTickCount();
		}
		SetStatus("Starting…");
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
	DWORD hostPid = 0;
	{
		const std::wstring pidStr = GetArg(L"host-pid", argc, argv);
		if (!pidStr.empty())
			hostPid = static_cast<DWORD>(_wtoi(pidStr.c_str()));
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

	if (hostPid == 0)
		hostPid = GetCurrentProcessId(); /* fallback — still unique vs other clients */

	/* Watchdog: if GW2 dies without a clean QUIT (crash, hard kill), exit too.
	   Orphaned helpers keep the named sections alive and stall the next launch. */
	HANDLE hostProc = nullptr;
	if (hostPid != GetCurrentProcessId())
		hostProc = OpenProcess(SYNCHRONIZE, FALSE, hostPid);

	char ipcName[96]{};
	char frameName[96]{};
	char wakeName[96]{};
	WikiIpcFormatNames(hostPid, ipcName, frameName, wakeName, sizeof(ipcName));

	gMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, ipcName);
	if (!gMap)
		return 4;
	gIpc = static_cast<WikiIpcState*>(MapViewOfFile(gMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(WikiIpcState)));
	if (!gIpc || gIpc->magic != kWikiIpcMagic)
		return 5;

	gFrameMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, frameName);
	if (!gFrameMap)
	{
		SetStatus("Failed to open frame mapping");
		return 9;
	}
	gFramePixels = static_cast<uint8_t*>(MapViewOfFile(gFrameMap, FILE_MAP_ALL_ACCESS, 0, 0, kWikiFrameMapBytes));
	if (!gFramePixels)
	{
		SetStatus("Failed to map frame buffer");
		return 10;
	}
	gIpc->frame_front = 0;
	gIpc->frame_reading = 0xFFFFFFFFu;
	gIpc->dirty_w = kWikiFrameMaxW;
	gIpc->dirty_h = kWikiFrameMaxH;
	gWakeEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, wakeName);

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
	const std::string localesUtf8 = cefDirUtf8 + "\\locales";
	MakeCefString(&settings.locales_dir_path, localesUtf8.c_str());
	/* Chromium profile under %LOCALAPPDATA% — never under Guild Wars 2/addons.
	   Previously %TEMP%, but Windows Storage Sense / Disk Cleanup / CCleaner wipe
	   %TEMP%, discarding Google cookies each session so users hit /sorry/index
	   "unusual traffic". %LOCALAPPDATA% persists like a normal browser profile.
	   Falls back to %TEMP% only if the variable is missing (rare). */
	std::wstring cacheRoot;
	{
		wchar_t local[MAX_PATH]{};
		const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
		if (n > 0 && n < MAX_PATH)
		{
			cacheRoot = local;
			cacheRoot += L"\\GW2-InGame-Helper-Beta";
			CreateDirectoryW(cacheRoot.c_str(), nullptr);
		}
		else
		{
			wchar_t tmp[MAX_PATH]{};
			GetTempPathW(MAX_PATH, tmp);
			cacheRoot = tmp;
		}
	}
	const std::wstring cache = cacheRoot + L"\\cef-cache";
	CreateDirectoryW(cache.c_str(), nullptr);
	const std::string cacheUtf8 = WideToUtf8(cache);
	MakeCefString(&settings.cache_path, cacheUtf8.c_str());
	MakeCefString(&settings.root_cache_path, cacheUtf8.c_str());
	/* Match private CEF Stable 150 — do not spoof an older Chrome major.
	   Trailing product token lets publishers allow/deny this client (see
	   docs/PUBLISHER_ACCESS.md). Keep the token stable once shipped. */
	MakeCefString(&settings.user_agent,
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
		"Chrome/150.0.7871.129 Safari/537.36 GW2-InGame-Helper");

	if (!g_initialize(&mainArgs, &settings, &gApp, nullptr))
	{
		SetStatus("cef_initialize failed");
		ClearCefString(&settings.browser_subprocess_path);
		ClearCefString(&settings.resources_dir_path);
		ClearCefString(&settings.locales_dir_path);
		ClearCefString(&settings.cache_path);
		ClearCefString(&settings.root_cache_path);
		ClearCefString(&settings.user_agent);
		return 7;
	}
	ClearCefString(&settings.browser_subprocess_path);
	ClearCefString(&settings.resources_dir_path);
	ClearCefString(&settings.locales_dir_path);
	ClearCefString(&settings.cache_path);
	ClearCefString(&settings.root_cache_path);
	ClearCefString(&settings.user_agent);

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

		if (hostProc && WaitForSingleObject(hostProc, 0) == WAIT_OBJECT_0)
		{
			gRunning = false;
			break;
		}

		/* Idle: wake on DLL cmds/input, else longer timeout for CEF timers.
		   Never spin at 1–2ms forever — under Proton that can lock the host. */
		const bool inputPending = gIpc && (gIpc->input_read != gIpc->input_write);
		const bool painted = gIpc && gIpc->frame_seq != 0;
		const bool busy = gIpc && gIpc->visible && painted;
		DWORD timeout = 16u;
		if (inputPending)
			timeout = 4u;
		else if (busy)
			timeout = 8u;
		else if (gIpc && gIpc->visible && !painted)
			timeout = 33u; /* create/paint stalled — back off hard */
		HANDLE waits[2]{};
		DWORD waitCount = 0;
		if (gWakeEvent)
			waits[waitCount++] = gWakeEvent;
		if (hostProc)
			waits[waitCount++] = hostProc;
		if (waitCount)
		{
			MsgWaitForMultipleObjects(waitCount, waits, FALSE, timeout, QS_ALLINPUT);
			if (gWakeEvent)
				ResetEvent(gWakeEvent);
		}
		else
			Sleep(timeout);
	}

	for (int i = 0; i < kWikiMaxTabs; ++i)
	{
		if (!gBrowsers[i])
			continue;
		gBrowsers[i]->base.release(&gBrowsers[i]->base);
		gBrowsers[i] = nullptr;
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
	if (gWakeEvent)
		CloseHandle(gWakeEvent);
	if (hostProc)
		CloseHandle(hostProc);
	return 0;
}
