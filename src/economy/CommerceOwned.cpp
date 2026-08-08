#include "CommerceShared.h"

#include "InventoryData.h"

#include <unordered_map>

namespace Commerce
{
	void EnsureOwnedWarm(bool force)
	{
		InventoryData::RefreshIfNeeded(force);
		InventoryData::Tick();
	}

	int OwnedQty(int itemId)
	{
		if (itemId <= 0) return 0;
		EnsureOwnedWarm(false);
		return InventoryData::OwnedCount(itemId);
	}

	void FillOwnedMap(std::unordered_map<int, int>& owned)
	{
		EnsureOwnedWarm(false);
		InventoryData::FillOwnedMap(owned);
	}
}
