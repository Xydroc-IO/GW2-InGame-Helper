#include "TekkitGuidesPad.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "PathingFeatures.h"
#include "PathingPacks.h"
#include "ConfirmedWaypoints.h"
#include "RoutingSuggest.h"
#include "Settings.h"
#include "TekkitTrails.h"
#include "WaypointsData.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
	constexpr float kPadW = 480.f;
	constexpr float kPadH = 700.f;

	bool gRequestDock = false; /* placeOnce — restore saved or dock beside helper */
	int gPathTab = 0; /* 0 Overview · 1 Features · 2 Categories · 3 Route */

	void SyncEnabledToSettings()
	{
		TekkitTrails::SerializeEnabledPaths(G::TekkitEnabled, sizeof(G::TekkitEnabled));
		Settings::SetDirty();
	}

	void DrawCredits()
	{
		ImGui::TextColored(HelperTheme::Gold, "PATHING");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(HelperTheme::Muted,
			"Curated packs auto-update. Drop extra .taco into pathing/ — yours are never deleted.");
		ImGui::TextDisabled("Tekkit · Lady Elyssa · QuitarHero (hover)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted("Tekkit's All-In-One © Tekkit's Workshop — used with permission");
			ImGui::TextUnformatted("https://www.tekkitsworkshop.net/");
			ImGui::Spacing();
			ImGui::TextUnformatted("Guides & Achievements © Lady Elyssa");
			ImGui::TextUnformatted("https://wiki.guildwars2.com/wiki/User:Lady_Elyssa");
			ImGui::Spacing();
			ImGui::TextUnformatted("Hero's Marker Pack © QuitarHero");
			ImGui::TextUnformatted("https://github.com/QuitarHero/Heros-Marker-Pack");
			ImGui::EndTooltip();
		}
		ImGui::PopTextWrapPos();
	}

	void DrawRouteTab()
	{
		ImGui::TextUnformatted("Route to nearest waypoint");
		ImGui::TextDisabled(
			"Uses public API waypoints — works with no pathing categories enabled. "
			"With packs on, prefers trail start; otherwise your position. "
			"Copy a chat code — no auto-teleport.");

		/* Keep map trails warm when categories are on — Find itself does not wait on packs. */
		uint32_t mapId = 0;
		if (G::Mumble && G::Mumble->uiTick != 0)
		{
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			if (ctx)
				mapId = ctx->mapId;
		}
		if (mapId)
			TekkitTrails::Update(mapId);

		WaypointsData::Tick();
		if (!WaypointsData::Ready() && !WaypointsData::Busy())
			WaypointsData::EnsureLoaded(false);

		static bool sPendingRoute = false;
		if (ImGui::Button("Find nearest waypoints###gw2igh_route_wp"))
			sPendingRoute = true;
		ImGui::SameLine();
		if (ImGui::Button("Clear orange guide###gw2igh_route_clear"))
		{
			RoutingSuggest::ClearGuide();
			sPendingRoute = false;
		}

		const bool trailsBusy = TekkitTrails::IsLoading() || PathingPacks::IsUpdating();
		if (sPendingRoute)
		{
			/* Waypoint index only — do not block on pathing pack load/toggles. */
			if (!WaypointsData::Ready())
			{
				WaypointsData::EnsureLoaded(false);
				ImGui::TextColored(HelperTheme::Warn, "%s",
					WaypointsData::Busy() ? WaypointsData::Status() : "Starting waypoint index…");
			}
			else
			{
				RoutingSuggest::SuggestNearTrailStart(3);
				sPendingRoute = false;
			}
		}
		else if (WaypointsData::Busy())
		{
			ImGui::TextDisabled("%s", WaypointsData::Status());
		}
		else if (trailsBusy)
		{
			ImGui::TextDisabled("Indexing trail packs… (Find still works without categories)");
		}

		if (ImGui::Button("Route from clipboard###gw2igh_route_clip"))
		{
			WaypointsData::EnsureLoaded(false);
			RoutingSuggest::SuggestFromClipboard();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Shift+click a waypoint in GW2 (copies [&…]), then click here.\n"
				"Sets the orange guide to that POI — no auto-teleport.");

		{
			bool prefer = ConfirmedWaypoints::PreferConfirmed();
			if (ImGui::Checkbox("Prefer walk-confirmed waypoints###gw2igh_wp_confirmed", &prefer))
				ConfirmedWaypoints::SetPreferConfirmed(prefer);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(
					"When on, Find uses waypoints this character has walked near.\n"
					"Confirmed locally from MumbleLink position + API coords.");
			const int mapIdWp = WaypointsData::CurrentMapId();
			ImGui::TextDisabled("Confirmed: %zu total · %zu on this map",
				ConfirmedWaypoints::CountForActive(),
				ConfirmedWaypoints::CountOnMap(mapIdWp));
		}

		const RoutingSuggest::Result& route = RoutingSuggest::Last();
		if (!route.status.empty() && !sPendingRoute)
			ImGui::TextWrapped("%s", route.status.c_str());
		if (route.trailLabel[0])
			ImGui::TextDisabled("Anchor: %s (%.0f, %.0f)", route.trailLabel, route.trailX, route.trailY);
		if (TekkitTrails::HasSearchGuide())
			ImGui::TextColored(HelperTheme::Ok, "Orange in-world guide active.");
		else if (TekkitTrails::HasSearchGuideActive())
			ImGui::TextDisabled("Orange guide loading trail geometry…");

		for (size_t i = 0; i < route.nearest.size(); ++i)
		{
			const RoutingSuggest::Candidate& c = route.nearest[i];
			ImGui::PushID(static_cast<int>(i));
			if (c.confirmed)
				ImGui::BulletText("%s  (%.0f)  [confirmed]", c.name.c_str(), c.dist);
			else
				ImGui::BulletText("%s  (%.0f)", c.name.c_str(), c.dist);
			ImGui::SameLine();
			ImGui::TextDisabled("%s", c.chatLink.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy"))
				RoutingSuggest::CopyChatLink(c.chatLink.c_str());
			ImGui::PopID();
		}

		ImGui::Separator();
		if (!G::Mumble || G::Mumble->uiTick == 0)
			ImGui::TextColored(HelperTheme::Warn, "MumbleLink: waiting");
		else
		{
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			ImGui::TextColored(HelperTheme::Ok, "MumbleLink OK");
			if (ctx && ctx->mapId)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("map %u · compass %ux%u",
					ctx->mapId, ctx->compassWidth, ctx->compassHeight);
			}
		}
	}
}
void TekkitGuidesPad::Open()
{
	G::ShowTekkitGuides = true;
	gRequestDock = true;
	Settings::SetDirty();
}

