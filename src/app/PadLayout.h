#pragma once

#include "PadNav.h"

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

	/* Scroll child with wrap bound to the child's live width (resize-safe). */
	inline void BeginList(const char* id, float minH = 120.f)
	{
		ImGui::BeginChild(id, ImVec2(0.f, RemainingListH(minH)), true);
		PadNav::PushWrap();
	}

	inline void EndList()
	{
		PadNav::PopWrap();
		ImGui::EndChild();
	}
}
