#pragma once

#include "EventsData.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

/* Shared state / helpers for EventsPad.cpp + EventsPadState.cpp. */
namespace EventsPadDetail
{
	constexpr int kMaxTrack = 120;
	constexpr int kHttpTimeoutMs = 2500;
	constexpr int kWarnWithinSec = 10 * 60;
	constexpr int kSoonFilterSec = 30 * 60;
	constexpr float kPadW = 440.f;
	constexpr float kPadH = 480.f;

	struct Timing
	{
		bool live = false;
		int  untilStart = -1; /* 0 when live */
		int  untilEnd = -1;
	};

	struct Row
	{
		int index = 0;
		Timing timing;
		bool tracked = false;
		bool claimed = false;
		bool warn = false;
	};

	extern std::mutex gMu;
	extern std::unordered_set<std::string> gBossDone;
	extern std::unordered_set<std::string> gChestDone;
	extern std::string gClaimNote;
	extern std::atomic<bool> gBusy;
	extern std::atomic<bool> gReady;
	extern std::unordered_set<std::string> gPendBoss;
	extern std::unordered_set<std::string> gPendChest;
	extern std::string gPendNote;
	extern HANDLE gThread;
	extern bool gFocus;
	extern bool gPlaceOnce;
	extern bool gTrackedOnly;
	extern bool gSoonOnly;
	extern bool gThisMapOnly;
	extern char gStatus[128];
	extern int gSectionPick; /* 0 = default mix */
	extern char gSearch[96];

	int PosMod(int v, int m);
	Timing FromStartList(int nowInCycle, int cycleLen, const int* starts, int n, int activeSec);
	Timing FromRepeat(time_t now, int cycleSec, int phaseSec, int activeSec, int copies);
	Timing ComputeTiming(const EventsData::Entry& e, time_t now);
	std::string FmtRemain(int secs);
	bool CopyText(const char* text);
	void ParseTrackCsv(const char* csv, std::vector<std::string>& out);
	void WriteTrackCsv(const std::vector<std::string>& ids);
	bool Tracked(const char* key);
	void FlipTrack(const char* key);
	unsigned CurrentMapId();
	bool ContainsFold(const char* hay, const char* needle);
	bool MatchesSearch(const EventsData::Entry& e, const char* q);
	void CollectQuotedIds(const std::string& body, std::unordered_set<std::string>& out);
	void BeginClaimRefresh();
	void ApplyClaimResult();
	bool EntryClaimed(const EventsData::Entry& e);
	void CollectRows(std::vector<Row>& rows, time_t now);
}
