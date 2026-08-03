#include "PathingFeatures.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PathingPacks.h"
#include "Settings.h"
#include "TekkitTrails.h"

#include "imgui/imgui.h"

#include <cctype>
#include <string>

namespace
{
	std::string LowerCopy(std::string s)
	{
		for (char& ch : s)
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		return s;
	}

	bool LadyPackEnabled()
	{
		for (const std::string& p : TekkitTrails::EnabledPaths())
		{
			const std::string l = LowerCopy(p);
			if (l == "legs" || l == "leag" ||
				(l.size() > 5 && (l.compare(0, 5, "legs.") == 0 || l.compare(0, 5, "leag.") == 0)))
				return true;
		}
		return false;
	}

	void EnsureLadyCategories()
	{
		if (!LadyPackEnabled())
			TekkitTrails::EnableAllLadyCategories();
	}

	/* One map-completion edition at a time — stacking drew WP on top of mounts. */
	void SetLadyEdition(bool bare, bool mounts, bool wp)
	{
		G::LadyBarefoot = bare;
		G::LadyWithMounts = mounts;
		G::LadyWpOnly = wp;
		if (bare || mounts || wp)
			EnsureLadyCategories();
		TekkitTrails::NotifyVisibilityFilterChanged();
	}
}

bool PathingFeatures::RenderContents()
{
	bool dirty = false;

	uint32_t mapId = 0;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
			mapId = ctx->mapId;
	}
	TekkitTrails::Update(mapId ? mapId : 1u);

	ImGui::TextUnformatted("Map Completion (Tekkit)");
	ImGui::TextDisabled("One route edition at a time.");
	const auto activeMc = TekkitTrails::ActiveMapCompletionRoutes();
	using Mc = TekkitTrails::MapCompletionRoutes;
	bool bareOn = (activeMc == Mc::Barefoot);
	bool griffOn = (activeMc == Mc::Griffon);
	bool skyOn = (activeMc == Mc::Skyscale);
	if (ImGui::Checkbox("Foot###gw2igh_feat_mc_bare", &bareOn))
	{
		if (bareOn)
			TekkitTrails::EnableMapCompletionPreset(Mc::Barefoot);
		else
			TekkitTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Griffon###gw2igh_feat_mc_griff", &griffOn))
	{
		if (griffOn)
			TekkitTrails::EnableMapCompletionPreset(Mc::Griffon);
		else
			TekkitTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Skyscale###gw2igh_feat_mc_sky", &skyOn))
	{
		if (skyOn)
			TekkitTrails::EnableMapCompletionPreset(Mc::Skyscale);
		else
			TekkitTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	ImGui::TextDisabled("Skyscale routes: HoT + SotO only. Elsewhere use Foot/Griffon.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Lady Elyssa");
	ImGui::TextDisabled("One map-completion route edition at a time.");
	if (ImGui::Button("Enable Lady packs###gw2igh_feat_lady_on"))
	{
		TekkitTrails::EnableAllLadyCategories();
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Turns on Lady Elyssa Guides + Achievements under Categories.\n"
			"Needed before Barefoot / WP Only / With Mounts show anything.");

	bool ladyBare = G::LadyBarefoot;
	bool ladyMounts = G::LadyWithMounts;
	bool ladyWp = G::LadyWpOnly;
	if (ImGui::Checkbox("Barefoot###gw2igh_feat_lady_bare", &ladyBare))
	{
		if (ladyBare)
			SetLadyEdition(true, false, false);
		else
			SetLadyEdition(false, false, false);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Foot / no-mount map routes (footprint trails + number markers).\n"
			"Barefoot Shortcuts (bfs mount icons) show here.\n"
			"Hides With Mounts and WP Only.");
	ImGui::SameLine();
	if (ImGui::Checkbox("WP Only###gw2igh_feat_lady_wp", &ladyWp))
	{
		if (ladyWp)
			SetLadyEdition(false, false, true);
		else
			SetLadyEdition(false, false, false);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Waypoint Only map routes only.\n"
			"Does not stack on Barefoot / With Mounts trails.");
	ImGui::SameLine();
	if (ImGui::Checkbox("With Mounts###gw2igh_feat_lady_mounts", &ladyMounts))
	{
		if (ladyMounts)
			SetLadyEdition(false, true, false);
		else
			SetLadyEdition(false, false, false);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Mount-optimized routes (all / main / withmounts).\n"
			"Shows mount shortcut icons (beetle, springer, …).\n"
			"Hides Barefoot and WP Only.");
	ImGui::TextDisabled("Barefoot = foot trails. With Mounts = mount trails + icons.");
	ImGui::TextDisabled("WP Only = waypoint trails alone. Categories must include Lady.");

	if (TekkitTrails::IsLoading() || PathingPacks::IsUpdating())
		ImGui::TextDisabled("Indexing packs…");

	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::Button("Reset marker states###gw2igh_path_feat_reset"))
		TekkitTrails::ResetMarkerBehaviorStates();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Clear Blish/TacO activation data (weekly chests, auto-triggers).\n"
			"Same idea as deleting Blish timers.txt.");
	ImGui::TextDisabled("Lua script-* features still need Blish HUD Pathing.");

	return dirty;
}
