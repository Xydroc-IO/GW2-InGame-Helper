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
	extern int gTab;
	extern int gForceTab; /* -1 none; set to force side-rail tab once */
	extern char gStatus[192];
	extern char gFlipFilter[64];
	extern bool gFlipBusy;
	extern bool gFlipDone;
	extern std::vector<FlipRow> gFlips;
	extern std::vector<PriceSample> gHistory;
	extern std::vector<CartItem> gCart;
	extern int gChartItemId;

	void EnsureSeed();
	void RequestFlipScan();
	void PollFlipWorker();
	void AddToCart(int id, const char* name, int qty = 1);
	void RemoveCart(size_t idx);
	void ClearCart();
	void SaveCart();
	void LoadCart();
	void SaveHistory();
	void LoadHistory();
	void RecordSample(int id, long long buy, long long sell);
	std::string FormatCoins(long long copper);
}
