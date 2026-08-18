#pragma once

#include "ProgressData.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "Gw2Http.h"

/* Shared state / helpers for ProgressData.cpp + ProgressFetch.cpp. */
namespace ProgressDetail
{
	constexpr int kHttpTimeoutMs = 3500;
	constexpr int kBulkTimeoutMs = 10000;
	constexpr DWORD kAccountTtlMs = 3 * 60 * 1000;
	constexpr DWORD kArmoryTtlMs = 24 * 60 * 60 * 1000;

	struct LegRow
	{
		int id = 0;
		int maxCount = 1;
		int owned = -1; /* -1 unknown */
		std::string name;
		std::string category;   /* Weapon, Armor, Trinket, ... */
		std::string generation; /* Gen 1, SotO, JW, VoE, Other */
		std::string itemType;   /* Spear, Full Set, ... */
	};

	/* Chip 0 is All. Labels match data/legendaries/catalog.json. */
	inline constexpr const char* kArmoryCats[] = {
		"All", "Weapon", "Armor", "Trinket", "Back Item", "Relic", "Sigil", "Rune"
	};
	inline constexpr int kArmoryCatCount = 8;
	inline constexpr const char* kArmoryGens[] = {
		"All", "Gen 1", "Gen 2", "Gen 3", "SotO", "JW", "VoE", "Other"
	};
	inline constexpr int kArmoryGenCount = 8;

	struct Snapshot
	{
		bool ok = false;
		bool hasKey = false;
		bool scopeFail = false;
		std::string status;
		std::vector<LegRow> legs;
		int unlocked = 0;
		DWORD fetchedAt = 0;
	};

	extern std::mutex gMu;
	extern Snapshot gSnap;
	extern Snapshot gDraw;
	extern std::atomic<unsigned> gGen;
	extern unsigned gDrawnGen;
	extern std::atomic<bool> gBusy;
	extern HANDLE gThread;
	extern char gFilter[96];
	extern int gShowMode;
	extern int gCatFilter;
	extern int gGenFilter;

	/* ProgressData.cpp */
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	std::string ReadUtf8File(const std::wstring& path);
	void WriteUtf8File(const std::wstring& path, const std::string& body);
	bool FileFresh(const std::wstring& path, DWORD ttlMs);
	void ParseArmoryCatalog(const std::string& body, std::vector<LegRow>& rows);
	void ApplyNames(const std::string& json, std::vector<LegRow>& rows);
	void FetchNames(std::vector<LegRow>& rows);
	void ApplyLegCatalogMeta(std::vector<LegRow>& rows);
	std::string WikiTitleToPath(const std::string& title);
	void OpenWikiItem(int id, const std::string& name);
	bool FilterMatch(const LegRow& r, const char* filter);
	bool RowVisible(const LegRow& r);
	void SyncDraw();

	/* ProgressDataUi.cpp — immersive armory rows. */
	void DrawArmoryList(const Snapshot& snap);

	/* ProgressFetch.cpp */
	DWORD WINAPI FetchProc(void*);
	void StartFetch(bool force);
}
