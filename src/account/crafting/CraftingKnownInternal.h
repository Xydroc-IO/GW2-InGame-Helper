#pragma once

#include "CraftingShared.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CraftingDetail
{
	/* Shared between CraftingKnown.cpp (ids) and CraftingKnownDetails.cpp. */
	extern std::mutex gKnownMu;
	extern std::unordered_map<int, KnownRecipeInfo> gRecipeDetails;
	extern std::vector<int> gDetailQueue;
	extern std::unordered_set<int> gDetailQueued;
	extern std::atomic<bool> gDetailBusy;
	extern std::atomic<int> gDetailWorkers;

	constexpr size_t kDetailBatch = 100;
	constexpr int kDetailMaxWorkers = 2;

	void SaveKnownDetailsDisk();
	void LoadKnownDetailsDisk();
}
