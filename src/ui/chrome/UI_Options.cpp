#include "UI.h"

#include "Settings.h"
#include "SettingsPad.h"

#include "imgui/imgui.h"

/* Nexus Options - stub only. Real settings live on the helper Settings pad. */
void UI_Options()
{
	ImGui::PushID("gw2igh_opts");
	ImGui::TextWrapped(
		"Open Settings from the helper side rail for opacity, font, API key, "
		"and the Keybinds tab (panel chords). Helper open is Ctrl+Shift+H / QuickAccess.");
	ImGui::Spacing();
	if (ImGui::Button("Open Settings###gw2igh_open_settings", ImVec2(-1.f, 0.f)))
		SettingsPad::Open();
	ImGui::Spacing();
	ImGui::TextWrapped(
		"Panel hotkeys live in Settings -> Keybinds (not Nexus Options).");
	Settings::Save(false);
	ImGui::PopID();
}
