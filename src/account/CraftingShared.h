#pragma once

/* Internal shared types/state for CraftingData / Api / Wiki / Plan / Dailies (not public API). */

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	constexpr int kHttpTimeoutMs = 6000;
	constexpr int kBulkTimeoutMs = 10000;
	/* Deep enough for legendary → gift → sub-gift → mats. */
	constexpr int kMaxDepth = 5;
	constexpr DWORD kDailyTtlMs = 10 * 60 * 1000;

	struct IngNode
	{
		int itemId = 0;
		int need = 0;
		int have = 0;
		int depth = 0;
		long long buyUnit = -1; /* instant-buy (sells) unit; -1 = not on TP */
		std::string name;
		bool crafted = false;
		std::vector<IngNode> kids;
	};

	struct RecipeIng
	{
		int itemId = 0;
		int count = 0;
		std::string name; /* wiki/API hint so sub-gifts expand without an extra lookup */
	};

	struct RecipeCacheEntry
	{
		bool apiTried = false;
		bool wikiTried = false;
		bool acquireTried = false;
		bool curatedTried = false;
		bool ok = false;
		int outCount = 1;
		int recipeId = 0;
		std::vector<RecipeIng> ings;
		std::string source; /* "Crafting station" / "Mystic Forge" / … */
	};

	struct DailyRow
	{
		int id = 0;
		std::string name;
	};

	struct Plan
	{
		bool ok = false;
		std::string status;
		std::string outputName;
		int outputId = 0;
		int outputCount = 1;
		IngNode root;
		long long buyTotal = 0;
		int noTpMissing = 0; /* missing stacks with no commerce listing */
		std::string recipeSource; /* station vs mystic forge (wiki) */
		std::vector<std::string> nameHints;
	};

	extern std::mutex gMu;
	extern Plan gPlan;
	extern Plan gPendingPlan;
	extern std::vector<DailyRow> gDailies;
	extern std::vector<DailyRow> gPendingDailies;
	extern std::string gDailyStatus;
	extern std::atomic<bool> gBusy;
	extern std::atomic<bool> gDailyBusy;
	extern std::atomic<bool> gReady;
	extern std::atomic<bool> gDailyReady;
	extern HANDLE gThread;
	extern HANDLE gDailyThread;
	extern char gQuery[192];
	extern char gThreadQuery[192];
	extern DWORD gDailyFetchedAt;
	extern std::atomic<bool> gFocusTab;
	extern std::atomic<unsigned> gPlanGen;

	extern std::mutex gWikiMu;
	extern std::unordered_map<std::string, std::string> gWikiTextCache;
	extern std::mutex gRecipeCacheMu;

	/* CraftingApi.cpp */
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	std::string FormatCoins(long long copper);
	std::string UrlEncode(const char* s);
	int ParseItemId(const char* text);
	std::string ItemName(int id);
	void FetchNames(std::unordered_map<int, std::string>& names, const std::vector<int>& ids);
	int FirstValidItemId(const std::vector<int>& candidates, std::string* nameOut);
	void FetchPrices(std::unordered_map<int, long long>& sells, const std::vector<int>& ids);

	/* CraftingApiRecipe.cpp */
	void AddOwnedCounts(std::unordered_map<int, int>& owned, const std::string& body);
	void LoadOwned(std::unordered_map<int, int>& owned);
	bool LoadApiRecipeForOutput(int outputId, int& outCount, std::vector<RecipeIng>& ings, int& recipeId);
	bool IsTerminalMaterial(const std::string& name);
	bool TryLoadRecipe(int outputId, const std::string& nameHint, int& outCount,
		std::vector<RecipeIng>& ings, int& recipeId,
		std::unordered_map<int, RecipeCacheEntry>& cache, std::string* sourceOut);
	std::string ToLowerCopy(std::string s);

	/* CraftingWiki.cpp */
	size_t FindCi(const std::string& hay, const char* needle, size_t from = 0);
	void AppendIdsAfterEquals(const std::string& wt, size_t eqPos, std::vector<int>& out);
	void CollectIdsInRange(const std::string& wt, size_t from, size_t to, std::vector<int>& out);
	std::vector<int> CollectWikiItemIdCandidates(const std::string& wikitext);
	std::string FetchWikiWikitext(const char* title);
	void FetchWikiWikitextBatch(const std::vector<std::string>& titles,
		std::unordered_map<std::string, std::string>& outByKey);
	int ResolveWikiTitleToItemId(const char* title, std::string* nameOut);
	std::string CleanWikiLinkName(std::string s);
	bool LoadWikiRecipeForName(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut);

	/* CraftingWikiAcquire.cpp — Sold by / tp-placeholder material lists */
	bool LoadWikiAcquisitionBill(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut);

	/* CraftingCurated.cpp — hardcoded bills when wiki has no {{recipe}} */
	bool LoadCuratedBill(int outputId, int& outCount, std::vector<RecipeIng>& ings,
		std::string* sourceOut);

	/* CraftingPlan.cpp */
	void PublishLivePlan(const Plan& plan);
	void FinishPrices(Plan& plan, std::unordered_map<int, std::string>& names);
	void ExpandFrontier(Plan& plan, std::unordered_map<int, int>& owned,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, RecipeCacheEntry>& recipeCache, int childDepth,
		bool expandEvenIfOwned = false);
	void BuildTree(IngNode& node, int depth, std::unordered_map<int, int>& owned,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, RecipeCacheEntry>& cache);
	void CollectLeafIds(const IngNode& n, std::vector<int>& ids);
	void ApplyPrices(IngNode& n, const std::unordered_map<int, long long>& sells,
		long long& buyTotal, int& noTpMissing);
	void ApplyOwnedCounts(IngNode& n, const std::unordered_map<int, int>& owned);

	/* CraftingPlanResolve.cpp */
	void TokenizeQuery(const char* q, std::vector<std::string>& tokens);
	int ScoreTitleMatch(const std::string& title, const std::vector<std::string>& tokens,
		const std::string& qLow);
	void WikiSearchTitles(const char* query, std::vector<std::string>& titles, size_t maxN);
	std::string TypoHintsQuery(const char* q);
	int ResolveQueryToItemId(const char* q, Plan& plan, std::string* nameOut);
	DWORD WINAPI PlanProc(void*);
	void StartPlan();

	/* CraftingDailies.cpp */
	DWORD WINAPI DailyProc(void*);
	void StartDailies(bool force);

	/* CraftingData.cpp */
	void DrawNode(const IngNode& n);
	void Tick();
}
