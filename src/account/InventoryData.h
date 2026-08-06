#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

/* Account inventory aggregation - materials, bank, shared, character bags,
   and worn equipment. WalletPad keeps its richer UI; Crafting/SessionHistory
   consume this module for owned counts. */
namespace InventoryData
{
	enum class LocKind
	{
		Materials = 0,
		Bank,
		Shared,
		Character,
		Equipment
	};

	struct Location
	{
		LocKind kind = LocKind::Bank;
		std::string where; /* e.g. "Bank", character name */
		int count = 0;
	};

	void RefreshIfNeeded(bool force = false);
	void Tick();
	bool Busy();
	bool Ready();
	const char* Status();

	int OwnedCount(int itemId);
	void Locations(int itemId, std::vector<Location>& out);
	void FillOwnedMap(std::unordered_map<int, int>& owned);

	size_t UniqueItemCount();
	size_t TotalStackCount();
	unsigned FetchedAtMs();
}
