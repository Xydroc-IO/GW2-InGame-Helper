#include "CommerceShared.h"

#include "Gw2Http.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

namespace Commerce
{
	namespace
	{
		std::mutex gMu;
		ExchangeSnap gLast{};
		ExchangeSnap gPending{};
		std::atomic<bool> gBusy{false};
		std::atomic<bool> gReady{false};

		long long JsonIntAnywhere(const std::string& body, const char* key)
		{
			std::string pat = "\"";
			pat += key;
			pat += "\"";
			size_t k = body.find(pat);
			if (k == std::string::npos) return -1;
			k = body.find(':', k);
			if (k == std::string::npos) return -1;
			++k;
			while (k < body.size() && (body[k] == ' ' || body[k] == '\t')) ++k;
			bool neg = false;
			if (k < body.size() && body[k] == '-') { neg = true; ++k; }
			long long v = 0;
			bool any = false;
			while (k < body.size() && body[k] >= '0' && body[k] <= '9')
			{
				any = true;
				v = v * 10 + (body[k++] - '0');
			}
			if (!any) return -1;
			return neg ? -v : v;
		}
	}

	void FetchExchange(ExchangeSnap& out)
	{
		out = {};
		constexpr int kGems = 100;
		constexpr long long kCoins = 1000000; /* 100g */
		char path[96];
		std::snprintf(path, sizeof(path), "/v2/commerce/exchange/gems?quantity=%d", kGems);
		auto gems = Gw2Http::Api(path, nullptr, 6000);
		std::snprintf(path, sizeof(path), "/v2/commerce/exchange/coins?quantity=%lld",
			static_cast<long long>(kCoins));
		auto coins = Gw2Http::Api(path, nullptr, 6000);
		if (!gems.ok && !coins.ok)
		{
			out.status = "Gem exchange unavailable.";
			return;
		}
		out.gemQty = kGems;
		out.coinQty = kCoins;
		if (gems.ok)
		{
			const long long c = JsonIntAnywhere(gems.body, "coins");
			if (c >= 0) out.coinsForGems = c;
		}
		if (coins.ok)
		{
			const long long g = JsonIntAnywhere(coins.body, "gems");
			if (g >= 0) out.gemsForCoins = g;
		}
		out.ok = (out.coinsForGems > 0 || out.gemsForCoins > 0);
		out.status = out.ok ? "Exchange rates updated." : "Exchange returned empty.";
	}

	void StartExchangeFetch()
	{
		if (gBusy.exchange(true)) return;
		std::thread([]() {
			ExchangeSnap snap;
			FetchExchange(snap);
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPending = std::move(snap);
				gReady = true;
			}
			gBusy = false;
		}).detach();
	}

	bool PollExchange(ExchangeSnap& out)
	{
		if (!gReady.load()) return false;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gReady.load()) return false;
		gLast = gPending;
		out = gLast;
		gReady = false;
		return true;
	}

	ExchangeSnap CopyLastExchange()
	{
		std::lock_guard<std::mutex> lock(gMu);
		return gLast;
	}
}
