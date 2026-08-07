#pragma once

/* Internal shared types/state for Economy Crafting (not public API). */

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	constexpr int kHttpTimeoutMs = 6000;
	constexpr int kBulkTimeoutMs = 10000;
	constexpr int kMaxDepth = 8;
	constexpr DWORD kDailyTtlMs = 10 * 60 * 1000;
	constexpr DWORD kKnownTtlMs = 30 * 60 * 1000;

	struct PlanOpts
	{
		bool useOwnMaterials = true;
		bool craftSubComponents = true; /* buy-vs-craft intermediates */
		bool groupByItem = false;       /* cart aggregate: group shop/steps by output */
	};

	struct IngNode
	{
		int itemId = 0;
		int need = 0;
		int have = 0;
		int depth = 0;
		long long buyUnit = -1;
		std::string name;
		bool crafted = false;
		std::vector<IngNode> kids;
	};

	struct RecipeIng
	{
		int itemId = 0;
		int count = 0;
		std::string name;
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
		std::string source;
		std::string discipline; /* Chef / Armorsmith / Mystic Forge / ... */
	};

	struct DailyRow
	{
		int id = 0;
		std::string slug;
		std::string name;
		bool done = false;
	};

	struct ShopRow
	{
		int itemId = 0;
		int qty = 0;
		long long unitSell = -1;
		long long total = -1;
		std::string name;
		bool priced = false;
	};

	struct StepRow
	{
		int outId = 0;
		int outCnt = 1;
		int crafts = 0;
		int depth = 0;
		std::string disc;
		std::string name;
		std::vector<RecipeIng> ings;
	};

	struct KnownRecipeInfo
	{
		int recipeId = 0;
		int outputId = 0;
		int outCount = 1;
		std::string outputName;
		std::string discipline;
	};

	struct Plan
	{
		bool ok = false;
		std::string status;
		std::string outputName;
		int outputId = 0;
		int outputCount = 1; /* per craft yield */
		int wantQty = 1;     /* how many finished items the user wants */
		int recipeId = 0;
		IngNode root;
		long long buyTotal = 0;
		long long tpBuyOutright = -1; /* instant-buy finished outputs (sells × wantQty) */
		long long tpListUnit = -1;    /* lowest sell listing unit (list near this) */
		long long tpInstantUnit = -1; /* highest buy order unit (instant sell) */
		int noTpMissing = 0;
		std::string recipeSource;
		std::string recipeDiscipline;
		std::vector<std::string> nameHints;
		std::vector<ShopRow> shopping;
		std::vector<StepRow> steps;
	};

	struct CartItem
	{
		int id = 0;
		int qty = 1;
		char name[96]{};
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
	extern int gThreadQty; /* wantQty for in-flight / next plan */
	extern int gPlanQty;   /* UI qty next to Plan button */
	extern DWORD gDailyFetchedAt;
	extern std::atomic<bool> gFocusTab;
	extern std::atomic<unsigned> gPlanGen;
	extern PlanOpts gOpts;

	/* Cart project rollup (multi-item aggregate plans). */
	extern std::mutex gCartPlanMu;
	extern std::vector<Plan> gCartPlans;
	extern std::vector<Plan> gPendingCartPlans;
	extern std::atomic<bool> gCartPlanBusy;
	extern std::atomic<bool> gCartPlanReady;
	extern std::string gCartPlanStatus;

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
	void FetchPrices(std::unordered_map<int, long long>& sells, const std::vector<int>& ids,
		std::unordered_map<int, long long>* buys = nullptr);
	void ParseIntArray(const std::string& body, std::vector<int>& out);
	void ParseQuotedStringArray(const std::string& body, std::vector<std::string>& out);
	std::string EncodeCharPath(const std::string& name);
	bool WriteUtf8File(const std::wstring& path, const std::string& body);
	bool ReadUtf8File(const std::wstring& path, std::string& out);
	std::wstring ConfigFile(const wchar_t* leaf);

	/* CraftingApiRecipe.cpp */
	void AddOwnedCounts(std::unordered_map<int, int>& owned, const std::string& body);
	void LoadOwned(std::unordered_map<int, int>& owned);
	bool LoadApiRecipeForOutput(int outputId, int& outCount, std::vector<RecipeIng>& ings, int& recipeId,
		std::string* disciplineOut = nullptr);
	bool LoadApiRecipeById(int recipeId, int& outputId, int& outCount, std::vector<RecipeIng>& ings,
		std::string* disciplineOut);
	bool IsTerminalMaterial(const std::string& name);
	bool TryLoadRecipe(int outputId, const std::string& nameHint, int& outCount,
		std::vector<RecipeIng>& ings, int& recipeId,
		std::unordered_map<int, RecipeCacheEntry>& cache, std::string* sourceOut);
	std::string ToLowerCopy(std::string s);

	/* CraftingWikiIds.cpp */
	size_t FindCi(const std::string& hay, const char* needle, size_t from = 0);
	void AppendIdsAfterEquals(const std::string& wt, size_t eqPos, std::vector<int>& out);
	void CollectIdsInRange(const std::string& wt, size_t from, size_t to, std::vector<int>& out);
	std::vector<int> CollectWikiItemIdCandidates(const std::string& wikitext);
	int ResolveWikiTitleToItemId(const char* title, std::string* nameOut);
	std::string CleanWikiLinkName(std::string s);
	std::string FetchWikiWikitext(const char* title);
	void FetchWikiWikitextBatch(const std::vector<std::string>& titles,
		std::unordered_map<std::string, std::string>& outByKey);
	bool LoadWikiRecipeForName(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut);
	bool LoadWikiAcquisitionBill(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut);
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
	void CollectMissingLeaves(const IngNode& n, std::vector<const IngNode*>& out);
	void ApplyPrices(IngNode& n, const std::unordered_map<int, long long>& sells,
		long long& buyTotal, int& noTpMissing);
	void ApplyOwnedCounts(IngNode& n, const std::unordered_map<int, int>& owned);

	/* CraftingPlanDecide.cpp — buy-vs-craft collapse + shopping + craft steps */
	void ApplyBuyVsCraft(Plan& plan, const std::unordered_map<int, long long>& sells,
		const PlanOpts& opts);
	void BuildShoppingAndSteps(Plan& plan);

	/* CraftingPlanResolve.cpp */
	void TokenizeQuery(const char* q, std::vector<std::string>& tokens);
	int ScoreTitleMatch(const std::string& title, const std::vector<std::string>& tokens,
		const std::string& qLow);
	void WikiSearchTitles(const char* query, std::vector<std::string>& titles, size_t maxN);
	std::string TypoHintsQuery(const char* q);
	int ResolveQueryToItemId(const char* q, Plan& plan, std::string* nameOut);
	DWORD WINAPI PlanProc(void*);
	void StartPlan();
	void StartPlanWithQty(int wantQty);

	/* CraftingPlanBuild.cpp — expand+price one item (shared by single plan + cart rollup) */
	bool ExpandAndPricePlan(Plan& plan, int itemId, const std::string& name, int wantQty,
		unsigned gen, bool publishLive, bool honorPlanGen = true);

	/* CraftingDailies.cpp */
	DWORD WINAPI DailyProc(void*);
	void StartDailies(bool force);

	/* CraftingKnown.cpp */
	void StartKnown(bool force);
	bool KnownBusy();
	bool KnownHasFetched();
	void KnownTick();
	std::vector<std::string> KnownCharacterNames();
	bool KnownByAccount(int recipeId);
	bool CharKnows(const char* charName, int recipeId);
	std::vector<std::string> CharsKnowing(int recipeId);
	std::vector<int> KnownRecipeIdsForChar(const char* charName);
	size_t KnownUnionCount();
	/* -2 N/A, -1 loading, 0 not known by selected/any, 1 known */
	int RecipeKnownState(int recipeId, const char* preferChar = nullptr);
	/* Queue missing recipe details for a worker (never blocks Present). */
	void EnsureKnownRecipeDetails(const std::vector<int>& recipeIds);
	/* Enqueue at most maxN missing ids (cheap; for UI throttle). */
	void EnsureNextKnownRecipeDetails(const std::vector<int>& recipeIds, size_t maxN);
	bool GetKnownRecipeDetail(int recipeId, KnownRecipeInfo& out);
	size_t KnownDetailsReadyCount(const std::vector<int>& recipeIds);
	void CopyKnownRecipeDetails(const std::vector<int>& recipeIds,
		std::vector<KnownRecipeInfo>& out, size_t* readyOut = nullptr);

	/* CraftingKnownUi.cpp */
	void DrawKnownRail();
	const char* SelectedKnownChar(); /* "" = account union */

	/* CraftingCart.cpp — multi-item named projects + got check-offs */
	void CartEnsureLoaded();
	std::vector<std::string> CartProjectNames();
	const char* CartActiveName();
	void CartSetActive(const char* name);
	std::string CartNew(const char* name);
	bool CartRename(const char* oldName, const char* newName);
	void CartDelete(const char* name);
	std::vector<CartItem> CartItems(const char* project = nullptr);
	void CartAdd(int itemId, const char* name, int qty, const char* project = nullptr);
	void CartSetQty(int itemId, int qty, const char* project = nullptr);
	void CartRemove(int itemId, const char* project = nullptr);
	void CartClear(const char* project = nullptr);
	bool CartIsGot(int matItemId, const char* project = nullptr);
	void CartSetGot(int matItemId, bool on, const char* project = nullptr);

	/* CraftingCartUi.cpp */
	void DrawCartUi();

	/* CraftingCartPlan.cpp — Plan project → aggregated multi-item results */
	void StartCartProjectPlan();
	bool CartPlanBusy();
	void CartPlanTick();
	std::vector<Plan> CartPlansCopy();
	std::string CartPlanStatus();

	/* CraftingOpts.cpp */
	void LoadCraftOpts();
	void SaveCraftOpts();

	/* CraftingResults.cpp */
	void DrawPlanResults(const Plan& plan, bool allowCartGot);
	void DrawAggregatedResults(const std::vector<Plan>& plans, bool allowCartGot);
	void DrawOptsBar();

	/* CraftingData.cpp */
	void Tick();
}
