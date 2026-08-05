#include "LivePanelsInternal.h"

#include "LivePanels.h"
#include "LivePanelsBuild.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsDetail
{
LiveAsyncState gAsync;

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

std::string PathToFileUrl(const std::wstring& path)
{
	std::string utf8 = WideToUtf8(path);
	for (char& c : utf8)
	{
		if (c == '\\')
			c = '/';
	}
	if (utf8.size() >= 2 && utf8[1] == ':')
		return std::string("file:///") + utf8;
	return std::string("file://") + utf8;
}

std::wstring StemPath(const std::wstring& addonDir, const char* stem, const wchar_t* ext)
{
	const std::wstring base = (ext && wcscmp(ext, L".json") == 0)
		? AddonPaths::EnsureUnder(addonDir, L"live\\cache")
		: AddonPaths::EnsureUnder(addonDir, L"pages");
	std::wstring p = base + L"\\";
	for (const char* s = stem; *s; ++s)
		p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*s)));
	p += ext;
	return p;
}

bool FileFresh(const std::wstring& path, DWORD ttlSec)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	FILETIME ft{};
	const BOOL ok = GetFileTime(h, nullptr, nullptr, &ft);
	CloseHandle(h);
	if (!ok)
		return false;
	ULARGE_INTEGER u{};
	u.LowPart = ft.dwLowDateTime;
	u.HighPart = ft.dwHighDateTime;
	FILETIME nowFt{};
	GetSystemTimeAsFileTime(&nowFt);
	ULARGE_INTEGER n{};
	n.LowPart = nowFt.dwLowDateTime;
	n.HighPart = nowFt.dwHighDateTime;
	const ULONGLONG age100ns = (n.QuadPart > u.QuadPart) ? (n.QuadPart - u.QuadPart) : 0;
	const ULONGLONG ageSec = age100ns / 10000000ull;
	return ageSec <= ttlSec;
}

bool WriteUtf8File(const std::wstring& path, const std::string& data)
{
	/* Write via .tmp then replace — CEF often holds the live HTML open; exclusive
	   CREATE_ALWAYS on the real path fails under Wine and surfaces as
	   "Failed to write Live panel HTML". */
	const std::wstring tmp = path + L".tmp";
	HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
	FlushFileBuffers(h);
	CloseHandle(h);
	if (!ok || written != data.size())
	{
		DeleteFileW(tmp.c_str());
		return false;
	}
	if (MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return true;
	/* Fallback: try direct open with share. */
	DeleteFileW(tmp.c_str());
	h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	written = 0;
	const BOOL ok2 = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
	CloseHandle(h);
	return ok2 && written == data.size();
}

std::string ReadUtf8File(const std::wstring& path)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return {};
	LARGE_INTEGER sz{};
	if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 12 * 1024 * 1024)
	{
		CloseHandle(h);
		return {};
	}
	std::string out(static_cast<size_t>(sz.QuadPart), '\0');
	DWORD read = 0;
	const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
	CloseHandle(h);
	if (!ok)
		return {};
	out.resize(read);
	return out;
}

namespace
{
	void ParseIdList(const char* csv, std::vector<int>& out, size_t maxN)
	{
		if (!csv || !csv[0])
			return;
		const char* p = csv;
		while (*p && out.size() < maxN)
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t')
				++p;
			if (!*p)
				break;
			int v = 0;
			bool any = false;
			while (*p >= '0' && *p <= '9')
			{
				any = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (any && v > 0)
			{
				bool dup = false;
				for (int x : out)
				{
					if (x == v) { dup = true; break; }
				}
				if (!dup)
					out.push_back(v);
			}
			while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t')
				++p;
		}
	}

	bool WatchlistContains(const std::vector<int>& ids, int id)
	{
		for (int x : ids)
			if (x == id) return true;
		return false;
	}

	void SerializeTpWatchIds(const std::vector<int>& ids)
	{
		std::string s;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) s += ',';
			s += std::to_string(ids[i]);
		}
		if (s.size() >= sizeof(G::TpWatchIds))
			s.resize(sizeof(G::TpWatchIds) - 1);
		std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", s.c_str());
	}
} // namespace

