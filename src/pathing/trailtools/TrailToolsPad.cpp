#include "TrailToolsPad.h"
#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"

#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <functional>

namespace
{
	constexpr float kHubW = 600.f;
	constexpr float kHubH = 720.f;
	constexpr float kEditW = 480.f;
	constexpr float kEditH = 560.f;

	bool RenderCollapsiblePad(
		const char* title,
		bool& showFlag,
		G::PadGeom& geom,
		bool& placeOnce,
		bool& focus,
		float defW,
		float defH,
		ImVec2 fallbackPos,
		const std::function<void()>& body)
	{
		if (!showFlag)
			return false;

		const float maxH = PadDock::MaxH(320.f);
		ImGui::SetNextWindowSizeConstraints(ImVec2(320.f, 120.f), ImVec2(PadDock::MaxW(780.f), maxH));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
		PadDock::Place(geom, placeOnce, defW, defH, fallbackPos);
		if (!placeOnce && geom.w < 80.f)
			ImGui::SetNextWindowSize(ImVec2(defW, defH), ImGuiCond_FirstUseEver);
		if (focus)
		{
			ImGui::SetNextWindowFocus();
			focus = false;
		}

		bool open = showFlag;
		HelperTheme::ScopedWindow theme(G::Opacity);
		if (!ImGui::Begin(title, &open, ImGuiWindowFlags_NoNavInputs))
		{
			const ImVec2 p = ImGui::GetWindowPos();
			if (std::fabs(p.x - geom.x) > 0.5f || std::fabs(p.y - geom.y) > 0.5f)
			{
				geom.x = p.x;
				geom.y = p.y;
				Settings::SetDirty();
			}
			const bool hovered = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
			const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			ImGui::End();
			if (!open)
			{
				showFlag = false;
				Settings::SetDirty();
			}
			return hovered || (focused && ImGui::GetIO().WantTextInput);
		}
		if (!open)
		{
			showFlag = false;
			Settings::SetDirty();
		}
		if (!ImGui::IsWindowCollapsed() && PadDock::Capture(geom))
			Settings::SetDirty();

		HelperTheme::ScopedFontScale fontScale;
		body();

		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool focusedWin = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		const bool typingHere = focusedWin && ImGui::GetIO().WantTextInput;
		ImGui::End();
		return hovered || typingHere;
	}
}

bool TrailToolsPad::AnyOpen()
{
	return TrailToolsDetail::AnyAuthoringPadOpen();
}

void TrailToolsPad::Open()
{
	G::ShowTrailTools = true;
	TrailToolsDetail::gPlaceOnce = true;
	TrailToolsDetail::gFocus = true;
	Settings::SetDirty();
}

void TrailToolsPad::OpenTrailsWindow()
{
	using namespace TrailToolsDetail;
	gPopoutTrails = true;
	gPlaceOnceTrails = true;
	gFocusTrails = true;
	gTab = 1;
	Settings::SetDirty();
}

void TrailToolsPad::OpenMarkersWindow()
{
	using namespace TrailToolsDetail;
	gPopoutMarkers = true;
	gPlaceOnceMarkers = true;
	gFocusMarkers = true;
	gTab = 2;
	Settings::SetDirty();
}

