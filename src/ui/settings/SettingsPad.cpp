#include "SettingsPad.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
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

	constexpr float kPadW = PadDock::kCompactW;
	constexpr float kPadH = PadDock::kCompactH;

	PadDock::SetSizeConstraints("Settings###GW2InGameHelperSettings", 360.f, 260.f,
		PadDock::MaxW(560.f), PadDock::MaxH(640.f));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	/* One-shot heal for absurdly bloated saves from older builds — never fight live resize. */
	static bool sHealedBloated = false;
	if (!sHealedBloated)
	{
		sHealedBloated = true;
		if (G::PadSettings.w > 900.f || G::PadSettings.h > 900.f)
		{
			G::PadSettings.w = kPadW;
			G::PadSettings.h = kPadH;
		}
	}
	PadDock::Place(G::PadSettings, gRequestDock, kPadW, kPadH, PadDock::BesideHelper(kPadW));
	if (!gRequestDock && G::PadSettings.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);

	bool open = G::ShowSettings;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Settings###GW2InGameHelperSettings", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Settings", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadSettings))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows);
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowSettings = false;
			Settings::SetDirty();
		}
		return hovered;
	}


	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
	ImGui::PushID("gw2igh_settings_pad");
	DrawContents();
	ImGui::PopID();

	if (PadDock::Capture(G::PadSettings))
		Settings::SetDirty();
	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_ChildWindows);
	HelperTheme::EndPad();
	if (!open)
	{
		G::ShowSettings = false;
		Settings::SetDirty();
	}
	return hovered;
}