bool MutateTpWatchlist(const char* op, int id)
{
	if (id <= 0 || !op || !op[0])
		return false;
	std::vector<int> ids;
	ParseIdList(G::TpWatchIds, ids, 120);
	if (std::strcmp(op, "add") == 0)
	{
		if (WatchlistContains(ids, id))
			return false;
		if (ids.size() >= 120)
			return false;
		ids.push_back(id);
	}
	else if (std::strcmp(op, "remove") == 0)
	{
		bool found = false;
		std::vector<int> next;
		next.reserve(ids.size());
		for (int x : ids)
		{
			if (x == id) { found = true; continue; }
			next.push_back(x);
		}
		if (!found)
			return false;
		ids.swap(next);
	}
	else
		return false;
	SerializeTpWatchIds(ids);
	Settings::SetDirty();
	LivePanels::InvalidateTpCache(AddonPaths::DataDir());
	return true;
}

bool ProcessTpWatchCmdFile(const std::wstring& addonDir)
{
	const std::wstring path = AddonPaths::EnsureUnder(addonDir, L"cmds") + L"\\live-tp-cmd.txt";
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
		const size_t lineStart = i;
		while (i < raw.size() && raw[i] != '\n' && raw[i] != '\r')
			++i;
		std::string line = raw.substr(lineStart, i - lineStart);
		const char* op = nullptr;
		const char* num = nullptr;
		if (line.rfind("add ", 0) == 0)
		{
			op = "add";
			num = line.c_str() + 4;
		}
		else if (line.rfind("remove ", 0) == 0)
		{
			op = "remove";
			num = line.c_str() + 7;
		}
		if (!op || !num)
			continue;
		int id = 0;
		for (const char* p = num; *p >= '0' && *p <= '9'; ++p)
			id = id * 10 + (*p - '0');
		if (MutateTpWatchlist(op, id))
			changed = true;
	}
	return changed;
}

bool ParseTpWatchMutateUrl(const std::string& url, const char** opOut, int* idOut)
{
	if (url.rfind("about:live-tp-add-", 0) == 0)
	{
		*opOut = "add";
		*idOut = 0;
		for (const char* p = url.c_str() + 18; *p >= '0' && *p <= '9'; ++p)
			*idOut = *idOut * 10 + (*p - '0');
		return *idOut > 0;
	}
	if (url.rfind("about:live-tp-remove-", 0) == 0)
	{
		*opOut = "remove";
		*idOut = 0;
		for (const char* p = url.c_str() + 21; *p >= '0' && *p <= '9'; ++p)
			*idOut = *idOut * 10 + (*p - '0');
		return *idOut > 0;
	}
	return false;
}

std::string OfflineShellHtml(const char* title, const char* heading, const char* note)
{
	std::string body = "<section class=\"block\"><div class=\"head\"><h2>Loading Live data…</h2></div><div class=\"body\">";
	body += "<p class=\"note\">";
	body += note;
	body += "</p>";
	body += "<ul class=\"rows\">";
	body += "<li><a class=\"link\" href=\"about:daily-weekly\">Open offline Daily / Weekly checklist</a></li>";
	body += "<li><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Wizard%27s_Vault/Easy_objectives\">Wiki — Easy Vault objectives</a></li>";
	body += "<li><a class=\"link\" href=\"https://gw2timer.com/\">GW2Timer</a></li>";
	body += "</ul></div></section>\n";
	return LivePanelsBuild::BuildPage(title, "GW2 In-Game Helper · Live", heading,
		"Loading in the background so the game stays smooth…",
		nullptr, body);
}

bool VerMatches(const std::wstring& verPath)
{
	std::string v = ReadUtf8File(verPath);
	while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' '))
		v.pop_back();
	return v == kPanelVer;
}