bool TrailToolsPad::Render()
{
	using namespace TrailToolsDetail;
	bool hover = false;

	if (G::ShowTrailTools)
	{
		const float maxH = PadDock::MaxH(320.f);
		ImGui::SetNextWindowSizeConstraints(ImVec2(440.f, 280.f), ImVec2(PadDock::MaxW(820.f), maxH));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
		PadDock::Place(G::PadTrailTools, gPlaceOnce, kHubW, kHubH, PadDock::BesideHelper(kHubW));
		if (!gPlaceOnce && G::PadTrailTools.w < 80.f)
			ImGui::SetNextWindowSize(ImVec2(kHubW, kHubH), ImGuiCond_FirstUseEver);
		if (gFocus)
		{
			ImGui::SetNextWindowFocus();
			gFocus = false;
		}

		char title[280]{};
		std::snprintf(title, sizeof(title), "Trail Tools%s###GW2InGameHelperTrailTools",
			gDraft.xmlDirty || gDraft.trailDirty ? " *" : "");

		bool open = G::ShowTrailTools;
		HelperTheme::ScopedWindow theme(G::Opacity);
		if (!ImGui::Begin(title, &open, ImGuiWindowFlags_NoNavInputs))
		{
			const ImVec2 p = ImGui::GetWindowPos();
			if (std::fabs(p.x - G::PadTrailTools.x) > 0.5f ||
				std::fabs(p.y - G::PadTrailTools.y) > 0.5f)
			{
				G::PadTrailTools.x = p.x;
				G::PadTrailTools.y = p.y;
				Settings::SetDirty();
			}
			hover = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ||
				(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
					ImGui::GetIO().WantTextInput);
			ImGui::End();
			if (!open)
			{
				G::ShowTrailTools = false;
				Settings::SetDirty();
			}
		}
		else
		{
			if (!open)
			{
				G::ShowTrailTools = false;
				Settings::SetDirty();
			}
			if (!ImGui::IsWindowCollapsed() && PadDock::Capture(G::PadTrailTools))
				Settings::SetDirty();

			HelperTheme::ScopedFontScale fontScale;

			ImGui::TextColored(HelperTheme::Gold, "TRAIL TOOLS");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Author packs here, or Open Trails / Markers in their own windows "
				"(collapse the title-bar arrow to leave only a bar).");
			PadNav::PopWrap();

			static const char* kTabs[] = { "Live", "Trails", "Markers", "Pack", "Keybinds" };
			static const int kTabIcons[] = {
				static_cast<int>(Gw2Ui::Icon::Map),
				static_cast<int>(Gw2Ui::Icon::Inventory),
				static_cast<int>(Gw2Ui::Icon::Alert),
				static_cast<int>(Gw2Ui::Icon::Bag),
				static_cast<int>(Gw2Ui::Icon::Options),
			};
			gTab = PadNav::DrawSideRail("###gw2igh_tt_nav", kTabs, 5, gTab < 0 || gTab > 4 ? 0 : gTab, 0.f, kTabIcons);

			ImGui::BeginChild("###gw2igh_tt_body", ImVec2(0.f, 0.f), true);
			if (gTab == 0)
				DrawLiveTab();
			else if (gTab == 1)
				DrawTrailDesk();
			else if (gTab == 2)
				DrawMarkersDesk();
			else if (gTab == 3)
				DrawPackTab();
			else
				DrawKeybindsTab();
			ImGui::EndChild();

			hover = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ||
				(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
					ImGui::GetIO().WantTextInput);
			ImGui::End();
		}
	}

	if (gPopoutTrails)
	{
		char title[280]{};
		const char* stem = gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail";
		std::snprintf(title, sizeof(title), "Trails1 - %s.trl%s###GW2InGameHelperTrailPopout",
			stem, gDraft.trailDirty ? " *" : "");
		hover = RenderCollapsiblePad(
			title,
			gPopoutTrails,
			G::PadTrailEditor,
			gPlaceOnceTrails,
			gFocusTrails,
			kEditW,
			kEditH,
			PadDock::ForTrailPopout(kEditW, kEditH),
			[]() {
				ImGui::TextColored(HelperTheme::Gold, "TRAILS1");
				ImGui::TextDisabled("Raw .trl editor — collapse title bar to shrink");
				ImGui::Separator();
				ImGui::BeginChild("###gw2igh_tr_po_body", ImVec2(0.f, 0.f), true);
				DrawTrailRawEditor();
				ImGui::EndChild();
			}) || hover;
	}

	if (gPopoutMarkers)
	{
		char title[96]{};
		std::snprintf(title, sizeof(title), "Markers1%s###GW2InGameHelperMarkerPopout",
			gDraft.selectedPoi >= 0 ? "" : " (none)");
		hover = RenderCollapsiblePad(
			title,
			gPopoutMarkers,
			G::PadMarkerEditor,
			gPlaceOnceMarkers,
			gFocusMarkers,
			kEditW,
			kEditH,
			PadDock::ForMarkerPopout(kEditW, kEditH),
			[]() {
				ImGui::TextColored(HelperTheme::Gold, "MARKERS1");
				ImGui::TextDisabled("Raw marker attrs — collapse title bar to shrink");
				ImGui::Separator();
				ImGui::BeginChild("###gw2igh_mk_po_body", ImVec2(0.f, 0.f), true);
				DrawMarkerRawEditor();
				ImGui::EndChild();
			}) || hover;
	}

	return hover;
}
