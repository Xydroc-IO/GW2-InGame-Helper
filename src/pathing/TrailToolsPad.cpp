#include "TrailToolsPad.h"
#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

namespace
{
	constexpr float kPadW = 640.f;
	constexpr float kPadH = 720.f;
}

void TrailToolsPad::Open()
{
	G::ShowTrailTools = true;
	TrailToolsDetail::gPlaceOnce = true;
	TrailToolsDetail::gFocus = true;
	Settings::SetDirty();
}

bool TrailToolsPad::Render()
{
	using namespace TrailToolsDetail;
	if (!G::ShowTrailTools)
		return false;

	const float maxH = PadDock::MaxH(360.f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(480.f, 320.f), ImVec2(PadDock::MaxW(860.f), maxH));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	PadDock::Place(G::PadTrailTools, gPlaceOnce, kPadW, kPadH, PadDock::BesideHelper(kPadW));
	if (!gPlaceOnce && G::PadTrailTools.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowTrailTools;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Trail Tools##GW2InGameHelperTrailTools", &open,
		ImGuiWindowFlags_NoNavInputs))
	{
		if (PadDock::Capture(G::PadTrailTools))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		ImGui::End();
		if (!open)
		{
			G::ShowTrailTools = false;
			Settings::SetDirty();
		}
		return hovered || (focused && ImGui::GetIO().WantTextInput);
	}
	if (!open)
	{
		G::ShowTrailTools = false;
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadTrailTools))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale;

	ImGui::TextColored(HelperTheme::Gold, "TRAIL TOOLS");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Author TacO/Blish packs — live coords, record trails, place markers, build .taco.");
	PadNav::PopWrap();

	static const char* kTabs[] = { "Live", "Trail", "Markers", "Pack" };
	gTab = PadNav::DrawSideRail("###gw2igh_tt_nav", kTabs, 4, gTab);

	ImGui::BeginChild("###gw2igh_tt_body", ImVec2(0.f, 0.f), true);
	switch (gTab)
	{
	case 0: DrawLiveTab(); break;
	case 1: DrawTrailTab(); break;
	case 2: DrawMarkersTab(); break;
	case 3: DrawPackTab(); break;
	default: break;
	}
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	const bool typingHere = focused && ImGui::GetIO().WantTextInput;
	ImGui::End();
	return hovered || typingHere;
}
