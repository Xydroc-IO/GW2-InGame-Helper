#include "CompletionPad.h"
#include "CompletionInternal.h"
#include "CompletionShared.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

bool CompletionPad::Render()
{
	using namespace CompletionDetail;
	if (!G::ShowCompletion) return false;
	EnsureCatalog();
	LoadChecklist();
	LoadFavorites();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	PadDock::SetSizeConstraints("Completion##GW2InGameHelperCompletion", 380.f, 280.f, PadDock::MaxW(560.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.42f) : 160.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.14f) : 100.f;
		PadDock::Place(G::PadCompletion, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadCompletion.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

	bool open = G::ShowCompletion;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Completion##GW2InGameHelperCompletion", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Completion", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadCompletion)) Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open) { G::ShowCompletion = false; Settings::SetDirty(); }
		return hovered;
	}

	if (!open) { G::ShowCompletion = false; Settings::SetDirty(); }
	if (PadDock::Capture(G::PadCompletion)) Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	if (gTabSelectOnce)
		gTabSelectOnce = false;
	if (gTab > 2)
		gTab = 0;

	static const char* kTabs[] = { "Checklist", "Atlas", "Route" };
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Check),
		static_cast<int>(Gw2Ui::Icon::Map),
		static_cast<int>(Gw2Ui::Icon::Story),
	};
	gTab = PadNav::DrawSideRail("###gw2igh_cmp_nav", kTabs, 3, gTab, 0.f, kTabIcons);
	ImGui::BeginChild("###gw2igh_cmp_body", ImVec2(0.f, 0.f), true);
	PadNav::Blurb(
		"Checklist / Atlas / Route. Local ticks. "
		"Hearts/HP from curated packs. GPS = orange guide only.");
	if (const MapInfo* m = FindMap(gFocusMapId))
		ImGui::TextColored(HelperTheme::Muted, "%s | %s | %s",
			m->name,
			m->release[0] ? m->release : DefaultRelease(),
			m->region[0] ? m->region : DefaultRegion());
	else
		ImGui::TextColored(HelperTheme::Muted, "No map selected");
	ImGui::Separator();
	switch (gTab)
	{
	case 0: DrawChecklistTab(); break;
	case 1: DrawAtlasTab(); break;
	case 2: DrawRouteTab(); break;
	default: break;
	}
	if (gStatus[0])
		ImGui::TextColored(HelperTheme::Muted, "%s", gStatus);
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}

bool CompletionPad::RenderAchievements()
{
	using namespace CompletionDetail;
	if (!G::ShowAchievements) return false;
	LoadAchPins();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	PadDock::SetSizeConstraints("Achievements##GW2InGameHelperAchievements", 380.f, 280.f, PadDock::MaxW(640.f), maxH);
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
	const bool padBody = ImGui::Begin("Achievements##GW2InGameHelperAchievements", &open, HelperTheme::PadFlags());
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
