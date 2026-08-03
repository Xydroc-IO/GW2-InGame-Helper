#pragma once

/* Account Crafting — daily crafts + item→recipe tree (station + wiki mystic forge /
   legendary gifts) + owned mats + TP cost. Original ImGui UI. */
namespace CraftingData
{
	void RefreshDailiesIfNeeded(bool force = false);

	/* Queue a plan from Progress (or elsewhere) and focus the Crafting tab. */
	void QueuePlan(const char* itemNameOrCode);

	/* True once if QueuePlan asked Account to select the Crafting tab. */
	bool ConsumeFocusTab();

	void RenderContents();
}
