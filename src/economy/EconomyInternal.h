#pragma once
#include "EconomyShared.h"
#include "PadDock.h"

#include <mutex>

namespace EconomyDetail
{
	/* Match Account workbench — stash / crafting need the larger pad. */
	constexpr float kPadW = PadDock::kWorkbenchW;
	constexpr float kPadH = PadDock::kWorkbenchH;

	/* Side-rail indices — Overview → Item → Trading → Flips → Charts → Cart → Crafting → Stash. */
	constexpr int kTabOverview = 0;
	constexpr int kTabItem = 1;
	constexpr int kTabTrading = 2;
	constexpr int kTabFlips = 3;
	constexpr int kTabCharts = 4;
	constexpr int kTabCart = 5;
	constexpr int kTabCrafting = 6;
	constexpr int kTabStash = 7;
	constexpr int kTabCount = 8;

	extern std::mutex gMu;
	const char* FallbackName(int id);

	constexpr size_t kMaxSamplesPerId = 120;
	constexpr size_t kMaxSamplesGlobal = 8000;

	void TickChartPoll();
	void DrawFlipsTab();
	void DrawChartsTab();
	void DrawCartTab();
}
