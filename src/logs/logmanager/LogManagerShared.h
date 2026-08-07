#pragma once

/* Internal shared types/state for LogManagerPad / Upload / Ei / Cache / KP / Scan / Stats / Ui. */

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

	/* Default when display metrics unavailable - nearly full 1080p client. */
	constexpr float kPadW = 1760.f;
	constexpr float kPadH = 900.f;
	/* Filters column: wide enough for checkbox labels at font scale. */
	constexpr float kFilterFrac = 0.24f;
	constexpr float kFilterMinW = 300.f;
	constexpr float kFilterMaxW = 400.f;
	constexpr float kLogListFracDef = 0.55f; /* of space after filters -> ~48% overall */
	constexpr float kLogListMinW = 420.f;
	constexpr float kRightPaneMinW = 360.f;
	constexpr float kSplitHitW = 6.f;
	constexpr float kPaneGap = 4.f;
	constexpr int kDaysMap[] = {0, 1, 3, 7, 30};

	/* killproof.me item ids (killproofs[]). */
	constexpr int kKpIdLi = 77302;
	constexpr int kKpIdLd = 88485;
	constexpr int kKpIdUfe = 94020;

	enum class ParseState : int
	{
		Pending = 0,
		Parsed,
		Failed,
		Uploading,
		Uploaded
	};

	enum class ResultFilter : int
	{
		All = 0,
		Success,
		Failure,
		Unknown
	};

	enum class ModeFilter : int
	{
		All = 0,
		Normal,
		CM,
		LCM
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

	struct KillProofCacheEntry
	{
		int li = -1;
		int ld = -1;
		int ufe = -1;
		std::string proofUrl;
		std::unordered_map<int, int> amounts; /* item id -> qty */
		DWORD fetchedAtMs = 0;
		bool missing = false;
		bool error = false;
	};

	struct PlayerAgg
	{
		std::string account;
		std::string displayName;
		std::string profession;
		int logs = 0;
		int success = 0;
	};

	struct GuildAgg
	{
		std::string key; /* tag or guildId */
		std::string label;
		int logs = 0;
		int players = 0;
	};

	struct FastestKill
	{
		std::string encounter;
		long long durationMs = 0;
		std::string fileName;
		std::string pathUtf8;
	};

	extern std::mutex gMu;
	extern std::vector<LogEntry> gLogs;
	extern std::atomic<unsigned> gGen;
	extern unsigned gDrawnGen;
	extern std::vector<LogEntry> gDraw;

	extern std::atomic<bool> gScanBusy;
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

	extern HANDLE gScanThread;
	extern HANDLE gParseThread;
	extern HANDLE gUploadThread;
	extern HANDLE gHydrateThread;
	extern HANDLE gEiInstallThread;
	extern HANDLE gKillProofThread;

	extern std::atomic<bool> gKillProofBusy;
	extern std::mutex gKpCacheMu;
	extern std::unordered_map<std::string, KillProofCacheEntry> gKpCache;
	extern std::vector<std::string> gKpQueue;
	extern std::atomic<bool> gKpForce;

	extern bool gFocus;
	extern bool gPlaceOnce;
	extern bool gExpandGroupsOnce;
	extern char gStatus[256];
	extern char gEiStatus[256];
	extern char gSearch[96];
	extern int gResultFilter;
	extern int gModeFilter;
	extern int gDaysCombo;
	extern int gSelected;
	extern bool gFocusSetupTab;
	extern int gSideTab; /* Detail...Setup side rail */
	extern float gLogListFrac;

	extern std::vector<std::string> gUploadQueue;

	/* Path / string helpers (defs in Pad or Cache). */
	std::wstring Utf8ToWide(const char* utf8);
	std::string WideToUtf8(const std::wstring& w);
	std::string JsonEscape(const std::string& s);
	bool PathExistsUtf8(const char* utf8);
	bool IsManagedEiPath(const char* path);
	bool ApplyManagedCliPath();
	void EnsureDefaultPaths();
	bool OpenConfiguredLogFolder();
	std::wstring EiConfPathW();
	std::wstring CachePathW();
	bool FileExistsW(const std::wstring& path);
	bool DirExistsW(const std::wstring& path);
	bool EndsWithI(const std::wstring& s, const wchar_t* suf);
	bool WriteFileUtf8(const std::wstring& path, const std::string& body);
	std::string ReadFileUtf8(const std::wstring& path);
	time_t FileTimeToUnix(const FILETIME& ft);
	std::string FmtDuration(long long ms);
	std::string FmtTime(time_t t);
	bool ContainsI(const std::string& hay, const char* needle);
	bool CopyText(const char* text);
	const char* ResultLabel(int r);
	void OpenFolderFor(const std::wstring& path);

	/* Cache */
	void ApplyEiJsonToEntry(LogEntry& e, const std::string& json);
	void SaveCacheLocked();
	void LoadCacheInto(std::unordered_map<std::string, LogEntry>& byPath);
	void ResolveGuildTagsForPlayers(std::vector<PlayerInfo>& players);

	/* KillProof */
	bool BossTokenForEncounter(const std::string& encounter, int& outId, const char*& outLabel);
	void ApplyKillProofCacheToPlayersLocked(std::vector<PlayerInfo>& players, const std::string& encounter);
	void ApplyKillProofCacheToAllLogsLocked();
	void QueueKillProofAccountsLocked(const std::vector<PlayerInfo>& players, bool force);
	bool EnsureKillProofForLog(LogEntry& e, bool force);
	void BeginKillProofFetch(bool force);

	/* Scan */
	void BeginScan();
	void MaybeAutoParseAfterScan(bool hasDotNet);

	/* Stats */
	void BuildPlayerAggs(const std::vector<const LogEntry*>& filtered, std::vector<PlayerAgg>& out);
	void BuildGuildAggs(const std::vector<const LogEntry*>& filtered, std::vector<GuildAgg>& out);
	void BuildFastest(const std::vector<const LogEntry*>& filtered, std::vector<FastestKill>& out);

	/* Pad orchestration helpers */
	void SyncDraw();
	void CollectFiltered(std::vector<const LogEntry*>& filtered);

	/* UI (ImGui) */
	void DrawBusyOrStatus();
	void DrawToolbar(const std::vector<const LogEntry*>& filtered, bool hasDotNet);
	void DrawFilterPane();
	void DrawLogTable(const std::vector<const LogEntry*>& filtered);
	void DrawDetailTab();
	void DrawPlayersTab(const std::vector<const LogEntry*>& filtered);
	void DrawKillProofTab();
	void DrawGuildsTab(const std::vector<const LogEntry*>& filtered);
	void DrawFastestTab(const std::vector<const LogEntry*>& filtered);
	void DrawSetupTab(bool hasDotNet);
}
