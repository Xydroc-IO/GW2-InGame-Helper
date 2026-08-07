#pragma once

/* Account Crafting — dailies, known recipes (per-character rail), multi-item craft cart,
   buy-vs-craft plans (shopping + steps + financial), owned mats + TP costs. */
namespace CraftingData
{
	void RefreshDailiesIfNeeded(bool force = false);

	/* Queue a plan from Progress (or elsewhere) and focus the Crafting tab. */
	void QueuePlan(const char* itemNameOrCode);

	/* Focus Account -> Crafting without starting a plan. */
	void RequestFocusTab();

	/* True once if QueuePlan / RequestFocusTab asked Account to select Crafting. */
	bool ConsumeFocusTab();

	void RenderContents();
}
