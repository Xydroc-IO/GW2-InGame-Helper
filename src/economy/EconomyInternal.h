#pragma once
#include "EconomyShared.h"
#include "PadDock.h"

#include <mutex>

namespace EconomyDetail
{
	/* Match Account workbench — stash / crafting need the larger pad. */
	constexpr float kPadW = PadDock::kWorkbenchW;
	constexpr float kPadH = PadDock::kWorkbenchH;

	/* Side-rail indices — Overview first, then market + stash tools. */
	constexpr int kTabOverview = 0;
	constexpr int kTabFlips = 1;
	constexpr int kTabCharts = 2;
	constexpr int kTabCart = 3;
	constexpr int kTabStash = 4;
	constexpr int kTabTrading = 5;
	constexpr int kTabItem = 6;
	constexpr int kTabCrafting = 7;
	constexpr int kTabCount = 8;

	extern std::mutex gMu;
	const char* FallbackName(int id);

	void DrawFlipsTab();
	void DrawChartsTab();
	void DrawCartTab();
}
