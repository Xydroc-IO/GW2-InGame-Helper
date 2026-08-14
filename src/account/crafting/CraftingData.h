#pragma once

/* Economy Crafting — dailies, known recipes (per-character rail), multi-item craft cart,
   buy-vs-craft plans (shopping + steps + financial), owned mats + TP costs. */
namespace CraftingData
{
	void RefreshDailiesIfNeeded(bool force = false);

	/* Pause known-recipe detail fetch when Crafting tab is not visible. */
	void SetKnownDetailsActive(bool active);

	/* Queue a plan and open the Crafting pad. */
	void QueuePlan(const char* itemNameOrCode);

	/* Open the Crafting pad. */
	void RequestFocusTab();

	/* Open Crafting on the Craft cart sub-tab. */
	void RequestFocusCraftCart();

	/* True once if QueuePlan / RequestFocusTab asked the Crafting pad to focus. */
	bool ConsumeFocusTab();

	void RenderContents();
}
