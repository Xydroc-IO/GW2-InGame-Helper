#include "DirectionCompass.h"
#include "DirectionCompassInternal.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

void DirectionCompass::DrawControls()
{
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"World N/E/S/W around your character. Reads Nexus FontBig; does not change Nexus fonts.");
	PadNav::PopWrap();
	if (ImGui::Checkbox("Enable direction compass###gw2igh_dircompass_pad", &G::ShowDirectionCompass))
		Settings::SetDirty();

	/* Labels above sliders — right-side ImGui labels clip in narrow pads. */
	if (PadNav::SliderFloatRow("Letter size", "gw2igh_dirletters_pad",
			&G::DirectionLetterScale, 0.5f, 2.5f, "%.2fx"))
		Settings::SetDirty();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Scales N/E/S/W draw size (1.00x ≈ FontBig / ~48px if Font/UI).\n"
			"Does not touch FontGlobalScale or Nexus fonts.");

	if (PadNav::SliderFloatRow("World radius", "gw2igh_dirradius_pad",
			&G::DirectionWorldRadiusScale, 0.4f, 3.0f, "%.2fx"))
		Settings::SetDirty();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How far N/E/S/W sit from your character (hitbox base x this).");
}

void DirectionCompass::Open()
{
	G::ShowCompassPad = true;
	DirectionCompassDetail::gRequestDock = true;
	/* Match Watch: heal tiny geom + clear sticky minimize so reopen after
	   Mirror / other pads is a usable window, not a dead title strip. */
	if (G::PadCompass.w < 80.f || G::PadCompass.h < 80.f)
	{
		G::PadCompass.w = 0.f;
		G::PadCompass.h = 0.f;
	}
	if (ImGuiWindow* w = ImGui::FindWindowByName("Compass###GW2InGameHelperCompass"))
		w->StateStorage.SetBool(w->GetID("##gw2igh_pad_collapsed"), false);
	Settings::SetDirty();
}

bool DirectionCompass::RenderPad()
{
	try
	{
		if (!G::ShowCompassPad)
			return false;

		constexpr float kPadW = PadDock::kCompactW;
		constexpr float kPadH = PadDock::kCompassH;

		PadDock::SetSizeConstraints("Compass###GW2InGameHelperCompass", 380.f, 260.f,
			PadDock::MaxW(520.f), PadDock::MaxH(360.f));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
		PadDock::Place(G::PadCompass, DirectionCompassDetail::gRequestDock, kPadW, kPadH,
			PadDock::BesideHelper(kPadW));
		if (!DirectionCompassDetail::gRequestDock &&
			(G::PadCompass.w < 80.f || G::PadCompass.h < 80.f))
			ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);

		bool open = G::ShowCompassPad;
		HelperTheme::ScopedWindow theme(G::Opacity);
		const bool padBody = ImGui::Begin("Compass###GW2InGameHelperCompass", &open, HelperTheme::PadFlags());
		if (!theme.AfterBegin("Compass", &open) || !padBody)
		{
			if (PadDock::Capture(G::PadCompass))
				Settings::SetDirty();
			const bool hovered = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
				ImGuiHoveredFlags_ChildWindows);
			HelperTheme::EndPad();
			if (!open)
			{
				G::ShowCompassPad = false;
				Settings::SetDirty();
			}
			return hovered;
		}

		HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
		DrawControls();

		if (PadDock::Capture(G::PadCompass))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows);
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowCompassPad = false;
			Settings::SetDirty();
		}
		return hovered;
	}
	catch (...)
	{
		return false;
	}
}
