#pragma once

#include "HelperTheme.h"
#include "imgui/imgui.h"

#include <algorithm>

/* Layout helpers shared by UI_ChromeSideRail*.cpp */
namespace UIDetail
{
namespace SideRail
{
	inline void SectionGap(bool labels, const char* title)
	{
		ImGui::Spacing();
		if (labels && title && title[0])
		{
			ImGui::TextColored(HelperTheme::GoldDim, "%s", title);
			ImGui::Separator();
		}
		else
		{
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float w = ImGui::GetContentRegionAvail().x;
			ImGui::GetWindowDrawList()->AddRectFilled(
				ImVec2(p.x + 4.f, p.y + 2.f),
				ImVec2(p.x + w - 4.f, p.y + 3.f),
				IM_COL32(161, 120, 56, 90));
			ImGui::Dummy(ImVec2(1.f, 6.f));
		}
	}

	inline void StretchGap(float stretch)
	{
		if (stretch > 0.5f)
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + stretch);
	}

	inline float BtnHeight(float iconSz, bool labels, float framePadY)
	{
		if (labels)
			return iconSz + framePadY * 2.f;
		const float padY = (std::max)(2.f, iconSz * 0.12f);
		return iconSz + padY;
	}

	inline float ItemSpacing(float iconSz, bool labels)
	{
		if (iconSz < (labels ? 22.f : 28.f))
			return 1.f;
		if (iconSz < (labels ? 30.f : 40.f))
			return 2.f;
		return 4.f;
	}

	inline float FramePadY(float iconSz, bool labels)
	{
		if (labels)
			return iconSz >= 30.f ? 5.f : 3.f;
		return iconSz >= 40.f ? 4.f : 2.f;
	}

	inline float SectionHeight(float itemSp, bool labels)
	{
		if (labels)
			return itemSp + ImGui::GetTextLineHeight() + itemSp + 4.f + itemSp;
		return itemSp + 6.f + itemSp;
	}

	inline float PadY(bool labels)
	{
		return labels ? 6.f : 8.f;
	}

	inline float PackedHeight(float iconSz, bool labels, float itemSp, float framePadY)
	{
		constexpr int kBtns = 17;
		constexpr int kGaps = 16;
		const float padY = PadY(labels);
		const float btnH = BtnHeight(iconSz, labels, framePadY);
		const float sectionH = labels
			? (SectionHeight(itemSp, true) + SectionHeight(itemSp, false))
			: (2.f * SectionHeight(itemSp, false));
		float h = padY * 2.f
			+ static_cast<float>(kBtns) * btnH
			+ static_cast<float>(kGaps) * itemSp
			+ sectionH;
		if (labels)
			h += ImGui::GetTextLineHeight() + itemSp + 4.f + itemSp;
		return h;
	}

	inline float FitIconSize(float dockH, bool labels)
	{
		constexpr float kMaxIcon = 52.f;
		constexpr float kMinIcon = 12.f;
		const float maxIcon = labels ? 36.f : kMaxIcon;
		const float bottomExtra = PadY(labels);
		for (float icon = maxIcon; icon >= kMinIcon - 0.5f; icon -= 1.f)
		{
			const float sz = (std::max)(kMinIcon, icon);
			const float itemSp = ItemSpacing(sz, labels);
			const float fp = FramePadY(sz, labels);
			if (PackedHeight(sz, labels, itemSp, fp) + bottomExtra <= dockH + 0.5f)
				return sz;
		}
		return kMinIcon;
	}
}
}
