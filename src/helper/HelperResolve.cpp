/* Builtin about: URL resolve + TP/craft cmd queue helpers — HelperDetail. */
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include "HelperInternal.h"

namespace HelperDetail
{
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
			/* Stay on the current page — DLL Tick opens Economy Crafting.
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
				"<body style=\"margin:0;background:#06070a;color:#a8aeb8;"
				"font-family:Segoe UI,sans-serif;padding:2rem\">"
				"<p>Building craft tree (gifts → mats)…</p>"
				"<p style=\"font-size:.85rem;color:#c9a227\">This page refreshes when ready.</p>"
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
		std::wstring dynamicFileName;
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
		else if (std::strcmp(url, "about:browse-hub") == 0)
			fileNameW = L"live-browse-hub.html";
		else if (std::strncmp(url, "about:browse-cat-", 17) == 0)
		{
			dynamicFileName = L"live-browse-cat-";
			for (const char* p = url + 17; *p; ++p)
			{
				if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')
					dynamicFileName.push_back(static_cast<wchar_t>(*p));
			}
			if (dynamicFileName.size() <= 16) /* "live-browse-cat-" only */
				return {};
			dynamicFileName += L".html";
			fileNameW = dynamicFileName.c_str();
		}
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
			fileNameW = L"live-legendary-vault.html"; /* demoted → Ledger */
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

		auto isLoadingShell = [](const std::wstring& path) -> bool {
			HANDLE in = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (in == INVALID_HANDLE_VALUE)
				return false;
			LARGE_INTEGER li{};
			if (!GetFileSizeEx(in, &li) || li.QuadPart <= 0)
			{
				CloseHandle(in);
				return false;
			}
			if (li.QuadPart > 900)
			{
				CloseHandle(in);
				return false;
			}
			char buf[320]{};
			DWORD got = 0;
			const BOOL ok = ReadFile(in, buf, sizeof(buf) - 1, &got, nullptr);
			CloseHandle(in);
			if (!ok || got == 0)
				return true;
			return std::strstr(buf, "Opening cheat sheet") != nullptr
				|| std::strstr(buf, "Opening Legendary Ledger") != nullptr
				|| std::strstr(buf, "Building page") != nullptr;
		};

		/* Real generated pages (Live / home / raid-food). Never treat the
		   loading stub as the sheet — same file:// is a CEF / NavigateSlot no-op. */
		if (GetFileAttributesW(pathPages.c_str()) != INVALID_FILE_ATTRIBUTES &&
			!isLoadingShell(pathPages))
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
		const char* loadMsg = "Opening cheat sheet…";
		if (std::strcmp(url, "about:legendary-vault") == 0 ||
			std::strcmp(url, "about:live-progress") == 0)
			loadMsg = "Opening Legendary Ledger…";
		else if (std::strcmp(url, "about:cheatsheets-hub") == 0 ||
			std::strcmp(url, "about:browse-hub") == 0 ||
			std::strcmp(url, "about:gw2-api-check") == 0 ||
			std::strncmp(url, "about:live-", 11) == 0 ||
			std::strncmp(url, "about:browse-cat-", 17) == 0)
			loadMsg = "Building page…";
		char shellBody[384];
		std::snprintf(shellBody, sizeof(shellBody),
			"<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
			"<title>Loading…</title></head>"
			"<body style=\"margin:0;background:#0b0a10;color:#a1a1aa;"
			"font-family:Segoe UI,sans-serif;padding:2rem\">"
			"<p>%s</p>"
			"</body></html>", loadMsg);
		const std::wstring pathStub = pages + L"\\opening-cheatsheet.html";
		HANDLE hf = CreateFileW(pathStub.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hf != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			WriteFile(hf, shellBody, static_cast<DWORD>(std::strlen(shellBody)), &written, nullptr);
			CloseHandle(hf);
		}
		if (GetFileAttributesW(pathStub.c_str()) == INVALID_FILE_ATTRIBUTES)
			return {};
		return WidePathToFileUrl(pathStub);
	}

} // namespace HelperDetail
