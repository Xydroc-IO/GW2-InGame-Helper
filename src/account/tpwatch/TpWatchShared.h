#pragma once

/* Internal shared types/state for TpWatchPad / TpWatchData (not public API). */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

namespace TpWatchDetail
{
	constexpr int kMaxItems = 120;
	constexpr int kHttpTimeoutMs = 2000;
	constexpr float kTpPadW = 440.f;

	struct Row
	{
		int id = 0;
		std::string name;
		long long buy = 0;
		long long sell = 0;
		long long alertSell = 0; /* 0 = off; fire when sell > 0 && sell <= alertSell */
		bool alertHit = false;
	};

	struct DeliveryItem
	{
		int id = 0;
		int count = 0;
		std::string name;
	};

	struct DeliverySnap
	{
		bool noKey = false;
		bool scopeFail = false;
		bool ok = false;
		long long coins = 0;
		std::vector<DeliveryItem> items;
		std::string status;
	};

	struct NameHit
	{
		int id = 0;
		std::string name;
		long long buy = 0;
		long long sell = 0;
		bool hasPrices = false;
	};

	extern std::mutex gMu;
	extern std::vector<Row> gRows;
	extern DeliverySnap gDelivery;
	extern std::atomic<bool> gBusy;
	extern std::atomic<bool> gResultReady;
	extern std::vector<Row> gPending;
	extern DeliverySnap gPendingDelivery;
	extern HANDLE gThread;
	extern HANDLE gAddThread;
	extern char gAddBuf[160];
	extern char gAddThreadQuery[160];
	extern std::string gStatus;
	extern std::vector<NameHit> gNameHits;
	extern std::atomic<bool> gAddBusy;
	extern std::atomic<bool> gAddReady;
	extern std::string gPendingAddStatus;
	extern std::vector<NameHit> gPendingNameHits;
	extern bool gRequestFocus;
	extern int gAlertEditId;
	extern char gAlertEditBuf[64];

	/* TpWatchData.cpp */
	std::string FormatCoins(long long copper);
	void FormatAlertEdit(long long copper, char* out, size_t outLen);
	long long ParseCoinsInput(const char* text);
	void ParseIds(const char* csv, std::vector<int>& out);
	void ParseAlerts(const char* csv, std::vector<std::pair<int, long long>>& out);
	void SaveAlerts(const std::vector<std::pair<int, long long>>& alerts);
	void SetAlertForId(int id, long long thresh);
	void PruneAlertsToIds(const std::vector<int>& ids);
	void SaveIds(const std::vector<int>& ids);
	int ApplyAlerts(std::vector<Row>& rows);
	int ParseItemInput(const char* text);
	void SyncRowsFromSettings();

	/* TpWatchResolve.cpp */
	std::string UrlEncode(const char* s);
	std::string ToLowerCopy(std::string s);
	std::string ExpandNameAlias(const char* q);
	int ExtractWikiItemId(const std::string& wikitext);
	bool ItemExists(int id);
	int ResolveWikiTitleToItemId(const char* title);
	bool CommitWatchId(int id, std::string* statusOut);
	DWORD WINAPI AddNameProc(void*);
	void StartNameResolve();

	/* TpWatchFetch.cpp */
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	std::string IdsQuery(const std::vector<int>& ids);
	void ResolveItemNames(const std::vector<int>& ids,
		std::vector<std::pair<int, std::string>>& outNames);
	void FillNameHitPrices(std::vector<NameHit>& hits);
	void FetchInto(std::vector<Row>& rows);
	void FetchDelivery(DeliverySnap& d);
	DWORD WINAPI FetchProc(void*);
	void StartFetch();
}
