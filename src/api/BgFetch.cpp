#include "BgFetch.h"

#include <atomic>

namespace
{
	std::atomic<bool> gWanted[static_cast<int>(BgFetch::Channel::Count)]{};
	std::atomic<bool> gCraftBusy{false};
}

void BgFetch::SetWanted(Channel c, bool wanted)
{
	const int i = static_cast<int>(c);
	if (i < 0 || i >= static_cast<int>(Channel::Count)) return;
	gWanted[i].store(wanted);
}

bool BgFetch::Wanted(Channel c)
{
	const int i = static_cast<int>(c);
	if (i < 0 || i >= static_cast<int>(Channel::Count)) return false;
	return gWanted[i].load();
}

void BgFetch::SetCraftingDetailsBusy(bool busy)
{
	gCraftBusy.store(busy);
}

bool BgFetch::CraftingDetailsBusy()
{
	return gCraftBusy.load();
}

bool BgFetch::AllowWork(Channel c)
{
	if (!Wanted(c))
		return false;

	/* Wallet / Vault wait until crafting is not the active tab and detail HTTP drained. */
	if (c == Channel::Wallet || c == Channel::Vault)
	{
		if (Wanted(Channel::CraftingDetails))
			return false;
		if (gCraftBusy.load())
			return false;
	}

	/* Soft: Instances yields while Wallet is the focused heavy pad. */
	if (c == Channel::Instances && Wanted(Channel::Wallet))
		return false;

	return true;
}
