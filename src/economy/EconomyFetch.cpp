#include "EconomyShared.h"

#include "EconomyInternal.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace EconomyDetail
{
	bool gFocus = false;
	bool gPlaceOnce = false;
	int gTab = 0;
	int gForceTab = -1;
	char gStatus[192] = {};
	char gFlipFilter[64] = {};
	bool gFlipBusy = false;
	bool gFlipDone = false;
	std::vector<FlipRow> gFlips;
	std::vector<PriceSample> gHistory;
	std::vector<CartItem> gCart;
	int gChartItemId = 0;
	std::vector<int> gChartIds;

	std::mutex gMu;

	std::string FormatCoins(long long copper)
	{
		if (copper < 0) copper = 0;
		const long long g = copper / 10000;
		const long long s = (copper % 10000) / 100;
		const long long c = copper % 100;
		char buf[64];
		if (g > 0)
			std::snprintf(buf, sizeof(buf), "%lldg %02llds %02lldc", g, s, c);
		else if (s > 0)
			std::snprintf(buf, sizeof(buf), "%llds %02lldc", s, c);
		else
			std::snprintf(buf, sizeof(buf), "%lldc", c);
		return buf;
	}
}
