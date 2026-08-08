#pragma once

/* Economy Crafting — dailies, known recipes (per-character rail), multi-item craft cart,
   buy-vs-craft plans (shopping + steps + financial), owned mats + TP costs. */
namespace CraftingData
{
	void RefreshDailiesIfNeeded(bool force = false);

	/* Pause known-recipe detail fetch when Crafting tab is not visible. */
	void SetKnownDetailsActive(bool active);

	/* Queue a plan from Progress (or elsewhere) and focus the Crafting tab. */
	void QueuePlan(const char* itemNameOrCode);

	/* Focus Economy -> Crafting without starting a plan. */
	void RequestFocusTab();

	/* Focus Economy -> Crafting -> Craft cart sub-tab. */
	void RequestFocusCraftCart();

	/* True once if QueuePlan / RequestFocusTab asked Economy to select Crafting. */
	bool ConsumeFocusTab();

	void RenderContents();
}