bool TekkitGuidesPad::Render()
{
	if (!G::ShowTekkitGuides)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = std::max(360.f, io.DisplaySize.y - 40.f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.f, 320.f), ImVec2(640.f, maxH));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	PadDock::Place(G::PadPathing, gRequestDock, kPadW, kPadH, PadDock::BesideHelper(kPadW));
	if (!gRequestDock && G::PadPathing.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);

	bool open = G::ShowTekkitGuides;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Pathing##GW2InGameHelperPathing", &open,
		ImGuiWindowFlags_NoNavInputs))
	{
		if (PadDock::Capture(G::PadPathing))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		ImGui::End();
		if (!open)
		{
			G::ShowTekkitGuides = false;
			Settings::SetDirty();
		}
		return hovered || (focused && ImGui::GetIO().WantTextInput);
	}
	if (!open)
	{
		G::ShowTekkitGuides = false;
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadPathing))
		Settings::SetDirty();

	static const char* kTabs[] = { "Overview", "Features", "Categories", "Route" };
	gPathTab = PadNav::DrawSideRail("###gw2igh_path_nav", kTabs, 4, gPathTab, 112.f);

	ImGui::BeginChild("###gw2igh_path_body", ImVec2(0.f, 0.f), gPathTab != 0);
	switch (gPathTab)
	{
	case 0:
		DrawCredits();
		ImGui::Separator();
		if (TekkitTrails::DrawOverlaySettings())
			SyncEnabledToSettings();
		ImGui::Separator();
		TekkitTrails::DrawPackTools();
		break;
	case 1:
		if (PathingFeatures::RenderContents())
			SyncEnabledToSettings();
		break;
	case 2:
		if (TekkitTrails::DrawCategoryBrowser())
			SyncEnabledToSettings();
		break;
	case 3:
		DrawRouteTab();
		break;
	default:
		break;
	}
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	const bool typingHere = focused && ImGui::GetIO().WantTextInput;
	ImGui::End();
	return hovered || typingHere;
}
