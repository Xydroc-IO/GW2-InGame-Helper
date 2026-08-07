#include "LivePanelsInternal.h"

#include "AccountPad.h"
#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "CraftingData.h"
#include "Globals.h"
#include "LivePanels.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include "WikiBrowserShared.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <windows.h>

namespace LivePanelsDetail
{
bool ParseCraftPlanUrl(const std::string& url, int* idOut)
{
	if (!idOut || url.rfind("about:craft-plan-", 0) != 0)
		return false;
	*idOut = 0;
	for (const char* p = url.c_str() + 17; *p >= '0' && *p <= '9'; ++p)
		*idOut = *idOut * 10 + (*p - '0');
	return *idOut > 0;
}

void QueueCraftPlanCmd(const std::wstring& addonDir, int itemId)
{
	if (addonDir.empty() || itemId <= 0)
		return;
	const std::wstring path = AddonPaths::EnsureUnder(addonDir, L"cmds") + L"\\craft-plan-cmd.txt";
	char line[48];
	std::snprintf(line, sizeof(line), "%d\n", itemId);
	HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return;
	DWORD written = 0;
	WriteFile(h, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
	CloseHandle(h);
}

bool ProcessCraftPlanCmdFile(const std::wstring& addonDir)
{
	const std::wstring path = AddonPaths::EnsureUnder(addonDir, L"cmds") + L"\\craft-plan-cmd.txt";
	const std::string raw = ReadUtf8File(path);
	if (raw.empty())
		return false;
	DeleteFileW(path.c_str());
	int id = 0;
	for (char c : raw)
	{
		if (c >= '0' && c <= '9')
			id = id * 10 + (c - '0');
		else if (id > 0)
			break;
	}
	if (id <= 0)
		return false;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%d", id);
	AccountPad::OpenAndRefresh();
	CraftingData::QueuePlan(buf);
	return true;
}

bool ParseLegendaryItemUrl(const std::string& url, int* idOut, bool* syncOut)
{
	if (!idOut)
		return false;
	*idOut = 0;
	if (syncOut)
		*syncOut = false;
	const char* p = nullptr;
	if (url.rfind("about:legendary-vault-sync-", 0) == 0)
	{
		if (syncOut)
			*syncOut = true;
		p = url.c_str() + 27;
	}
	else if (url.rfind("about:legendary-vault-item-", 0) == 0)
		p = url.c_str() + 27;
	else
		return false;
	for (; *p >= '0' && *p <= '9'; ++p)
		*idOut = *idOut * 10 + (*p - '0');
	return *idOut > 0;
}

bool ProcessLegendaryDetailCmdFile(const std::wstring& addonDir)
{
	const std::wstring path = AddonPaths::EnsureUnder(addonDir, L"cmds") + L"\\legendary-detail-cmd.txt";
	const std::string raw = ReadUtf8File(path);
	if (raw.empty())
		return false;
	DeleteFileW(path.c_str());

	bool any = false;
	size_t i = 0;
	while (i < raw.size())
	{
		while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\r' || raw[i] == '\n' || raw[i] == '\t'))
			++i;
		if (i >= raw.size())
			break;
		const size_t lineStart = i;
		while (i < raw.size() && raw[i] != '\n' && raw[i] != '\r')
			++i;
		std::string line = raw.substr(lineStart, i - lineStart);
		bool sync = false;
		const char* num = line.c_str();
		if (line.rfind("sync ", 0) == 0)
		{
			sync = true;
			num = line.c_str() + 5;
		}
		else if (line.rfind("open ", 0) == 0)
			num = line.c_str() + 5;
		int id = 0;
		for (const char* p = num; *p >= '0' && *p <= '9'; ++p)
			id = id * 10 + (*p - '0');
		if (id <= 0)
			continue;

		char stem[64];
		std::snprintf(stem, sizeof(stem), "live-legendary-detail-%d", id);
		/* Sync rebuilds; open reuses the cached craft tree when ready. */
		if (sync)
		{
			char craftStem[64];
			std::snprintf(craftStem, sizeof(craftStem), "live-leg-craft-%d", id);
			DeleteFileW(StemPath(addonDir, stem, L".html").c_str());
			DeleteFileW(StemPath(addonDir, stem, L".ver").c_str());
			DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());
			DeleteFileW(StemPath(addonDir, craftStem, L".json").c_str());
			DeleteFileW(StemPath(addonDir, "live-acc-armory", L".json").c_str());
			DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ver").c_str());
			DeleteFileW(StemPath(addonDir, "live-legendary-vault", L".ok").c_str());
		}
		else if (PanelReady(addonDir, stem))
		{
			const std::string fileUrl = PathToFileUrl(StemPath(addonDir, stem, L".html"));
			if (!fileUrl.empty())
			{
				WikiBrowser::Navigate(fileUrl);
				any = true;
			}
			continue;
		}
		char title[96];
		std::snprintf(title, sizeof(title), "Legendary craft #%d", id);
		const std::string fileUrl = EnsurePanel(addonDir, stem, LiveAsyncJob::LegendaryDetail,
			"GW2 Legendary Ledger", title, id);
		if (!fileUrl.empty())
		{
			WikiBrowser::Navigate(fileUrl);
			any = true;
		}
	}
	return any;
}

bool ProcessOpenAboutCmdFile(const std::wstring& addonDir)
{
	const std::wstring path = AddonPaths::EnsureUnder(addonDir, L"cmds") + L"\\open-about-cmd.txt";
	const std::string raw = ReadUtf8File(path);
	if (raw.empty())
		return false;
	DeleteFileW(path.c_str());

	bool any = false;
	size_t i = 0;
	while (i < raw.size())
	{
		while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\r' || raw[i] == '\n' || raw[i] == '\t'))
			++i;
		if (i >= raw.size())
			break;
		size_t start = i;
		while (i < raw.size() && raw[i] != '\r' && raw[i] != '\n')
			++i;
		std::string line = raw.substr(start, i - start);
		while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		if (line.rfind("about:", 0) != 0 || line == "about:blank")
			continue;
		WikiBrowser::Navigate(line);
		any = true;
	}
	return any;
}

void InvalidateBrowseHubCaches(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return;
	/* Never delete live-browse-*.html while CEF may still display it — that
	   surfaces Chromium's "Can't find the page" (ERR_FILE_NOT_FOUND). Stamp
	   only; EnsurePanel rewrites via tmp+replace. */
	DeleteFileW(StemPath(addonDir, "live-browse-hub", L".ver").c_str());
	DeleteFileW(StemPath(addonDir, "live-browse-hub", L".ok").c_str());
	const std::wstring pages = AddonPaths::EnsureUnder(addonDir, L"pages");
	static const wchar_t* kPats[] = {
		L"\\live-browse-cat-*.ok",
		L"\\live-browse-cat-*.ver",
	};
	for (const wchar_t* pat : kPats)
	{
		WIN32_FIND_DATAW fd = {};
		HANDLE h = FindFirstFileW((pages + pat).c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			continue;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			DeleteFileW((pages + L"\\" + fd.cFileName).c_str());
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}
}

void InvalidateBrowseFavCaches(const std::wstring& addonDir, const char* categoryStem)
{
	if (addonDir.empty())
		return;
	/* Stamp-only — keep .html so CEF does not hit ERR_FILE_NOT_FOUND. */
	DeleteFileW(StemPath(addonDir, "live-browse-hub", L".ver").c_str());
	DeleteFileW(StemPath(addonDir, "live-browse-hub", L".ok").c_str());
	if (!categoryStem || !categoryStem[0])
		return;
	if (std::strncmp(categoryStem, "live-browse-cat-", 16) != 0)
		return;
	DeleteFileW(StemPath(addonDir, categoryStem, L".ver").c_str());
	DeleteFileW(StemPath(addonDir, categoryStem, L".ok").c_str());
}

bool ProcessOpenSiteCmdFile(const std::wstring& addonDir)
{
	const std::wstring path = AddonPaths::EnsureUnder(
		addonDir.empty() ? AddonPaths::DataDir() : addonDir, L"cmds") +
		L"\\open-site-cmd.txt";
	const std::string raw = ReadUtf8File(path);
	if (raw.empty())
		return false;
	DeleteFileW(path.c_str());

	bool any = false;
	size_t i = 0;
	while (i < raw.size())
	{
		while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\r' || raw[i] == '\n' || raw[i] == '\t'))
			++i;
		if (i >= raw.size())
			break;
		size_t start = i;
		while (i < raw.size() && raw[i] != '\r' && raw[i] != '\n')
			++i;
		std::string line = raw.substr(start, i - start);
		while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		const char* id = nullptr;
		if (line.rfind("open ", 0) == 0)
			id = line.c_str() + 5;
		else if (!line.empty() && line.find(' ') == std::string::npos)
			id = line.c_str();
		if (!id || !id[0] || Sites::IndexOfId(id) < 0)
			continue;
		if (BrowserTabs::OpenNew(id, true) < 0)
		{
			BrowserTabs::OpenInActive(id, true);
			WikiBrowserDetail::SetLocalStatus("Tab limit reached — opened in this tab");
		}
		else
			WikiBrowserDetail::SetLocalStatus("Opened in a new tab");
		any = true;
	}
	return any;
}

bool ProcessFavCmdFile(const std::wstring& addonDir)
{
	const std::wstring path = AddonPaths::EnsureUnder(addonDir.empty() ? AddonPaths::DataDir() : addonDir,
		L"cmds") + L"\\fav-cmd.txt";
	const std::string raw = ReadUtf8File(path);
	if (raw.empty())
		return false;
	DeleteFileW(path.c_str());

	bool changed = false;
	size_t i = 0;
	while (i < raw.size())
	{
		while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\r' || raw[i] == '\n' || raw[i] == '\t'))
			++i;
		if (i >= raw.size())
			break;
		size_t start = i;
		while (i < raw.size() && raw[i] != '\r' && raw[i] != '\n')
			++i;
		std::string line = raw.substr(start, i - start);
		while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		if (line.rfind("folder-create ", 0) == 0)
		{
			const char* name = line.c_str() + 14;
			if (name[0] && Sites::CreateFavoriteFolder(name))
				changed = true;
			continue;
		}
		if (line.rfind("folder-move ", 0) == 0)
		{
			const char* p = line.c_str() + 12;
			while (*p == ' ' || *p == '\t')
				++p;
			const char* idStart = p;
			while (*p && *p != ' ' && *p != '\t')
				++p;
			std::string siteId(idStart, p);
			while (*p == ' ' || *p == '\t')
				++p;
			if (siteId.empty() || !*p)
				continue;
			char* end = nullptr;
			const long folderId = std::strtol(p, &end, 10);
			if (end == p || folderId < 0 || folderId > 1000000)
				continue;
			if (Sites::IndexOfId(siteId.c_str()) < 0)
				continue;
			if (!Sites::IsFavorite(siteId.c_str()))
				continue;
			if (Sites::SetFavoriteFolder(siteId.c_str(), static_cast<int>(folderId)))
				changed = true;
			continue;
		}
		if (line.rfind("folder-delete ", 0) == 0)
		{
			const char* p = line.c_str() + 14;
			while (*p == ' ' || *p == '\t')
				++p;
			char* end = nullptr;
			const long folderId = std::strtol(p, &end, 10);
			if (end == p || folderId <= 0 || folderId > 1000000)
				continue;
			if (Sites::DeleteFavoriteFolder(static_cast<int>(folderId)))
				changed = true;
			continue;
		}
		const char* id = nullptr;
		if (line.rfind("toggle ", 0) == 0)
			id = line.c_str() + 7;
		if (!id || !id[0])
			continue;
		if (Sites::IndexOfId(id) < 0)
			continue;
		(void)Sites::ToggleFavorite(id);
		changed = true;
	}
	if (changed)
	{
		Settings::SaveNow();
		/* Rebuild Browse hub if open so unfavorited tiles disappear without a
		   manual Reload. Category pages keep in-page star JS only. */
		LivePanels::NotifyFavoritesChanged();
	}
	return changed;
}
} // namespace LivePanelsDetail
