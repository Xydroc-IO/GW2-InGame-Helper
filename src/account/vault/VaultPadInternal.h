#pragma once

#include "VaultPad.h"

#include <atomic>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "Gw2Http.h"

#include "imgui/imgui.h"

/* Shared state / helpers for VaultPad.cpp + VaultData.cpp + VaultFetch.cpp. */
namespace VaultDetail
{
	constexpr int kHttpTimeoutMs = 3500;
	constexpr int kBulkTimeoutMs = 8000;
	constexpr DWORD kCacheTtlMs = 3 * 60 * 1000;
	constexpr float kPadW = 440.f;
	constexpr float kPadH = 480.f;

	struct Obj
	{
		std::string title;
		std::string track;
		int cur = 0;
		int need = 0;
		int acclaim = 0;
		bool done = false;
	};

	struct Snapshot
	{
		bool ok = false;
		bool hasKey = false;
		bool scopeFail = false;
		std::string status;
		std::string seasonTitle;
		std::string seasonBlurb;
		std::vector<Obj> daily;
		std::vector<Obj> weekly;
		std::vector<Obj> special;
		std::vector<Obj> easyPreview; /* no key */
		DWORD fetchedAt = 0;
	};

	extern std::mutex gMu;
	extern Snapshot gSnap;
	extern Snapshot gDraw;
	extern std::atomic<unsigned> gGen;
	extern unsigned gDrawnGen;
	extern std::atomic<bool> gBusy;
	extern std::atomic<bool> gDeferredFetch;
	extern std::atomic<bool> gDeferredForce;
	extern HANDLE gThread;
	extern bool gFocus;
	extern bool gPlaceOnce;

	/* VaultData.cpp */
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	bool JsonBoolAfterKey(const std::string& json, const char* key, size_t from = 0);
	std::string FormatIsoDateUtc(const std::string& iso);
	bool ParseIsoUtc(const std::string& iso, time_t* out);
	std::string SeasonBlurb(const std::string& startIso, const std::string& endIso);
	std::string FormatCountdown(long long sec);
	void UtcNowParts(int& y, int& mo, int& d, int& h, int& mi, int& s, int& wday);
	time_t MakeUtc(int y, int mo, int d, int h, int mi, int s);
	void AddUtcDays(int& y, int& mo, int& d, int days);
	long long SecUntilDailyResetUtc();
	long long SecUntilWeeklyResetUtc();
	void ParseVaultObjs(const std::string& json, std::vector<Obj>& out, int maxItems = 80,
		int maxAcclaim = -1);

	/* VaultFetch.cpp */
	std::wstring StemJson(const std::wstring& dir, const char* stem);
	bool FileFresh(const std::wstring& path, DWORD ttlSec);
	std::string ReadUtf8File(const std::wstring& path);
	bool TryLiveCache(const std::wstring& dir, const char* stem, DWORD ttlSec, Gw2Http::Result& out);
	DWORD WINAPI MasterProc(void*);
	void StartFetch(bool force);
	void TickDeferredFetch();

	/* VaultPad.cpp */
	void DrawResetCountdowns();
	void SyncDraw();
	void DrawObjList(const char* label, const std::vector<Obj>& list);
}
