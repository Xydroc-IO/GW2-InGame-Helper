#include "SettingsPad.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

namespace
{
	bool gRequestDock = false;
}

void SettingsPad::Open()
{
	G::ShowSettings = true;
	gRequestDock = true;
	Settings::SetDirty();
}

bool SettingsPad::Render()
{
	if (!G::ShowSettings)
		return false;

	constexpr float kPadW = 440.f;
	constexpr float kPadH = 560.f;

	ImGui::SetNextWindowSizeConstraints(ImVec2(320.f, 280.f),
		ImVec2(PadDock::MaxW(520.f), PadDock::MaxH(720.f)));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	PadDock::Place(G::PadSettings, gRequestDock, kPadW, kPadH, PadDock::BesideHelper(kPadW));
	if (!gRequestDock && G::PadSettings.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);

	bool open = G::ShowSettings;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Settings###GW2InGameHelperSettings", &open))
	{
		if (PadDock::Capture(G::PadSettings))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows);
		ImGui::End();
		if (!open)
		{
			G::ShowSettings = false;
			Settings::SetDirty();
		}
		return hovered;
	}

	HelperTheme::ScopedFontScale fontScale(440.f, 560.f);
	ImGui::PushID("gw2igh_settings_pad");
	DrawContents();
	ImGui::PopID();

	if (PadDock::Capture(G::PadSettings))
		Settings::SetDirty();
	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_ChildWindows);
	ImGui::End();
	if (!open)
	{
		G::ShowSettings = false;
		Settings::SetDirty();
	}
	return hovered;
}
