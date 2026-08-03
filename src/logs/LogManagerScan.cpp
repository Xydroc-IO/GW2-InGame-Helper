#include "LogManagerShared.h"

#include "LogManagerUpload.h"
#include "LogManagerEi.h"

#include "Globals.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace LogManagerDetail
{
	bool IsLogFileName(const std::wstring& name)
	{
		if (EndsWithI(name, L".zevtc") || EndsWithI(name, L".evtc") || EndsWithI(name, L".evtc.zip"))
			return true;
		return false;
	}

	void ScanDirRecursive(const std::wstring& dir, std::vector<LogEntry>& out, int depth)
	{
		if (depth > 12 || gCancel.load())
			return;
		const std::wstring pattern = dir + L"\\*";
		WIN32_FIND_DATAW fd{};
		HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (fd.cFileName[0] == L'.' &&
				(fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
				continue;
			const std::wstring full = dir + L'\\' + fd.cFileName;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				ScanDirRecursive(full, out, depth + 1);
				continue;
			}
			if (!IsLogFileName(fd.cFileName))
				continue;
			LogEntry e;
			e.pathW = full;
			e.pathUtf8 = WideToUtf8(full);
			e.fileName = WideToUtf8(fd.cFileName);
			e.fileSize.LowPart = fd.nFileSizeLow;
			e.fileSize.HighPart = fd.nFileSizeHigh;
			e.mtime = fd.ftLastWriteTime;
			e.encounterTime = FileTimeToUnix(fd.ftLastWriteTime);
			out.push_back(std::move(e));
		} while (FindNextFileW(h, &fd) && !gCancel.load());
		FindClose(h);
	}


	DWORD WINAPI ScanWorker(LPVOID)
	{
		EnsureDefaultPaths();
		const std::wstring root = Utf8ToWide(G::LogFolder);
		std::vector<LogEntry> found;
		std::unordered_map<std::string, LogEntry> cache;
		LoadCacheInto(cache);

		if (!root.empty() && DirExistsW(root))
			ScanDirRecursive(root, found, 0);

		for (LogEntry& e : found)
		{
			auto it = cache.find(e.pathUtf8);
			if (it == cache.end())
				continue;
			const LogEntry& c = it->second;
			if (c.fileSize.QuadPart == e.fileSize.QuadPart &&
				c.mtime.dwLowDateTime == e.mtime.dwLowDateTime &&
				c.mtime.dwHighDateTime == e.mtime.dwHighDateTime &&
				(c.state == ParseState::Parsed || c.state == ParseState::Uploaded ||
					c.state == ParseState::Failed))
			{
				e.state = c.state;
				e.encounter = c.encounter;
				e.mode = c.mode;
				e.result = c.result;
				e.durationMs = c.durationMs;
				if (c.encounterTime > 0)
					e.encounterTime = c.encounterTime;
				e.dpsReportUrl = c.dpsReportUrl;
				e.jsonPathUtf8 = c.jsonPathUtf8;
				e.players = c.players;
				e.compDps = c.compDps;
				e.parseError = c.parseError;
			}
		}

		std::sort(found.begin(), found.end(), [](const LogEntry& a, const LogEntry& b) {
			if (a.encounterTime != b.encounterTime)
				return a.encounterTime > b.encounterTime;
			return a.fileName > b.fileName;
		});

		{
			std::lock_guard<std::mutex> lock(gMu);
			gLogs = std::move(found);
			SaveCacheLocked();
			gGen.fetch_add(1);
		}

		int needMeta = 0;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (const auto& e : gLogs)
			{
				if (e.dpsReportUrl.empty())
					continue;
				if (e.encounter.empty() || e.result < 0 || e.durationMs <= 0 ||
					e.players.empty() || PlayersNeedCombatStats(e.players))
					++needMeta;
			}
		}
		std::snprintf(gStatus, sizeof(gStatus), "Found %d logs.", static_cast<int>(gLogs.size()));
		gScanBusy.store(false);
		if (needMeta > 0)
			BeginHydrateFromReports(false);
		return 0;
	}

	void BeginScan()
	{
		if (gScanBusy.exchange(true))
			return;
		gCancel.store(false);
		std::snprintf(gStatus, sizeof(gStatus), "Scanning logs…");
		if (gScanThread)
		{
			WaitForSingleObject(gScanThread, 0);
			CloseHandle(gScanThread);
			gScanThread = nullptr;
		}
		gScanThread = CreateThread(nullptr, 0, ScanWorker, nullptr, 0, nullptr);
		if (!gScanThread)
			gScanBusy.store(false);
	}

	void MaybeAutoParseAfterScan(bool hasDotNet)
	{
		static bool sWasScanBusy = false;
		const bool scanning = gScanBusy.load();
		const bool justFinished = sWasScanBusy && !scanning;
		sWasScanBusy = scanning;
		if (!justFinished || !G::LogManagerAutoParse)
			return;
		if (!hasDotNet || gParseBusy.load() || gEiInstallBusy.load())
			return;
		if (!G::EliteInsightsPath[0] || !PathExistsUtf8(G::EliteInsightsPath))
			return;
		BeginParsePending();
	}

} // namespace LogManagerDetail
