#pragma once

/* Account Crafting - daily crafts + item->recipe tree (station + wiki mystic forge /
   legendary gifts) + owned mats + TP cost. Original ImGui UI. */
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
