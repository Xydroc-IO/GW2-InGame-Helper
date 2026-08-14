#pragma once

#include "Gw2UiInternal.h"
#include "HelperTheme.h"

#include "imgui/imgui.h"
#include "nexus/Nexus.h"

#include <algorithm>

/* Layout helpers shared by UI_ChromeSideRail*.cpp */
namespace UIDetail
{
namespace SideRail
{
	/* Section label only — no gold divider Dummy (those read as blank gaps). */
	inline void SectionGap(bool labels, const char* title)
	{
		if (!labels || !title || !title[0])
			return;
		ImGui::Spacing();
		ImGui::TextColored(HelperTheme::GoldDim, "%s", title);
	}

	inline float BtnHeight(float iconSz, bool /*labels*/, float framePadY)
	{
		/* Match Gw2Ui::RailToggle — height is icon + FramePadding.y * 2. */
		return iconSz + framePadY * 2.f;
	}

	constexpr int kBtnCount = 20;

	/* Match PaintPadChrome plaque-corner draw size (helper right edge). */
	constexpr float kCornerCap = 28.f;
	/* Keep first/last rail buttons clear of corner ornaments. */
	constexpr float kCornerClear = 22.f;

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
		/* Label + one Spacing — no divider Dummy height. */
		if (labels)
			return itemSp + ImGui::GetTextLineHeight() + itemSp;
		return itemSp;
	}

	inline float PadY(bool /*labels*/)
	{
		return kCornerClear;
	}

	inline float PackedHeight(float iconSz, bool labels, float itemSp, float framePadY)
	{
		constexpr int kGaps = kBtnCount - 1;
		const float padY = PadY(labels);
		const float btnH = BtnHeight(iconSz, labels, framePadY);
		/* TOOLS label (+ HELPER label when shown). No stretch gaps / gold dividers. */
		const float sectionH = labels ? SectionHeight(itemSp, true) : 0.f;
		float h = padY * 2.f
			+ static_cast<float>(kBtnCount) * btnH
			+ static_cast<float>(kGaps) * itemSp
			+ sectionH;
		if (labels)
			h += ImGui::GetTextLineHeight() + itemSp;
		return h;
	}

	/* Grow FramePadding.y so the stack fills dockH (no empty strip under Settings). */
	inline float FillFramePadY(float dockH, float iconSz, bool labels, float itemSp, float baseFp)
	{
		const float packed = PackedHeight(iconSz, labels, itemSp, baseFp);
		const float leftover = dockH - packed;
		if (leftover <= 0.5f)
			return baseFp;
		return baseFp + leftover / (static_cast<float>(kBtnCount) * 2.f);
	}

	inline float FitIconSize(float dockH, bool labels)
	{
		constexpr float kMaxIcon = 52.f;
		constexpr float kMinIcon = 12.f;
		const float maxIcon = labels ? 36.f : kMaxIcon;
		for (float icon = maxIcon; icon >= kMinIcon - 0.5f; icon -= 1.f)
		{
			const float sz = (std::max)(kMinIcon, icon);
			const float itemSp = ItemSpacing(sz, labels);
			const float fp = FramePadY(sz, labels);
			if (PackedHeight(sz, labels, itemSp, fp) <= dockH + 0.5f)
				return sz;
		}
		return kMinIcon;
	}
}
}
