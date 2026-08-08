#pragma once

/* Shared commerce foundation — prices cache, listings, transactions, exchange,
   owned-qty snap. Worker-thread HTTP only; Present just reads. */

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Commerce
{
	constexpr int kBulkChunk = 200;
	constexpr unsigned kPriceTtlMs = 45000;

	struct Quote
	{
		int id = 0;
		long long buy = 0;
		long long sell = 0;
		int demand = 0;
		int supply = 0;
		bool ok = false;
	};

	struct ListingLevel
	{
		long long unitPrice = 0;
		int quantity = 0;
	};

	struct ListingBook
	{
		int id = 0;
		std::vector<ListingLevel> buys;
		std::vector<ListingLevel> sells;
		bool ok = false;
	};

	struct TxOrder
	{
		long long txId = 0;
		int itemId = 0;
		int quantity = 0;
		long long price = 0;
		char name[96]{};
	};

	struct TxSnap
	{
		bool noKey = false;
		bool scopeFail = false;
		bool ok = false;
		std::vector<TxOrder> currentBuys;
		std::vector<TxOrder> currentSells;
		std::vector<TxOrder> histBuys;
		std::vector<TxOrder> histSells;
		std::string status;
	};

	struct ExchangeSnap
	{
		bool ok = false;
		/* Coins you get for `gemQty` gems (from /exchange/gems). */
		long long coinsForGems = 0;
		int gemQty = 0;
		/* Gems you get for `coinQty` coins (from /exchange/coins). */
		long long gemsForCoins = 0;
		long long coinQty = 0;
		std::string status;
	};

	/* CommercePrices.cpp — TTL cache + bulk fetch (worker). */
	void FetchQuotes(const std::vector<int>& ids, std::unordered_map<int, Quote>& out,
		bool force = false);
	bool PeekQuote(int id, Quote& out);
	void PutQuotes(const std::unordered_map<int, Quote>& quotes);

	/* Convenience for craft / callers that only want sell (+ optional buy). */
	void FetchSellBuyMaps(const std::vector<int>& ids,
		std::unordered_map<int, long long>& sells,
		std::unordered_map<int, long long>* buys,
		bool force = false);

	/* CommerceListings.cpp */
	void FetchListings(const std::vector<int>& ids, std::unordered_map<int, ListingBook>& out);
	bool FetchListing(int id, ListingBook& out); /* blocking — workers only */
	void RequestListingAsync(int id); /* Present-safe kick */
	bool TryGetListing(int id, ListingBook& out);
	bool ListingBusy(int id);

	/* CommerceTransactions.cpp — needs tradingpost scope. */
	void FetchTransactions(TxSnap& out);
	void StartTransactionsFetch(); /* async; poll with PollTransactions */
	bool PollTransactions(TxSnap& out);
	bool TransactionsBusy();
	TxSnap CopyLastTransactions();

	/* CommerceExchange.cpp */
	void FetchExchange(ExchangeSnap& out);
	void StartExchangeFetch();
	bool PollExchange(ExchangeSnap& out);
	ExchangeSnap CopyLastExchange();

	/* CommerceOwned.cpp — InventoryData snap for cart / plan. */
	void EnsureOwnedWarm(bool force = false);
	int OwnedQty(int itemId);
	void FillOwnedMap(std::unordered_map<int, int>& owned);
}
