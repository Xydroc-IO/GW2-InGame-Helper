/* Live panel query IPC: TP / ledger / browse hub + Navigate — HelperDetail. */
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "WikiIpc.h"
#include "HelperInternal.h"

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/internal/cef_string.h"

namespace HelperDetail
{
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
		const bool vaultRefresh = query.find("gw2igh-leg-vault=") != std::string::npos;
		if (openId <= 0 && syncId <= 0 && craftId <= 0 && !vaultRefresh)
			return false;
		char about[64];
		if (vaultRefresh)
		{
			*outNavigate = ResolveBuiltinUrl("about:legendary-vault");
			return true;
		}
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

	void AppendCmdLine(const std::wstring& fileName, const std::string& line)
	{
		const std::wstring cmds = HelperCmdsDir();
		if (cmds.empty() || line.empty())
			return;
		const std::wstring path = cmds + L"\\" + fileName;
		HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return;
		DWORD written = 0;
		WriteFile(h, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
		CloseHandle(h);
	}

	std::string ParseQueryValue(const std::string& query, const char* key)
	{
		std::string pat = key;
		pat += '=';
		size_t p = query.find(pat);
		if (p == std::string::npos)
			return {};
		p += pat.size();
		size_t end = p;
		while (end < query.size() && query[end] != '&' && query[end] != '#')
			++end;
		return query.substr(p, end - p);
	}

	/* Map live-browse-*.html file URL → about: for DLL EnsurePanel refresh. */
	std::string AboutFromBrowseFileUrl(const std::string& url)
	{
		const size_t hub = url.find("live-browse-hub.html");
		if (hub != std::string::npos)
			return "about:browse-hub";
		const size_t cat = url.find("live-browse-cat-");
		if (cat == std::string::npos)
			return {};
		size_t start = cat + 15; /* strlen("live-browse-cat-") */
		size_t end = start;
		while (end < url.size())
		{
			const char c = url[end];
			if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')
				++end;
			else
				break;
		}
		if (end <= start)
			return {};
		return std::string("about:browse-cat-") + url.substr(start, end - start);
	}

	void InvalidateHelperBrowseCaches()
	{
		const std::wstring pages = HelperPagesDir();
		if (pages.empty())
			return;
		DeleteFileW((pages + L"\\live-browse-hub.ok").c_str());
		DeleteFileW((pages + L"\\live-browse-hub.ver").c_str());
		DeleteFileW((pages + L"\\live-browse-hub.html").c_str());
	}

	void InvalidateHelperBrowsePage(const std::string& fileUrl)
	{
		InvalidateHelperBrowseCaches();
		const std::wstring pages = HelperPagesDir();
		if (pages.empty())
			return;
		const std::string about = AboutFromBrowseFileUrl(fileUrl);
		if (about.rfind("about:browse-cat-", 0) != 0)
			return;
		const std::string stem = std::string("live-browse-cat-") + about.substr(17);
		std::wstring wstem;
		wstem.reserve(stem.size());
		for (char c : stem)
			wstem.push_back(static_cast<wchar_t>(c));
		DeleteFileW((pages + L"\\" + wstem + L".html").c_str());
		DeleteFileW((pages + L"\\" + wstem + L".ok").c_str());
		DeleteFileW((pages + L"\\" + wstem + L".ver").c_str());
	}

	/* Browse hub: open site / about drill-down / favorite toggle (file:// query IPC). */
	bool ConsumeBrowseHubActionUrl(const std::string& url, std::string* outNavigate)
	{
		if (!outNavigate)
			return false;
		outNavigate->clear();
		if (url.find("live-browse-") == std::string::npos &&
			url.find("live-cheatsheets-hub") == std::string::npos)
			return false;
		size_t q = url.find('?');
		if (q == std::string::npos)
			return false;
		std::string query = url.substr(q + 1);
		const size_t hash = query.find('#');
		if (hash != std::string::npos)
			query.resize(hash);

		const std::string openSite = ParseQueryValue(query, "gw2igh-open-site");
		const std::string aboutKey = ParseQueryValue(query, "gw2igh-about");
		const std::string favId = ParseQueryValue(query, "gw2igh-fav-toggle");
		const std::string folderCreateEnc = ParseQueryValue(query, "gw2igh-fav-folder-create");
		const std::string folderMoveId = ParseQueryValue(query, "gw2igh-fav-folder-move");
		const std::string folderMoveTo = ParseQueryValue(query, "to");
		const std::string folderDeleteId = ParseQueryValue(query, "gw2igh-fav-folder-delete");
		if (openSite.empty() && aboutKey.empty() && favId.empty() &&
			folderCreateEnc.empty() && folderMoveId.empty() && folderDeleteId.empty())
			return false;

		if (!openSite.empty())
		{
			/* Catalog id only — alphanumeric / - _ */
			for (char c : openSite)
			{
				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
					(c >= '0' && c <= '9') || c == '-' || c == '_'))
					return false;
			}
			QueueOpenSiteInAddonTab(openSite);
			return true;
		}
		if (!aboutKey.empty())
		{
			for (char c : aboutKey)
			{
				if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
					return false;
			}
			if (aboutKey != "browse-hub" && aboutKey.rfind("browse-cat-", 0) != 0)
				return false;
			AppendCmdLine(L"open-about-cmd.txt", std::string("about:") + aboutKey + "\n");
			SetStatus("Opening…");
			return true;
		}
		if (!folderCreateEnc.empty())
		{
			std::string name = UrlDecodeQueryValue(folderCreateEnc);
			/* Fold printable name; reject control / path separators. */
			std::string cleaned;
			cleaned.reserve(name.size());
			for (unsigned char c : name)
			{
				if (c < 0x20 || c == 0x7f || c == '/' || c == '\\' || c == '\'' || c == '"')
					continue;
				cleaned.push_back(static_cast<char>(c));
				if (cleaned.size() >= 47)
					break;
			}
			while (!cleaned.empty() && (cleaned.front() == ' ' || cleaned.front() == '\t'))
				cleaned.erase(cleaned.begin());
			while (!cleaned.empty() && (cleaned.back() == ' ' || cleaned.back() == '\t'))
				cleaned.pop_back();
			if (cleaned.empty())
			{
				SetStatus("Folder name required");
				return true;
			}
			{
				static std::string sLastFolderName;
				static DWORD sLastFolderMs = 0;
				const DWORD now = GetTickCount();
				if (cleaned == sLastFolderName && sLastFolderMs != 0 && (now - sLastFolderMs) < 400u)
					return true;
				sLastFolderName = cleaned;
				sLastFolderMs = now;
			}
			AppendCmdLine(L"fav-cmd.txt", std::string("folder-create ") + cleaned + "\n");
			SetStatus("Creating folder…");
			return true;
		}
		if (!folderMoveId.empty())
		{
			for (char c : folderMoveId)
			{
				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
					(c >= '0' && c <= '9') || c == '-' || c == '_'))
					return false;
			}
			int toFolder = -1;
			if (!folderMoveTo.empty())
			{
				toFolder = 0;
				for (char c : folderMoveTo)
				{
					if (c < '0' || c > '9')
					{
						toFolder = -1;
						break;
					}
					toFolder = toFolder * 10 + (c - '0');
					if (toFolder > 1000000)
					{
						toFolder = -1;
						break;
					}
				}
			}
			if (toFolder < 0)
			{
				SetStatus("Invalid folder");
				return true;
			}
			{
				static std::string sLastMoveKey;
				static DWORD sLastMoveMs = 0;
				const DWORD now = GetTickCount();
				const std::string key = folderMoveId + ":" + std::to_string(toFolder);
				if (key == sLastMoveKey && sLastMoveMs != 0 && (now - sLastMoveMs) < 400u)
					return true;
				sLastMoveKey = key;
				sLastMoveMs = now;
			}
			AppendCmdLine(L"fav-cmd.txt",
				std::string("folder-move ") + folderMoveId + " " + std::to_string(toFolder) + "\n");
			SetStatus("Moving favorite…");
			return true;
		}
		if (!folderDeleteId.empty())
		{
			int folderId = 0;
			for (char c : folderDeleteId)
			{
				if (c < '0' || c > '9')
				{
					folderId = -1;
					break;
				}
				folderId = folderId * 10 + (c - '0');
				if (folderId > 1000000)
				{
					folderId = -1;
					break;
				}
			}
			if (folderId <= 0)
			{
				SetStatus("Invalid folder");
				return true;
			}
			{
				static int sLastDelId = -1;
				static DWORD sLastDelMs = 0;
				const DWORD now = GetTickCount();
				if (folderId == sLastDelId && sLastDelMs != 0 && (now - sLastDelMs) < 400u)
					return true;
				sLastDelId = folderId;
				sLastDelMs = now;
			}
			AppendCmdLine(L"fav-cmd.txt",
				std::string("folder-delete ") + std::to_string(folderId) + "\n");
			SetStatus("Deleting folder…");
			return true;
		}
		/* fav toggle — queue DLL only; do NOT delete/rebuild the open page.
		   Wiki category HTML is ~1MB; wipe+EnsurePanel races CEF and logs
		   "Failed to write Live panel HTML". Update the star in-page instead. */
		for (char c : favId)
		{
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_'))
				return false;
		}
		/* CEF can fire OnBeforeBrowse twice for one click — debounce. */
		{
			static std::string sLastFavId;
			static DWORD sLastFavMs = 0;
			const DWORD now = GetTickCount();
			if (favId == sLastFavId && sLastFavMs != 0 && (now - sLastFavMs) < 400u)
				return true;
			sLastFavId = favId;
			sLastFavMs = now;
		}
		AppendCmdLine(L"fav-cmd.txt", std::string("toggle ") + favId + "\n");
		if (gActiveSlot >= 0 && gActiveSlot < kWikiMaxTabs && gBrowsers[gActiveSlot] &&
			gBrowsers[gActiveSlot]->get_main_frame)
		{
			cef_frame_t* frame = gBrowsers[gActiveSlot]->get_main_frame(gBrowsers[gActiveSlot]);
			if (frame && frame->execute_java_script)
			{
				char js[768];
				std::snprintf(js, sizeof(js),
					"(function(){"
					"var a=document.querySelector('a.star[href*=\"gw2igh-fav-toggle=%s\"]');"
					"if(!a)return;"
					"var on=a.classList.toggle('on');"
					"a.textContent=on?'\\u2605':'\\u2606';"
					"a.title=on?'Remove favorite':'Add favorite';"
					"if(!on){var w=a.closest('.tile-wrap');if(w)w.remove();}"
					"})();",
					favId.c_str());
				cef_string_t code{};
				MakeCefString(&code, js);
				frame->execute_java_script(frame, &code, nullptr, 0);
				ClearCefString(&code);
				frame->base.release(&frame->base);
			}
		}
		SetStatus("Favorites updated");
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

} // namespace HelperDetail
