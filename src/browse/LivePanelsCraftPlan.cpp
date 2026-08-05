#include "LivePanelsInternal.h"

#include "AddonPaths.h"
#include "CraftingData.h"
#include "Globals.h"
#include "WikiBrowser.h"

#include <cstdio>
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
	G::ShowAccount = true;
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
		if (sync)
		{
			char craftStem[64];
			std::snprintf(craftStem, sizeof(craftStem), "live-leg-craft-%d", id);
			DeleteFileW(StemPath(addonDir, stem, L".html").c_str());
			DeleteFileW(StemPath(addonDir, stem, L".ver").c_str());
			DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());
			DeleteFileW(StemPath(addonDir, craftStem, L".json").c_str());
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
} // namespace LivePanelsDetail
