#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace EconomyDetail
{
	struct FlipRow
	{
		int id = 0;
		char name[96]{};
		long long buy = 0;
		long long sell = 0;
		long long spread = 0; /* sell - buy after fees approx */
		int supply = 0;
		int demand = 0;
	};

	struct PriceSample
	{
		int id = 0;
		long long buy = 0;
		long long sell = 0;
		unsigned ts = 0; /* unix */
	};

	struct CartItem
	{
		int id = 0;
		char name[96]{};
		int qty = 1;
	};

	extern bool gFocus;
	extern bool gPlaceOnce;
	extern int gDeferLoads; /* Wine: defer LoadCart/Charts/History off Soft-open frame */
	extern int gTab;
	extern int gForceTab; /* -1 none; set to force side-rail tab once */
	extern char gStatus[192];
	extern char gFlipFilter[64];
	extern bool gFlipBusy;
	extern bool gFlipDone;
	extern std::vector<FlipRow> gFlips;
	extern std::vector<PriceSample> gHistory;
	extern std::vector<CartItem> gCart;
	extern int gChartItemId; /* last focused chart (also used when list is empty) */
	extern std::vector<int> gChartIds; /* pinned Charts tab entries */

	void EnsureSeed();
	void RequestFlipScan();
	void PollFlipWorker();
	void TickChartPoll(); /* background /v2/commerce/prices for pinned charts */
	void AddToCart(int id, const char* name, int qty = 1);
	void RemoveCart(size_t idx);
	void ClearCart();
	void SaveCart();
	void LoadCart();
	void AddChart(int id); /* pin item on Charts tab (no-op if already pinned) */
	void RemoveChart(size_t idx);
	void ClearCharts();
	void SaveCharts();
	void LoadCharts();
	void SaveHistory();
	void LoadHistory();
	void RecordSample(int id, long long buy, long long sell);
	void AppendSamples(const std::vector<PriceSample>& samples); /* per-id + global trim */
	std::string FormatCoins(long long copper);
}
