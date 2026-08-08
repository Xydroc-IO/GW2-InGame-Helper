#include "EconomyShared.h"

#include "EconomyInternal.h"

#include "CommerceShared.h"

#include <windows.h>

#include <atomic>
#include <ctime>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace EconomyDetail
{
	static std::atomic<bool> gChartPollBusy{false};
	static DWORD gChartPollLastMs = 0;
	constexpr DWORD kChartPollIntervalMs = 90 * 1000;

	void TickChartPoll()
	{
		/* Called from EconomyPad::Render (every Present). Poll while charts are pinned. */
		if (gChartIds.empty())
			return;

		if (gChartPollBusy.load())
			return;

		const DWORD now = GetTickCount();
		if (gChartPollLastMs != 0 && (now - gChartPollLastMs) < kChartPollIntervalMs)
			return;

		std::vector<int> ids;
		{
			std::lock_guard<std::mutex> lock(gMu);
			ids = gChartIds;
		}
		if (ids.empty())
			return;

		gChartPollLastMs = now;
		if (gChartPollBusy.exchange(true))
			return;

		std::thread([ids]() {
			std::unordered_map<int, Commerce::Quote> quotes;
			Commerce::FetchQuotes(ids, quotes, true);
			std::vector<PriceSample> samples;
			samples.reserve(quotes.size());
			const unsigned ts = static_cast<unsigned>(std::time(nullptr));
			for (const auto& kv : quotes)
			{
				const Commerce::Quote& q = kv.second;
				if (q.id <= 0 || (q.buy <= 0 && q.sell <= 0))
					continue;
				PriceSample s{};
				s.id = q.id;
				s.buy = q.buy;
				s.sell = q.sell;
				s.ts = ts;
				samples.push_back(s);
			}
			if (!samples.empty())
			{
				AppendSamples(samples);
				SaveHistory();
			}
			gChartPollBusy = false;
		}).detach();
	}
}
