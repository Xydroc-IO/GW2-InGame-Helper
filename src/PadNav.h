#pragma once

#include "HelperTheme.h"

#include "imgui/imgui.h"

/* Wrapping pad section nav — replaces ImGui tab bars that scroll with ◀ ▶.
   Labels wrap onto new rows when the window is narrow. */
namespace PadNav
{
	inline bool WrapButton(const char* label, bool selected = false)
	{
		const float gap = ImGui::GetStyle().ItemSpacing.x;
		const ImVec2 sz = ImGui::CalcTextSize(label);
		const float btnW = sz.x + ImGui::GetStyle().FramePadding.x * 2.f;
		const bool midLine = ImGui::GetCursorPosX() > ImGui::GetCursorStartPos().x + 1.f;
		if (midLine)
		{
			if (ImGui::GetContentRegionAvail().x < btnW)
				ImGui::NewLine();
			else
				ImGui::SameLine(0.f, gap);
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
		const float gap = 4.f;
		const float slashW = ImGui::CalcTextSize("/").x;
		const bool midLine = ImGui::GetCursorPosX() > ImGui::GetCursorStartPos().x + 1.f;
		if (midLine)
		{
			if (ImGui::GetContentRegionAvail().x < slashW + gap)
				ImGui::NewLine();
			else
				ImGui::SameLine(0.f, gap);
		}
		ImGui::TextDisabled("/");
	}

	/* labels[count] — returns selected index (unchanged if none clicked). */
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
			if (WrapButton(labels[i], i == current))
				current = i;
			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::Spacing();
		ImGui::Separator();
		return current;
	}
}
