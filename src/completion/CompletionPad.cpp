#include "CompletionPad.h"
#include "CompletionInternal.h"
#include "CompletionShared.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "Settings.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

bool CompletionPad::RenderAchievements()
{
	using namespace CompletionDetail;
	if (!G::ShowAchievements) return false;
	LoadAchPins();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	PadDock::SetSizeConstraints("Achievements##GW2InGameHelperAchievements",
		380.f, 280.f, PadDock::MaxW(640.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.58f) : 200.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.16f) : 120.f;
		PadDock::Place(G::PadAchievements, gAchPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gAchPlaceOnce && G::PadAchievements.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gAchFocus);

	bool open = G::ShowAchievements;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Achievements##GW2InGameHelperAchievements", &open,
		HelperTheme::PadFlags());
	if (!theme.AfterBegin("Achievements", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadAchievements)) Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open) { G::ShowAchievements = false; Settings::SetDirty(); }
		return hovered;
	}

	if (!open) { G::ShowAchievements = false; Settings::SetDirty(); }
	if (PadDock::Capture(G::PadAchievements)) Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	ImGui::BeginChild("###gw2igh_ap_body", ImVec2(0.f, 0.f), true);
	DrawAchievementsTab();
	if (gStatus[0])
		ImGui::TextColored(HelperTheme::Muted, "%s", gStatus);
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
