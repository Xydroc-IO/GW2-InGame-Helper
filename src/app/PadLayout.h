#pragma once

#include "imgui/imgui.h"

/* Shared layout helpers for floating companion pads. */
namespace PadLayout
{
	/* Height left in the current window for a scroll child (after header/tabs). */
	inline float RemainingListH(float minH = 120.f, float reserveBelow = 4.f)
	{
		float h = ImGui::GetContentRegionAvail().y - reserveBelow;
		if (h < minH)
			h = minH;
		return h;
	}

	inline void BeginList(const char* id, float minH = 120.f)
	{
		ImGui::BeginChild(id, ImVec2(0.f, RemainingListH(minH)), true);
	}

	inline void EndList()
	{
		ImGui::EndChild();
	}
}
