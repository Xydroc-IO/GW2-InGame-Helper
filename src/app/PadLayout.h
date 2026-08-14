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

	inline void Chip(const char* label, ImVec4 fill, ImVec4 text)
	{
		if (!label || !label[0])
			return;
		const ImVec2 pad(5.f, 1.f);
		const ImVec2 sz = ImGui::CalcTextSize(label);
		const float h = ImGui::GetTextLineHeight();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const float w = sz.x + pad.x * 2.f;
		const float ty = p.y + (h - sz.y) * 0.5f;
		ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + h),
			ImGui::GetColorU32(fill), 3.f);
		ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + pad.x, ty),
			ImGui::GetColorU32(text), label);
		ImGui::Dummy(ImVec2(w, h));
	}

	/* Optional chip + name on the left, primary number pinned right. One line. */
	inline void TitleRow(const char* chip, ImVec4 chipFill, ImVec4 chipText,
		const char* name, const char* value, ImVec4 valueCol)
	{
		if (!name)
			name = "";
		if (!value)
			value = "";
		const float x0 = ImGui::GetCursorPosX();
		const float y0 = ImGui::GetCursorPosY();
		const float avail = ImGui::GetContentRegionAvail().x;
		const float lineH = ImGui::GetTextLineHeight();
		float nameX = x0;
		if (chip && chip[0])
		{
			Chip(chip, chipFill, chipText);
			ImGui::SameLine(0.f, 6.f);
			nameX = ImGui::GetCursorPosX();
		}

		ImGui::PushTextWrapPos(-1.f);
		const ImVec2 valSz = ImGui::CalcTextSize(value);
		const float valX = (avail > valSz.x) ? (x0 + avail - valSz.x) : x0;
		ImGui::SetCursorPos(ImVec2(valX, y0));
		ImGui::TextColored(valueCol, "%s", value);
		ImGui::PopTextWrapPos();

		const float nameW = valX - nameX - 8.f;
		ImGui::SetCursorPos(ImVec2(nameX, y0));
		if (nameW > 12.f)
		{
			const ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), p,
				ImVec2(p.x + nameW, p.y + lineH), p.x + nameW, p.x + nameW,
				name, nullptr, nullptr);
			ImGui::Dummy(ImVec2(nameW, lineH));
		}
		ImGui::SetCursorPos(ImVec2(x0, y0 + ImGui::GetTextLineHeightWithSpacing()));
	}

	inline void NameAndValue(const char* name, const char* value, ImVec4 valueCol)
	{
		TitleRow(nullptr, ImVec4(), ImVec4(), name, value, valueCol);
	}

	/* Kicker + time on line 1, ellipsized title on line 2. */
	inline void Hero(const char* id, const char* kicker, const char* title, const char* value)
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, HelperTheme::Header);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
		const float h = ImGui::GetTextLineHeightWithSpacing() * 2.15f
			+ ImGui::GetStyle().WindowPadding.y * 2.f;
		ImGui::BeginChild(id && id[0] ? id : "###pad_hero", ImVec2(0.f, h), true,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		NameAndValue(kicker ? kicker : "", value ? value : "", HelperTheme::GoldBright);
		if (title && title[0])
		{
			const float avail = ImGui::GetContentRegionAvail().x;
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float lineH = ImGui::GetTextLineHeight();
			ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), p,
				ImVec2(p.x + avail, p.y + lineH),
				p.x + avail, p.x + avail, title, nullptr, nullptr);
			ImGui::Dummy(ImVec2(avail, lineH));
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	/* Full pad actions — not SmallButton chips. Primary = gold fill. */
	inline bool GoldButton(const char* label, bool primary = false, bool first = false)
	{
		if (!label || !label[0])
			return false;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 7.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
		const float w = PadNav::VisibleLabelWidth(label) + ImGui::GetStyle().FramePadding.x * 2.f;
		if (!first)
			PadNav::WrapSameLine(w);
		if (primary)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::Header);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.30f, 0.14f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);
			ImGui::PushStyleColor(ImGuiCol_Border, HelperTheme::Gold);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.07f, 0.055f, 0.038f, 0.96f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.14f, 0.08f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Ink);
			ImGui::PushStyleColor(ImGuiCol_Border, HelperTheme::GoldDim);
		}
		const bool clicked = ImGui::Button(label);
		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar(3);
		return clicked;
	}
}
