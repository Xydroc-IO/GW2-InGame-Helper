#include "TekkitGuidesPad.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "Settings.h"
#include "TekkitTrails.h"

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
	if (!ImGui::Begin("Tekkit's Guides##GW2InGameHelperTekkit", &open,
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

	ImGui::TextColored(HelperTheme::Gold, "TEKKIT'S GUIDES");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(HelperTheme::Muted,
		"Guides & trails © Tekkit's Workshop (All-In-One pack) — used with permission. "
		"This panel only toggles display.");
	ImGui::TextDisabled("https://www.tekkitsworkshop.net/");
	ImGui::PopTextWrapPos();
	ImGui::Separator();

	if (TekkitTrails::DrawSettings())
		SyncEnabledToSettings();

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
