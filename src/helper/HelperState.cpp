/* Helper shared globals + CEF string/URL helpers — HelperDetail. */
#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cwchar>
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

	/* Mirror AddonPaths::EnsureUnder — helper exe cannot link Nexus AddonPaths. */
	std::wstring EnsureHelperUnder(const std::wstring& root, const wchar_t* relative)
	{
		if (root.empty() || !relative || !relative[0])
			return {};
		std::wstring cur = root;
		const wchar_t* p = relative;
		while (*p)
		{
			while (*p == L'\\' || *p == L'/')
				++p;
			if (!*p)
				break;
			const wchar_t* start = p;
			while (*p && *p != L'\\' && *p != L'/')
				++p;
			cur.push_back(L'\\');
			cur.append(start, p);
			CreateDirectoryW(cur.c_str(), nullptr);
		}
		return cur;
	}

	std::wstring HelperPagesDir()
	{
		return EnsureHelperUnder(HelperDir(), L"pages");
	}

	std::wstring HelperCmdsDir()
	{
		return EnsureHelperUnder(HelperDir(), L"cmds");
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
		const std::wstring cmds = HelperCmdsDir();
		const std::wstring pages = HelperPagesDir();
		if (cmds.empty())
			return;
		const std::wstring path = cmds + L"\\live-tp-cmd.txt";
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
		if (!pages.empty())
		{
			DeleteFileW((pages + L"\\live-tp.ok").c_str());
			DeleteFileW((pages + L"\\live-tp.ver").c_str());
		}
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
		else if (std::strncmp(url, "about:craft-plan-", 17) == 0)
		{
			int id = 0;
			for (const char* p = url + 17; *p >= '0' && *p <= '9'; ++p)
				id = id * 10 + (*p - '0');
			if (id > 0)
			{
				const std::wstring cmds = HelperCmdsDir();
				if (!cmds.empty())
				{
					const std::wstring path = cmds + L"\\craft-plan-cmd.txt";
					char line[48];
					std::snprintf(line, sizeof(line), "%d\n", id);
					HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
						nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
					if (h != INVALID_HANDLE_VALUE)
					{
						DWORD written = 0;
						WriteFile(h, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
						CloseHandle(h);
					}
				}
			}
			/* Stay on the current page — DLL Tick opens Account Crafting.
			   Returning about:legendary-vault caused a white page when the
			   vault HTML was missing. */
			return {};
		}
		else if (std::strncmp(url, "about:legendary-vault-item-", 27) == 0 ||
			std::strncmp(url, "about:legendary-vault-sync-", 27) == 0)
		{
			const bool sync = std::strncmp(url, "about:legendary-vault-sync-", 27) == 0;
			int id = 0;
			for (const char* p = url + 27; *p >= '0' && *p <= '9'; ++p)
				id = id * 10 + (*p - '0');
			const std::wstring cmds = HelperCmdsDir();
			const std::wstring pages = HelperPagesDir();
			if (id <= 0 || cmds.empty() || pages.empty())
				return {};
			/* Queue DLL worker; write a dark loading shell so CEF never sees raw about:. */
			{
				const std::wstring cmdPath = cmds + L"\\legendary-detail-cmd.txt";
				char line[64];
				std::snprintf(line, sizeof(line), "%s %d\n", sync ? "sync" : "open", id);
				HANDLE h = CreateFileW(cmdPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
					nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (h != INVALID_HANDLE_VALUE)
				{
					DWORD written = 0;
					WriteFile(h, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
					CloseHandle(h);
				}
			}
			wchar_t name[80];
			std::swprintf(name, 80, L"live-legendary-detail-%d.html", id);
			const std::wstring path = pages + L"\\" + name;
			static const char kShell[] =
				"<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
				"<title>Loading craft tree…</title></head>"
				"<body style=\"margin:0;background:#0b0a10;color:#a1a1aa;"
				"font-family:Segoe UI,sans-serif;padding:2rem\">"
				"<p>Building craft tree (gifts → mats)…</p>"
				"<p style=\"font-size:.85rem;color:#52525b\">This page refreshes when ready.</p>"
				"</body></html>";
			HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (hf != INVALID_HANDLE_VALUE)
			{
				DWORD written = 0;
				WriteFile(hf, kShell, static_cast<DWORD>(sizeof(kShell) - 1), &written, nullptr);
				CloseHandle(hf);
			}
			DeleteFileW((pages + L"\\live-legendary-detail-" + std::to_wstring(id) + L".ok").c_str());
			DeleteFileW((pages + L"\\live-legendary-detail-" + std::to_wstring(id) + L".ver").c_str());
			return WidePathToFileUrl(path);
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
		else if (std::strcmp(url, "about:legendary-vault") == 0)
			fileNameW = L"live-legendary-vault.html";
		else if (std::strcmp(url, "about:cheatsheets-hub") == 0)
			fileNameW = L"live-cheatsheets-hub.html";
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
		else if (std::strcmp(url, "about:gw2-api-check") == 0)
			fileNameW = L"gw2-api-check.html";
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
		const std::wstring pages = HelperPagesDir();
		const std::wstring cmds = HelperCmdsDir();
		if (dir.empty() || pages.empty())
			return {};
		/* Pack sheets live under cheatsheets\; generated HTML under pages/.
		   Prefer the pack path so a stale loading shell cannot win. */
		const std::wstring pathSheets = dir + L"\\cheatsheets\\" + fileNameW;
		const std::wstring pathPages = pages + L"\\" + fileNameW;
		if (GetFileAttributesW(pathSheets.c_str()) != INVALID_FILE_ATTRIBUTES)
			return WidePathToFileUrl(pathSheets);
		if (GetFileAttributesW(pathPages.c_str()) != INVALID_FILE_ATTRIBUTES)
			return WidePathToFileUrl(pathPages);

		/* Ask the DLL to Ensure* + Navigate — never hand CEF a raw about:
		   (blocked → white page). Show a dark loading shell until then. */
		if (!cmds.empty())
		{
			const std::wstring cmdPath = cmds + L"\\open-about-cmd.txt";
			const std::string line = std::string(url) + "\n";
			HANDLE h = CreateFileW(cmdPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
				nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (h != INVALID_HANDLE_VALUE)
			{
				DWORD written = 0;
				WriteFile(h, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
				CloseHandle(h);
			}
		}
		static const char kShell[] =
			"<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
			"<title>Loading…</title></head>"
			"<body style=\"margin:0;background:#0b0a10;color:#a1a1aa;"
			"font-family:Segoe UI,sans-serif;padding:2rem\">"
			"<p>Opening cheat sheet…</p>"
			"</body></html>";
		HANDLE hf = CreateFileW(pathPages.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hf != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			WriteFile(hf, kShell, static_cast<DWORD>(sizeof(kShell) - 1), &written, nullptr);
			CloseHandle(hf);
		}
		if (GetFileAttributesW(pathPages.c_str()) == INVALID_FILE_ATTRIBUTES)
			return {};
		return WidePathToFileUrl(pathPages);
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

	/* Ledger CTAs use ?gw2igh-* on file:// pages — CEF blocks unknown about: before rewrite. */
	bool ConsumeLedgerActionUrl(const std::string& url, std::string* outNavigate)
	{
		if (!outNavigate)
			return false;
		outNavigate->clear();
		if (url.find("live-legendary") == std::string::npos)
			return false;
		size_t q = url.find('?');
		if (q == std::string::npos)
			return false;
		std::string query = url.substr(q + 1);
		const size_t hash = query.find('#');
		if (hash != std::string::npos)
			query.resize(hash);
		const int openId = ParseQueryInt(query, "gw2igh-leg-open");
		const int syncId = ParseQueryInt(query, "gw2igh-leg-sync");
		const int craftId = ParseQueryInt(query, "gw2igh-craft-plan");
		if (openId <= 0 && syncId <= 0 && craftId <= 0)
			return false;
		char about[64];
		if (craftId > 0)
		{
			std::snprintf(about, sizeof(about), "about:craft-plan-%d", craftId);
			(void)ResolveBuiltinUrl(about); /* queues DLL cmd; stay on page */
			return true;
		}
		if (syncId > 0)
			std::snprintf(about, sizeof(about), "about:legendary-vault-sync-%d", syncId);
		else
			std::snprintf(about, sizeof(about), "about:legendary-vault-item-%d", openId);
		*outNavigate = ResolveBuiltinUrl(about);
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
}
