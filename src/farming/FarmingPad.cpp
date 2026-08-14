#include "FarmingPad.h"
#include "FarmingInternal.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

bool FarmingPad::Render()
{
	using namespace FarmingDetail;
	if (!G::ShowFarming) return false;
	EnsureCatalog();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(260.f);
	PadDock::SetSizeConstraints("Farming##GW2InGameHelperFarming", 480.f, 300.f, PadDock::MaxW(780.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.46f) : 180.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.18f) : 120.f;
		PadDock::Place(G::PadFarming, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadFarming.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

	bool open = G::ShowFarming;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Farming##GW2InGameHelperFarming", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Farming", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadFarming)) Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open) { G::ShowFarming = false; Settings::SetDirty(); }
		return hovered;
	}

	if (!open) { G::ShowFarming = false; Settings::SetDirty(); }
	if (PadDock::Capture(G::PadFarming)) Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	static const char* kTabs[] = { "Runs", "Fishing" };
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Map),
		static_cast<int>(Gw2Ui::Icon::Bag),
	};
	gTab = PadNav::DrawSideRail("###gw2igh_farm_nav", kTabs, 2, gTab, 0.f, kTabIcons);

	ImGui::BeginChild("###gw2igh_farm_body", ImVec2(0.f, 0.f), true);
	PadNav::Blurb("Pick a run, start pathing, check steps as you go.");
	ImGui::Separator();

	if (gTab == 0)
		DrawRunsTab();
	else
		DrawFishingTab();

	if (gStatus[0])
		ImGui::TextColored(HelperTheme::Muted, "%s", gStatus);
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
