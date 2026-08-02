#pragma once

/* Internal shared types/state for LogManagerPad / Upload / Ei (not public API). */

#include "LogManagerParse.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace LogManagerDetail
{
	using LogManagerParse::PlayerInfo;
	using LogManagerParse::ExtractGuildTag;
	using LogManagerParse::SkipWs;
	using LogManagerParse::JsonStringAfterKey;
	using LogManagerParse::JsonBoolAfterKey;
	using LogManagerParse::JsonLongAfterKey;
	using LogManagerParse::JsonDoubleAfterKey;
	using LogManagerParse::ObjectEnd;
	using LogManagerParse::ExtractFirstObjectInArrayAfterKey;
	using LogManagerParse::FillPlayerCombatStats;
	using LogManagerParse::PlayersHaveDps;
	using LogManagerParse::PlayersHaveBoons;
	using LogManagerParse::PlayersNeedCombatStats;
	using LogManagerParse::FillPlayerFromDpsReportObj;
	using LogManagerParse::ParsePlayersFromJson;
	using LogManagerParse::ParseDpsReportPlayers;
	using LogManagerParse::AmountNearId;

	constexpr int kParseTimeoutMs = 180000;
	constexpr int kUploadTimeoutMs = 120000;

	enum class ParseState : int
	{
		Pending = 0,
		Parsed,
		Failed,
		Uploading,
		Uploaded
	};

	struct LogEntry
	{
		std::wstring pathW;
		std::string pathUtf8;
		std::string fileName;
		ULARGE_INTEGER fileSize{};
		FILETIME mtime{};
		ParseState state = ParseState::Pending;
		std::string encounter;
		std::string mode; /* "", "CM", "LCM" */
		int result = -1; /* -1 unknown, 0 fail, 1 success */
		long long durationMs = 0;
		time_t encounterTime = 0;
		int compDps = 0; /* squad DPS from dps.report / EI */
		std::string dpsReportUrl;
		std::string parseError;
		std::string jsonPathUtf8;
		std::vector<PlayerInfo> players;
	};

	extern std::mutex gMu;
	extern std::vector<LogEntry> gLogs;
	extern std::atomic<unsigned> gGen;
	extern std::atomic<bool> gParseBusy;
	extern std::atomic<bool> gUploadBusy;
	extern std::atomic<bool> gHydrateBusy;
	extern std::atomic<bool> gHydrateForce;
	extern std::atomic<bool> gEiInstallBusy;
	extern std::atomic<bool> gCancel;
	extern std::atomic<int> gParseDone;
	extern std::atomic<int> gParseTotal;
	extern std::atomic<int> gUploadDone;
	extern std::atomic<int> gUploadTotal;
	extern HANDLE gParseThread;
	extern HANDLE gUploadThread;
	extern HANDLE gHydrateThread;
	extern HANDLE gEiInstallThread;
	extern char gStatus[256];
	extern char gEiStatus[256];
	extern std::vector<std::string> gUploadQueue;

	std::wstring Utf8ToWide(const char* utf8);
	std::string WideToUtf8(const std::wstring& w);
	bool PathExistsUtf8(const char* utf8);
	bool IsManagedEiPath(const char* path);
	bool ApplyManagedCliPath();
	void EnsureDefaultPaths();
	std::wstring EiConfPathW();
	std::wstring CachePathW();
	bool FileExistsW(const std::wstring& path);
	bool DirExistsW(const std::wstring& path);
	bool EndsWithI(const std::wstring& s, const wchar_t* suf);
	bool WriteFileUtf8(const std::wstring& path, const std::string& body);
	std::string ReadFileUtf8(const std::wstring& path);
	void ApplyEiJsonToEntry(LogEntry& e, const std::string& json);
	void SaveCacheLocked();
}