bool PanelReady(const std::wstring& addonDir, const char* stem)
{
	const std::wstring okPath = StemPath(addonDir, stem, L".ok");
	return GetFileAttributesW(okPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool StemIsRunningOrQueued(const std::string& stem)
{
	for (const std::string& s : gAsync.runningStems)
		if (s == stem) return true;
	for (LiveAsyncJob* j : gAsync.queue)
		if (j && j->stem == stem) return true;
	return false;
}

DWORD WINAPI LiveWorkerProc(void* param)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	LiveAsyncJob* job = static_cast<LiveAsyncJob*>(param);
	std::string html;
	if (job->kind == LiveAsyncJob::Dailies)
		html = LivePanelsBuild::BuildDailiesHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::News)
		html = LivePanelsBuild::BuildNewsHtml();
	else if (job->kind == LiveAsyncJob::Fashion)
		html = LivePanelsBuild::BuildFashionHtml(job->addonDir);
	else if (job->kind == LiveAsyncJob::Tp)
		html = LivePanelsBuild::BuildTpHtml(job->tpWatchIds.c_str(), true);
	else if (job->kind == LiveAsyncJob::ApiCheck)
		html = LivePanelsBuild::BuildApiCheckHtml(job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::LegendaryLedger)
		html = LivePanelsBuild::BuildLegendaryLedgerHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::LegendaryDetail)
		html = LivePanelsBuild::BuildLegendaryDetailHtml(job->addonDir, job->apiKey.c_str(),
			job->itemId);
	else if (job->kind == LiveAsyncJob::CheatSheetsHub)
		html = LivePanelsBuild::BuildCheatSheetsHubHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::BrowseHub)
		html = LivePanelsBuild::BuildBrowseHubHtml(job->addonDir, job->apiKey.c_str());
	else if (job->kind == LiveAsyncJob::BrowseCategory)
	{
		const char* slug = nullptr;
		static const char kPrefix[] = "live-browse-cat-";
		if (job->stem.rfind(kPrefix, 0) == 0)
			slug = job->stem.c_str() + (sizeof(kPrefix) - 1);
		const char* cat = LivePanelsBuild::BrowseCategoryFromSlug(slug);
		html = LivePanelsBuild::BuildBrowseCategoryHtml(job->addonDir, cat ? cat : "");
	}
	else
		html = LivePanelsBuild::BuildProgressHtml(job->addonDir, job->apiKey.c_str());

	/* Write on the worker — never dump multi-KB HTML on the game/UI thread. */
	std::string fileUrl;
	bool accept = false;
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		accept = (job->generation == gAsync.generation && !html.empty());
	}
	if (accept)
	{
		const std::wstring htmlPath = StemPath(job->addonDir, job->stem.c_str(), L".html");
		const std::wstring verPath = StemPath(job->addonDir, job->stem.c_str(), L".ver");
		const std::wstring okPath = StemPath(job->addonDir, job->stem.c_str(), L".ok");
		if (WriteUtf8File(htmlPath, html))
		{
			WriteUtf8File(verPath, kPanelVer);
			WriteUtf8File(okPath, "1");
			fileUrl = PathToFileUrl(htmlPath);
		}
	}

	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		if (!fileUrl.empty())
		{
			LiveReadyNav nav;
			nav.stem = job->stem;
			nav.fileUrl = std::move(fileUrl);
			gAsync.readyNav.push_back(std::move(nav));
		}
		for (size_t i = 0; i < gAsync.runningStems.size(); ++i)
		{
			if (gAsync.runningStems[i] == job->stem)
			{
				gAsync.runningStems.erase(gAsync.runningStems.begin() +
					static_cast<std::ptrdiff_t>(i));
				break;
			}
		}
		PumpLiveQueueUnlocked();
	}
	delete job;
	return 0;
}

void PumpLiveQueueUnlocked()
{
	while (static_cast<int>(gAsync.runningStems.size()) < kMaxLiveWorkers &&
		!gAsync.queue.empty())
	{
		LiveAsyncJob* job = gAsync.queue.front();
		gAsync.queue.pop_front();
		if (!job)
			continue;
		if (job->generation != gAsync.generation)
		{
			delete job;
			continue;
		}
		gAsync.runningStems.push_back(job->stem);
		HANDLE th = CreateThread(nullptr, 0, LiveWorkerProc, job, 0, nullptr);
		if (!th)
		{
			gAsync.runningStems.pop_back();
			delete job;
			continue;
		}
		gAsync.joinable.push_back(th);
	}
}

void ReapJoinableUnlocked()
{
	std::vector<HANDLE> keep;
	keep.reserve(gAsync.joinable.size());
	for (HANDLE th : gAsync.joinable)
	{
		if (!th)
			continue;
		if (WaitForSingleObject(th, 0) == WAIT_OBJECT_0)
			CloseHandle(th);
		else
			keep.push_back(th);
	}
	gAsync.joinable.swap(keep);
}

void StartLiveWorker(const std::wstring& addonDir, const char* stem, LiveAsyncJob::Kind kind,
	int itemId)
{
	if (!stem || !stem[0])
		return;
	std::lock_guard<std::mutex> lock(gAsync.mu);
	ReapJoinableUnlocked();
	if (StemIsRunningOrQueued(stem))
		return;

	auto* job = new LiveAsyncJob();
	job->addonDir = addonDir;
	job->stem = stem;
	job->apiKey = G::Gw2ApiKey;
	job->tpWatchIds = G::TpWatchIds;
	job->generation = gAsync.generation;
	job->kind = kind;
	job->itemId = itemId;
	gAsync.queue.push_back(job);
	PumpLiveQueueUnlocked();
}

