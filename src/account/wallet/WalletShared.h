#pragma once

/* Internal shared types/state for WalletPad / WalletFetch (not public API). */

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace WalletDetail
{
	constexpr int kHttpTimeoutMs = 8000;
	constexpr int kCharTimeoutMs = 8000;
	constexpr DWORD kCacheTtlMs = 5 * 60 * 1000; /* soft TTL - still show instantly */
	constexpr int kItemBatch = 200;
	constexpr int kMaxChars = 64;
	constexpr int kCharWorkers = 4; /* ApiBudget caps total HTTP; keep char fan-out modest */

	enum LocKind : int
	{
		Loc_Wallet = 0,
		Loc_Materials,
		Loc_Bank,
		Loc_Shared,
		Loc_Character,
		Loc_Count
	};

	extern const char* kLocLabels[6];

	struct LocQty
	{
		LocKind kind = Loc_Bank;
		std::string where;
		int count = 0;
	};

	struct Entry
	{
		int id = 0;
		bool isCurrency = false;
		std::string name;
		int total = 0;
		std::vector<LocQty> locs;
	};

	struct SlotCell
	{
		int id = 0;
		int count = 0;
	};
	struct SlotSection
	{
		LocKind kind = Loc_Bank;
		std::string title;
		int filled = 0;
		int capacity = 0;
		std::vector<SlotCell> slots;
	};

	struct Snapshot
	{
		bool ok = false;
		bool noKey = false;
		bool scopeFail = false;
		bool charsPending = false; /* true while toon bags still loading */
		std::string status;
		std::vector<Entry> entries;
		std::vector<SlotSection> sections;
		int charCount = 0;
		int charBagsOk = 0;
		int characterLocItems = 0; /* entries that have at least one Loc_Character */
		DWORD fetchedAt = 0;
	};

	extern std::mutex gMu;
	extern Snapshot gSnap;
	extern std::atomic<unsigned> gGen;
	extern unsigned gDrawnGen;
	extern Snapshot gDraw;

	extern std::atomic<bool> gBusy;
	extern std::atomic<bool> gCancel;
	extern std::atomic<bool> gDeferredFetch;
	extern std::atomic<bool> gDeferredForce;
	extern HANDLE gMasterThread;
	extern bool gFocus;
	extern bool gPlaceOnce;
	extern char gFilter[96];
	extern int gLocFilter;
	extern int gStashSort;

	void DrawStashFolds(const Snapshot& snap, const char* filter, int locFilter, int sortMode);

	/* Persistent id -> name (currency keys stored negative). */
	extern std::mutex gNameMu;
	extern std::unordered_map<int, std::string> gNames;
	extern bool gNamesLoaded;

	using QtyMap = std::unordered_map<int, int>;

	/* WalletFetch.cpp */
	std::wstring NamesPathW();
	void LoadNames();
	void SaveNames();
	std::string LookupName(int mapKey, int id, bool currency);
	void RememberName(int mapKey, const std::string& name);
	std::string FormatCoins(long long copper);
	std::string FormatCount(long long n);
	std::string UrlEncode(const std::string& s);
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	void ParseStringArray(const std::string& body, std::vector<std::string>& out);
	void CollectSlots(const std::string& body, QtyMap& m);
	void CollectOrderedSlots(const std::string& json, size_t openBracket,
		std::vector<SlotCell>& out);
	void CollectBankTabs(const std::string& body, std::vector<SlotSection>& out);
	void CollectSharedSlots(const std::string& body, std::vector<SlotSection>& out);
	void CollectMaterialSections(const std::string& body, const std::string& catJson,
		std::vector<SlotSection>& out);
	void CollectCharBagSections(const std::string& body, const std::string& charName,
		std::vector<SlotSection>& out);
	void MergeLoc(std::unordered_map<int, Entry>& byId, int id, bool currency,
		LocKind kind, const std::string& where, int count);
	void MergeMap(std::unordered_map<int, Entry>& dst, const std::unordered_map<int, Entry>& src);
	Snapshot SnapshotFromMap(std::unordered_map<int, Entry>& byId, const char* status,
		int charCount, int charBagsOk, bool ok, bool charsPending = false);
	void Publish(const std::unordered_map<int, Entry>& byId, const char* status,
		int charCount, int charBagsOk, bool ok, bool charsPending = false,
		const std::vector<SlotSection>* sections = nullptr);
	void ResolveMissingNames(const std::unordered_map<int, Entry>& byId, const char* apiKey);
	/* WalletFetchAcc.cpp */
	DWORD WINAPI CharWorker(void* p);
	DWORD WINAPI AccWallet(void* p);
	DWORD WINAPI AccMats(void* p);
	DWORD WINAPI AccBank(void* p);
	DWORD WINAPI AccShared(void* p);
	DWORD WINAPI MasterProc(void*);
	void StartFetch(bool force);
	void TickDeferredFetch(); /* retry StartFetch when BgFetch allows */

	/* WalletPad.cpp */
	void SyncDrawCopy();
	/* WalletPadStash.cpp */
	bool MatchesFilter(const Entry& e, const char* filter, int locFilter);
}
