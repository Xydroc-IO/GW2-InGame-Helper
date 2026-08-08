#pragma once

/* Active-tab / active-pad background fetch arbitration.
   UI sets Wanted(); workers/callers check AllowWork() before starting heavy jobs.
   Crafting details outrank Wallet/Vault so Crafting→Stash does not stampede. */
namespace BgFetch
{
	enum class Channel : int
	{
		CraftingDetails = 0,
		Wallet,
		Vault,
		Instances,
		TpWatch,
		Count
	};

	void SetWanted(Channel c, bool wanted);
	bool Wanted(Channel c);

	/* True when this channel may start / continue heavy API work. */
	bool AllowWork(Channel c);

	/* Optional: true while known-recipe detail workers still hold HTTP. */
	void SetCraftingDetailsBusy(bool busy);
	bool CraftingDetailsBusy();
}
