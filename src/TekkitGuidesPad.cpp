#include "TekkitGuidesPad.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PathingPacks.h"
#include "RoutingSuggest.h"
#include "Settings.h"
#include "TekkitTrails.h"
#include "WaypointsData.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
	constexpr float kPadW = 460.f;
	constexpr float kPadH = 680.f;

	bool gRequestDock = false;

	void SyncEnabledToSettings()
	{
		TekkitTrails::SerializeEnabledPaths(G::TekkitEnabled, sizeof(G::TekkitEnabled));
		Settings::SetDirty();
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
	ImGui::SetNextWindowSizeConstraints(ImVec2(380.f, 320.f), ImVec2(620.f, maxH));
	ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);

	/* Dock beside the helper on each Open() — same as Notes / TP.
	   Cond_Always so a stale imgui.ini left-edge pos cannot win. */
	if (gRequestDock)
	{
		ImGui::SetNextWindowPos(PadDock::BesideHelper(kPadW), ImGuiCond_Always);
		ImGui::SetNextWindowFocus();
		gRequestDock = false;
	}

	bool open = G::ShowTekkitGuides;
	HelperTheme::ScopedWindow theme(G::Opacity);
	/* NoNavInputs — gamepad/keyboard nav steals letters from the category filter. */
	if (!ImGui::Begin("Pathing##GW2InGameHelperPathing", &open,
		ImGuiWindowFlags_NoNavInputs))
	{
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
		ImGui::End();
		return false;
	}

	ImGui::TextColored(HelperTheme::Gold, "PATHING");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(HelperTheme::Muted,
		"Curated packs auto-update from the authors' releases. Drop any extra .taco "
		"into the pathing folder — your files are never deleted.");
	ImGui::Spacing();
	ImGui::TextColored(HelperTheme::Muted,
		"Tekkit's All-In-One © Tekkit's Workshop — used with permission.");
	ImGui::TextDisabled("https://www.tekkitsworkshop.net/");
	ImGui::TextColored(HelperTheme::Muted,
		"Lady Elyssa's Guides & Achievements © Lady Elyssa.");
	ImGui::TextDisabled("https://wiki.guildwars2.com/wiki/User:Lady_Elyssa");
	ImGui::PopTextWrapPos();
	ImGui::Separator();

	if (TekkitTrails::DrawSettings())
		SyncEnabledToSettings();

	ImGui::Separator();
	ImGui::TextUnformatted("Route to trail start");
	ImGui::TextDisabled(
		"Nearest public waypoints to the first loaded trail point on this map. "
		"Copy a chat code — no auto-teleport.");
	WaypointsData::Tick();
	/* Warm the public floor index while this pad is open so Find is instant. */
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

	if (sPendingRoute)
	{
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

	const RoutingSuggest::Result& route = RoutingSuggest::Last();
	if (!route.status.empty() && !sPendingRoute)
		ImGui::TextWrapped("%s", route.status.c_str());
	if (route.trailLabel[0])
		ImGui::TextDisabled("Trail: %s (%.0f, %.0f)", route.trailLabel, route.trailX, route.trailY);
	if (TekkitTrails::HasSearchGuide())
		ImGui::TextColored(HelperTheme::Ok, "Orange in-world guide active.");

	for (size_t i = 0; i < route.nearest.size(); ++i)
	{
		const RoutingSuggest::Candidate& c = route.nearest[i];
		ImGui::PushID(static_cast<int>(i));
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
		ImGui::TextColored(HelperTheme::Warn,
			"MumbleLink: waiting (needed for compass / GPS)");
	else
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		ImGui::TextColored(HelperTheme::Ok, "MumbleLink: OK");
		if (ctx && ctx->mapId)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("map %u · compass %ux%u",
				ctx->mapId, ctx->compassWidth, ctx->compassHeight);
		}
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	/* Keep routing keys here while the filter (or any text field) is active,
	   even if the cursor leaves the window slightly. */
	const bool typingHere = focused && ImGui::GetIO().WantTextInput;
	ImGui::End();
	return hovered || typingHere;
}