std::string EnsurePanel(const std::wstring& addonDir, const char* stem,
	LiveAsyncJob::Kind kind, const char* offlineTitle, const char* offlineHeading,
	int itemId)
{
	const std::wstring path = StemPath(addonDir, stem, L".html");
	const std::wstring verPath = StemPath(addonDir, stem, L".ver");
	DWORD ttl = kHtmlTtlSec;
	if (kind == LiveAsyncJob::Tp)
		ttl = kTpHtmlTtlSec;
	else if (kind == LiveAsyncJob::ApiCheck)
		ttl = kApiCheckTtlSec;
	else if (kind == LiveAsyncJob::LegendaryLedger)
		ttl = kLegendaryVaultTtlSec;
	else if (kind == LiveAsyncJob::LegendaryDetail)
		ttl = 2u * 60u * 60u; /* craft tree is expensive — reuse until Sync */
	else if (kind == LiveAsyncJob::BrowseCategory)
		ttl = 7u * 24u * 60u * 60u; /* catalog — version stamp is the real invalidator */
	/* Browse hub: never serve a stale favorites list (cheap sync rebuild). */
	if (kind != LiveAsyncJob::BrowseHub &&
		VerMatches(verPath) && FileFresh(path, ttl) && PanelReady(addonDir, stem))
		return PathToFileUrl(path);

	/* TP tip page — no network; ImGui TpWatchPad owns the real watchlist. */
	if (kind == LiveAsyncJob::Tp)
	{
		WriteUtf8File(path, LivePanelsBuild::BuildTpHtml(nullptr, false));
		WriteUtf8File(verPath, kPanelVer);
		WriteUtf8File(StemPath(addonDir, stem, L".ok"), "1");
		return PathToFileUrl(path);
	}
	/* Cheat sheets hub — catalog only, no network. */
	if (kind == LiveAsyncJob::CheatSheetsHub)
	{
		WriteUtf8File(path, LivePanelsBuild::BuildCheatSheetsHubHtml(addonDir, nullptr));
		WriteUtf8File(verPath, kPanelVer);
		WriteUtf8File(StemPath(addonDir, stem, L".ok"), "1");
		return PathToFileUrl(path);
	}
	/* Browse hub — small; sync so favorites are current immediately. */
	if (kind == LiveAsyncJob::BrowseHub)
	{
		WriteUtf8File(path, LivePanelsBuild::BuildBrowseHubHtml(addonDir, nullptr));
		WriteUtf8File(verPath, kPanelVer);
		WriteUtf8File(StemPath(addonDir, stem, L".ok"), "1");
		return PathToFileUrl(path);
	}
	/* Browse category (Wiki is huge) — shell now, build on worker, then Reload. */
	if (kind == LiveAsyncJob::BrowseCategory)
	{
		const char* slug = nullptr;
		static const char kPrefix[] = "live-browse-cat-";
		if (stem && std::strncmp(stem, kPrefix, sizeof(kPrefix) - 1) == 0)
			slug = stem + (sizeof(kPrefix) - 1);
		const char* cat = LivePanelsBuild::BrowseCategoryFromSlug(slug);
		WriteUtf8File(path, LivePanelsBuild::BuildBrowseCategoryShellHtml(cat ? cat : "Browse"));
		DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());
		StartLiveWorker(addonDir, stem, kind, itemId);
		return PathToFileUrl(path);
	}

	const std::string shell = kind == LiveAsyncJob::LegendaryDetail
		? LivePanelsBuild::BuildLegendaryDetailShellHtml(itemId)
		: OfflineShellHtml(offlineTitle, offlineHeading,
			kind == LiveAsyncJob::ApiCheck
				? "Probing api.guildwars2.com in the background. This page will refresh when ready."
				: "Fetching Live data in the background. This page will refresh when ready. "
				  "You can keep playing — the game should not freeze.");
	WriteUtf8File(path, shell);
	DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());

	StartLiveWorker(addonDir, stem, kind, itemId);
	return PathToFileUrl(path);
}

} // namespace LivePanelsDetail
