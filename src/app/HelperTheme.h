#pragma once

#include "Globals.h"
#include "UiScale.h"

#include "imgui/imgui.h"

/* Dark-warm parchment + gold — aged map-board / leather plaque.
   No corner brackets or fake frames. Shared by Browse, pads, overlays. */
namespace HelperTheme
{
	inline const ImVec4 Gold(0.94f, 0.78f, 0.38f, 1.f);
	inline const ImVec4 GoldBright(1.f, 0.90f, 0.55f, 1.f);
	inline const ImVec4 GoldDim(0.76f, 0.58f, 0.22f, 1.f);
	inline const ImVec4 GoldMuted(0.80f, 0.68f, 0.42f, 1.f);
	/* Warm ink on parchment (not cool slate gray). */
	inline const ImVec4 Ink(0.96f, 0.93f, 0.86f, 1.f);
	inline const ImVec4 Muted(0.72f, 0.68f, 0.58f, 1.f);
	/* Dark parchment boards. */
	inline const ImVec4 Bg(0.07f, 0.055f, 0.040f, 0.97f);
	inline const ImVec4 Panel(0.11f, 0.090f, 0.065f, 1.f);
	inline const ImVec4 Child(0.095f, 0.078f, 0.055f, 0.95f);
	inline const ImVec4 Border(0.55f, 0.42f, 0.20f, 0.92f);
	inline const ImVec4 TabActive(0.32f, 0.24f, 0.12f, 1.f);
	inline const ImVec4 TabIdle(0.10f, 0.085f, 0.060f, 1.f);
	inline const ImVec4 Header(0.28f, 0.22f, 0.12f, 0.95f);
	inline const ImVec4 Warn(0.92f, 0.55f, 0.28f, 1.f);
	inline const ImVec4 Ok(0.52f, 0.72f, 0.42f, 1.f);

	inline void Push()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, Ink);
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, Muted);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bg);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, Child);
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.065f, 0.045f, 0.98f));
		ImGui::PushStyleColor(ImGuiCol_Border, Border);
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.10f, 0.07f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.14f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.24f, 0.18f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.08f, 0.065f, 0.045f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.14f, 0.11f, 0.07f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.06f, 0.05f, 0.035f, 0.88f));
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, Panel);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.05f, 0.04f, 0.03f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.42f, 0.32f, 0.16f, 0.90f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.58f, 0.44f, 0.20f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, Gold);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, Gold);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, GoldDim);
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.12f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.22f, 0.12f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.28f, 0.14f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Header, Header);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.34f, 0.26f, 0.14f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.42f, 0.32f, 0.16f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.48f, 0.36f, 0.18f, 0.50f));
		ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, Gold);
		ImGui::PushStyleColor(ImGuiCol_SeparatorActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(0.48f, 0.36f, 0.18f, 0.40f));
		ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, Gold);
		ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_Tab, TabIdle);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.28f, 0.22f, 0.12f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabActive, TabActive);
		ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0.09f, 0.07f, 0.05f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0.20f, 0.16f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.14f, 0.11f, 0.07f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0.48f, 0.36f, 0.18f, 0.50f));
		ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0.36f, 0.28f, 0.14f, 0.32f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.12f, 0.10f, 0.07f, 0.35f));
		ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.52f, 0.40f, 0.16f, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_NavHighlight, Gold);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 5.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 6.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 14.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
	}

	inline void Pop()
	{
		ImGui::PopStyleVar(15);
		ImGui::PopStyleColor(45);
	}

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

	/* Floating overlays — themed chrome with a custom bg alpha. */
	struct ScopedOverlay
	{
		explicit ScopedOverlay(float bgAlpha)
		{
			Push();
			ImGui::SetNextWindowBgAlpha(bgAlpha);
		}
		~ScopedOverlay()
		{
			Pop();
		}
		ScopedOverlay(const ScopedOverlay&) = delete;
		ScopedOverlay& operator=(const ScopedOverlay&) = delete;
	};

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
