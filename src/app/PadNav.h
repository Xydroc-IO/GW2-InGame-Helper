#pragma once

#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "UiScale.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

/* Pad section navigation. Prefer DrawSideRail for every themed pad —
   a fixed left column instead of wrapping rows or ImGui ◀ ▶ tabs. */
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

	/* Window-local wrap X for PushTextWrapPos (tracks resize / rail / scroll). */
	inline float WrapLocalX()
	{
		float x = ImGui::GetContentRegionMax().x - kScrollGutterPad + ImGui::GetScrollX();
		const float minX = ImGui::GetCursorPos().x + 48.f;
		if (x < minX)
			x = minX;
		return x;
	}

	/* Word-wrap to the live content edge (reflows when the pad is resized).
	   Always PopWrap() before ImGui::End() / EndChild() — do not RAII across End. */
	inline void PushWrap()
	{
		ImGui::PushTextWrapPos(WrapLocalX());
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

	/* Label text before ### id (ImGui right-side labels). */
	inline float VisibleLabelWidth(const char* label)
	{
		if (!label || !label[0])
			return 0.f;
		const char* end = std::strstr(label, "###");
		return end
			? ImGui::CalcTextSize(label, end, true).x
			: ImGui::CalcTextSize(label, nullptr, true).x;
	}

	/* Field + label span for WrapSameLine before InputText / DragFloat / etc. */
	inline float LabeledSpan(const char* label, float fieldW)
	{
		return fieldW + ImGui::GetStyle().ItemInnerSpacing.x + VisibleLabelWidth(label);
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

	/* Wrap if needed, then SetNextItemWidth for a right-labeled widget. */
	inline void PrepLabeled(const char* label, float fieldW, bool first = false)
	{
		if (!first)
			WrapSameLine(LabeledSpan(label, fieldW));
		ImGui::SetNextItemWidth(fieldW);
	}

	/* Full-row labeled input: shrink field so the right-side label never clips. */
	inline void PushWidthForLabel(const char* label)
	{
		const float reserve = VisibleLabelWidth(label) +
			ImGui::GetStyle().ItemInnerSpacing.x + kScrollGutterPad;
		ImGui::PushItemWidth(-(reserve > 24.f ? reserve : 24.f));
	}

	inline void PopWidthForLabel()
	{
		ImGui::PopItemWidth();
	}

	/* Caption above, full-width control — avoids right-side label clipping. */
	inline bool SliderFloatRow(const char* caption, const char* id, float* v,
		float vMin, float vMax, const char* fmt = "%.2f")
	{
		ImGui::TextUnformatted(caption);
		ImGui::SetNextItemWidth(-1.f);
		char buf[96];
		std::snprintf(buf, sizeof(buf), "##%s", id);
		return ImGui::SliderFloat(buf, v, vMin, vMax, fmt);
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
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::Header);
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

	/* Left rail: full-width buttons stacked vertically. Optional DAT icons
	   (icons[i] > 0). Caller draws content after — rail ends with SameLine. */
	inline int DrawSideRail(const char* id, const char* const* labels, int count, int current,
		float width = 0.f, const int* icons = nullptr)
	{
		if (width <= 1.f)
			width = UiScale::FitSideRailWidth(labels, count, 80.f, 260.f, icons ? 18.f : 0.f);
		else
			width = UiScale::SideRailWidth(width);
		if (icons)
		{
			/* FitSideRailWidth already reserved icon when iconSize passed; keep a floor. */
			const float minIcons = UiScale::SideRailWidth(108.f);
			if (width < minIcons)
				width = minIcons;
		}
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
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s###side_%d", labels[i], i);
			const int asset = (icons && icons[i] > 0) ? icons[i] : 0;
			if (Gw2Ui::RailToggle(buf, i == current, asset))
				current = i;
			ImGui::PopID();
		}

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::SameLine(0.f, 8.f);
		return current;
	}

	/* Toggle-style rail entry for helper chrome (open pads / flags). */
	inline bool SideToggle(const char* label, bool on, int assetId = 0)
	{
		return Gw2Ui::RailToggle(label, on, assetId);
	}
}
