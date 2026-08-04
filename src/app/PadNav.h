#pragma once

#include "HelperTheme.h"
#include "UiScale.h"

#include "imgui/imgui.h"

#include <cstdio>

/* Pad section navigation. Prefer DrawSideRail for Account / Pathing / helper
   chrome — a fixed left column instead of wrapping rows or ImGui ◀ ▶ tabs. */
namespace PadNav
{
	/* Breathing room between content / slider labels and the scrollbar gutter. */
	constexpr float kScrollGutterPad = 10.f;

	/* Visible right edge in screen space. Prefer ContentRegionMax over
	   WindowContentRegionMax — the latter can overshoot the clip rect after a
	   side-rail SameLine + BeginChild, so text never wraps and just clips. */
	inline float WrapEdgeX()
	{
		return ImGui::GetWindowPos().x + ImGui::GetContentRegionMax().x - kScrollGutterPad;
	}

	/* Word-wrap to the remaining content width (not WorkRect-0, which can lie). */
	inline void PushWrap()
	{
		float avail = ImGui::GetContentRegionAvail().x - kScrollGutterPad;
		if (avail < 48.f)
			avail = 48.f;
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + avail);
	}

	inline void PopWrap()
	{
		ImGui::PopTextWrapPos();
	}

	/* Leave room for right-side labels + gutter (SliderFloat / DragFloat). */
	inline void PushLabeledItemWidth()
	{
		const float reserve = ImGui::GetFontSize() * 12.f + kScrollGutterPad;
		ImGui::PushItemWidth(-reserve);
	}

	inline void PopLabeledItemWidth()
	{
		ImGui::PopItemWidth();
	}

	/* SameLine only when next item still fits (Account-style flow). */
	inline void WrapSameLine(float nextItemWidth)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float lastX2 = ImGui::GetItemRectMax().x;
		const float nextX2 = lastX2 + style.ItemSpacing.x + nextItemWidth;
		if (nextX2 < WrapEdgeX() - 1.f)
			ImGui::SameLine(0.f, style.ItemSpacing.x);
	}

	inline float CheckboxWidth(const char* label)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float box = ImGui::GetFrameHeight();
		return box + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label, nullptr, true).x;
	}

	inline float ButtonWidth(const char* label)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		return ImGui::CalcTextSize(label, nullptr, true).x + style.FramePadding.x * 2.f;
	}

	/* Pack chips onto wrapping rows (imgui_demo pattern: SameLine only when the
	   next chip still fits on the current row). Pass first=true for the first
	   chip in a group (or after Spacing / headers). */
	inline bool WrapButton(const char* label, bool selected = false, bool first = false)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float btnW = ButtonWidth(label);
		if (!first)
		{
			const float lastX2 = ImGui::GetItemRectMax().x;
			const float nextX2 = lastX2 + style.ItemSpacing.x + btnW;
			if (nextX2 < WrapEdgeX() - 1.f)
				ImGui::SameLine(0.f, style.ItemSpacing.x);
		}

		if (selected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabIdle);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.16f, 0.08f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Muted);
		}
		const bool clicked = ImGui::SmallButton(label);
		ImGui::PopStyleColor(4);
		return clicked;
	}

	inline void WrapSlash()
	{
		const float slashW = ImGui::CalcTextSize("/").x;
		const float lastX2 = ImGui::GetItemRectMax().x;
		const float nextX2 = lastX2 + 4.f + slashW;
		if (nextX2 < WrapEdgeX() - 1.f)
			ImGui::SameLine(0.f, 4.f);
		ImGui::TextDisabled("/");
	}

	/* Wrapping chip rows (Unlocks kinds, tight toolbars). */
	inline int DrawTabs(const char* id, const char* const* labels, int count, int current)
	{
		if (!labels || count <= 0)
			return 0;
		if (current < 0)
			current = 0;
		if (current >= count)
			current = count - 1;

		ImGui::PushID(id);
		for (int i = 0; i < count; ++i)
		{
			ImGui::PushID(i);
			if (WrapButton(labels[i], i == current, /*first=*/i == 0))
				current = i;
			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::Spacing();
		ImGui::Separator();
		return current;
	}

	/* Left rail: full-width buttons stacked vertically. Caller draws content
	   after this (usually SameLine is already done — rail ends with SameLine). */
	inline int DrawSideRail(const char* id, const char* const* labels, int count, int current,
		float width = 0.f)
	{
		if (width <= 1.f)
			width = UiScale::FitSideRailWidth(labels, count);
		else
			width = UiScale::SideRailWidth(width);
		if (!labels || count <= 0)
			return 0;
		if (current < 0)
			current = 0;
		if (current >= count)
			current = count - 1;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.f, 8.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 5.f));
		ImGui::BeginChild(id, ImVec2(width, 0.f), true,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NavFlattened);

		for (int i = 0; i < count; ++i)
		{
			ImGui::PushID(i);
			const bool on = (i == current);
			if (on)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabActive);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::TabActive);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
				ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabIdle);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.16f, 0.08f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
				ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Muted);
			}
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s###side_%d", labels[i], i);
			if (ImGui::Button(buf, ImVec2(-1.f, 0.f)))
				current = i;
			ImGui::PopStyleColor(4);
			ImGui::PopID();
		}

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::SameLine(0.f, 8.f);
		return current;
	}

	/* Toggle-style rail entry for helper chrome (open pads / flags). */
	inline bool SideToggle(const char* label, bool on)
	{
		if (on)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabIdle);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.16f, 0.08f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Muted);
		}
		const bool clicked = ImGui::Button(label, ImVec2(-1.f, 0.f));
		ImGui::PopStyleColor(4);
		return clicked;
	}
}
