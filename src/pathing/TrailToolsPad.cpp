#include "TrailToolsPad.h"
#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <functional>

namespace
{
	constexpr float kHubW = 560.f;
	constexpr float kHubH = 640.f;
	constexpr float kEditW = 480.f;
	constexpr float kEditH = 560.f;

	bool RenderPadWindow(
		const char* title,
		bool& showFlag,
		G::PadGeom& geom,
		bool& placeOnce,
		bool& focus,
		float defW,
		float defH,
		const std::function<void()>& body)
	{
		if (!showFlag)
			return false;

		const float maxH = PadDock::MaxH(320.f);
		ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 280.f), ImVec2(PadDock::MaxW(780.f), maxH));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
		PadDock::Place(geom, placeOnce, defW, defH, PadDock::BesideHelper(defW));
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
			if (PadDock::Capture(geom))
				Settings::SetDirty();
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
		if (PadDock::Capture(geom))
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

void TrailToolsPad::OpenTrails()
{
	G::ShowTrailEditor = true;
	TrailToolsDetail::gPlaceOnceTrails = true;
	TrailToolsDetail::gFocusTrails = true;
	Settings::SetDirty();
}

void TrailToolsPad::OpenMarkers()
{
	G::ShowMarkerEditor = true;
	TrailToolsDetail::gPlaceOnceMarkers = true;
	TrailToolsDetail::gFocusMarkers = true;
	Settings::SetDirty();
}

bool TrailToolsPad::Render()
{
	using namespace TrailToolsDetail;
	return RenderPadWindow(
		"Trail Tools##GW2InGameHelperTrailTools",
		G::ShowTrailTools,
		G::PadTrailTools,
		gPlaceOnce,
		gFocus,
		kHubW,
		kHubH,
		[]() {
			ImGui::TextColored(HelperTheme::Gold, "TRAIL TOOLS");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Pack hub — Live coords + build. Open Trails and Markers as separate windows "
				"so you can place markers along a trail.");
			PadNav::PopWrap();

			if (ImGui::Button("Open Trails###gw2igh_tt_open_trails"))
				TrailToolsPad::OpenTrails();
			PadNav::WrapSameLine(PadNav::ButtonWidth("Open Markers"));
			if (ImGui::Button("Open Markers###gw2igh_tt_open_marks"))
				TrailToolsPad::OpenMarkers();

			static const char* kTabs[] = { "Live", "Pack" };
			gTab = PadNav::DrawSideRail("###gw2igh_tt_nav", kTabs, 2, gTab > 1 ? 0 : gTab);

			ImGui::BeginChild("###gw2igh_tt_body", ImVec2(0.f, 0.f), true);
			if (gTab == 0)
				DrawLiveTab();
			else
				DrawPackTab();
			ImGui::EndChild();
		});
}

bool TrailToolsPad::RenderTrails()
{
	using namespace TrailToolsDetail;
	char title[280]{};
	const char* stem = gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail";
	std::snprintf(title, sizeof(title), "Trails — %s.trl%s###GW2InGameHelperTrailEditor",
		stem, gDraft.trailDirty ? " *" : "");
	return RenderPadWindow(
		title,
		G::ShowTrailEditor,
		G::PadTrailEditor,
		gPlaceOnceTrails,
		gFocusTrails,
		kEditW,
		kEditH,
		[]() {
			ImGui::TextColored(HelperTheme::Gold, "TRAILS");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Record path points. Keep Markers open beside this to drop POIs along the route.");
			PadNav::PopWrap();
			if (ImGui::SmallButton("Open Markers###gw2igh_te_open_m"))
				TrailToolsPad::OpenMarkers();
			ImGui::Separator();
			ImGui::BeginChild("###gw2igh_te_body", ImVec2(0.f, 0.f), true);
			DrawTrailTab();
			ImGui::EndChild();
		});
}

bool TrailToolsPad::RenderMarkers()
{
	using namespace TrailToolsDetail;
	return RenderPadWindow(
		"Markers##GW2InGameHelperMarkerEditor",
		G::ShowMarkerEditor,
		G::PadMarkerEditor,
		gPlaceOnceMarkers,
		gFocusMarkers,
		kEditW,
		kEditH,
		[]() {
			ImGui::TextColored(HelperTheme::Gold, "MARKERS");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Drop POIs at your feet. Open Trails beside this to follow the path while placing.");
			PadNav::PopWrap();
			if (ImGui::SmallButton("Open Trails###gw2igh_me_open_t"))
				TrailToolsPad::OpenTrails();
			ImGui::Separator();
			ImGui::BeginChild("###gw2igh_me_body", ImVec2(0.f, 0.f), true);
			DrawMarkersTab();
			ImGui::EndChild();
		});
}
