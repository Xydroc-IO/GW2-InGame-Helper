#pragma once

#include "Globals.h"
#include "UiScale.h"

#include "imgui/imgui.h"

/* Shared gold/dark chrome used by In-Game Helper Browse and themed pads. */
namespace HelperTheme
{
	inline const ImVec4 Gold(0.941f, 0.776f, 0.353f, 1.f);       /* #f0c65a */
	inline const ImVec4 GoldBright(1.f, 0.878f, 0.541f, 1.f);     /* #ffe08a */
	inline const ImVec4 GoldDim(0.788f, 0.635f, 0.153f, 1.f);     /* #c9a227 */
	inline const ImVec4 GoldMuted(0.75f, 0.62f, 0.32f, 0.88f);
	inline const ImVec4 Muted(0.659f, 0.682f, 0.722f, 1.f);       /* #a8aeb8 */
	inline const ImVec4 Bg(0.024f, 0.027f, 0.039f, 1.f);          /* #06070a */
	inline const ImVec4 Panel(0.071f, 0.078f, 0.102f, 1.f);       /* #12141a */
	inline const ImVec4 Border(0.353f, 0.290f, 0.157f, 0.95f);    /* #5a4a28 */
	inline const ImVec4 TabActive(0.36f, 0.28f, 0.12f, 1.f);
	inline const ImVec4 TabIdle(0.10f, 0.09f, 0.07f, 1.f);
	inline const ImVec4 Warn(0.90f, 0.55f, 0.28f, 1.f);
	inline const ImVec4 Ok(0.55f, 0.75f, 0.55f, 1.f);

	inline void Push()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.941f, 0.949f, 0.961f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, Muted);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bg);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.047f, 0.051f, 0.067f, 0.92f));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.059f, 0.078f, 0.98f));
		ImGui::PushStyleColor(ImGuiCol_Border, Border);
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.078f, 0.086f, 0.110f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.12f, 0.07f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.16f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.055f, 0.047f, 0.031f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.12f, 0.10f, 0.055f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.04f, 0.04f, 0.05f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, Panel);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.04f, 0.04f, 0.05f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.35f, 0.28f, 0.14f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.55f, 0.42f, 0.18f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, Gold);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, Gold);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.75f, 0.58f, 0.22f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.11f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.22f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.30f, 0.12f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.18f, 0.09f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.35f, 0.28f, 0.12f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.45f, 0.35f, 0.14f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.36f, 0.16f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, Gold);
		ImGui::PushStyleColor(ImGuiCol_SeparatorActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(0.45f, 0.36f, 0.16f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, Gold);
		ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_Tab, TabIdle);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.30f, 0.24f, 0.11f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabActive, TabActive);
		ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0.08f, 0.08f, 0.09f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0.22f, 0.18f, 0.09f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.12f, 0.10f, 0.055f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0.45f, 0.36f, 0.16f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0.35f, 0.28f, 0.14f, 0.35f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.10f, 0.09f, 0.06f, 0.35f));
		ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.55f, 0.42f, 0.15f, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_NavHighlight, Gold);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 12.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.f, 5.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 7.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.f);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
	}

	inline void Pop()
	{
		ImGui::PopStyleVar(14);
		ImGui::PopStyleColor(45);
	}

	/* RAII: push theme + opacity before Begin; pop on scope exit (safe on early return). */
	struct ScopedWindow
	{
		explicit ScopedWindow(float opacity)
		{
			Push();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, opacity);
			ImGui::SetNextWindowBgAlpha(opacity);
		}
		~ScopedWindow()
		{
			ImGui::PopStyleVar();
			Pop();
		}
		ScopedWindow(const ScopedWindow&) = delete;
		ScopedWindow& operator=(const ScopedWindow&) = delete;
	};

	/* RAII: apply FontScale × window-size factor after Begin.
	   Do NOT reset after End() — that marks ImGui's Debug##Default. */
	struct ScopedFontScale
	{
		explicit ScopedFontScale(float refW = 560.f, float refH = 700.f)
		{
			ImGui::SetWindowFontScale(UiScale::EffectiveFontScale(refW, refH));
		}
		~ScopedFontScale() = default;
		ScopedFontScale(const ScopedFontScale&) = delete;
		ScopedFontScale& operator=(const ScopedFontScale&) = delete;
	};
}
