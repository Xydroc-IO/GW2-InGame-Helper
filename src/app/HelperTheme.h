#pragma once

#include "Globals.h"
#include "UiScale.h"

#include "imgui/imgui.h"

/* Dark-warm parchment + gold — Tyrian map-board / leather plaque immersion.
   Soft depth via color hierarchy; no copied third-party art or fonts. */
namespace HelperTheme
{
	inline const ImVec4 Gold(0.94f, 0.77f, 0.35f, 1.f);
	inline const ImVec4 GoldBright(1.f, 0.91f, 0.63f, 1.f);
	inline const ImVec4 GoldDim(0.76f, 0.58f, 0.22f, 1.f);
	inline const ImVec4 GoldMuted(0.72f, 0.60f, 0.36f, 1.f);
	/* Warm ink on parchment (not cool slate gray). */
	inline const ImVec4 Ink(0.96f, 0.94f, 0.87f, 1.f);
	inline const ImVec4 Muted(0.71f, 0.66f, 0.56f, 1.f);
	/* Dark leather / aged wood boards. */
	inline const ImVec4 Bg(0.055f, 0.043f, 0.031f, 0.98f);
	inline const ImVec4 Panel(0.12f, 0.098f, 0.070f, 1.f);
	inline const ImVec4 Child(0.085f, 0.068f, 0.048f, 0.97f);
	inline const ImVec4 Border(0.63f, 0.47f, 0.22f, 0.95f);
	inline const ImVec4 TabActive(0.28f, 0.21f, 0.11f, 1.f);
	inline const ImVec4 TabIdle(0.075f, 0.060f, 0.042f, 1.f);
	inline const ImVec4 Header(0.30f, 0.23f, 0.12f, 0.96f);
	inline const ImVec4 Warn(0.92f, 0.55f, 0.28f, 1.f);
	inline const ImVec4 Ok(0.52f, 0.72f, 0.42f, 1.f);

	inline void Push()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, Ink);
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, Muted);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bg);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, Child);
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.055f, 0.038f, 0.99f));
		ImGui::PushStyleColor(ImGuiCol_Border, Border);
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.f, 0.f, 0.f, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.082f, 0.055f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.17f, 0.13f, 0.075f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.23f, 0.17f, 0.095f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.07f, 0.055f, 0.038f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.14f, 0.11f, 0.065f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.05f, 0.04f, 0.028f, 0.90f));
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, Panel);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.04f, 0.032f, 0.022f, 0.70f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.48f, 0.36f, 0.16f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.65f, 0.48f, 0.20f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, Gold);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, Gold);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, GoldDim);
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.10f, 0.065f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.20f, 0.11f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.27f, 0.13f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Header, Header);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.34f, 0.26f, 0.13f, 0.96f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.42f, 0.32f, 0.15f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.55f, 0.40f, 0.18f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, Gold);
		ImGui::PushStyleColor(ImGuiCol_SeparatorActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(0.48f, 0.36f, 0.18f, 0.40f));
		ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, Gold);
		ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, GoldBright);
		ImGui::PushStyleColor(ImGuiCol_Tab, TabIdle);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.26f, 0.20f, 0.11f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabActive, TabActive);
		ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0.07f, 0.055f, 0.038f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0.18f, 0.14f, 0.085f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.14f, 0.11f, 0.065f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0.55f, 0.40f, 0.18f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0.36f, 0.28f, 0.14f, 0.32f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.12f, 0.10f, 0.065f, 0.38f));
		ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.52f, 0.40f, 0.16f, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_NavHighlight, Gold);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 5.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 6.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 15.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
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
