#include "PathingFeatures.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PathingPacks.h"
#include "Settings.h"
#include "PathingTrails.h"

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
		for (const std::string& p : PathingTrails::EnabledPaths())
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
			PathingTrails::EnableAllLadyCategories();
	}

	/* Prefer a narrow enable so Hero Point Train does not need the whole pack. */
	void EnsureLadyHpTrainCategories()
	{
		for (const std::string& p : PathingTrails::EnabledPaths())
		{
			const std::string l = LowerCopy(p);
			if (l == "legs" || l == "legs.hp" ||
				(l.size() > 8 && l.compare(0, 8, "legs.hp.") == 0))
				return;
		}
		std::vector<std::string> paths = PathingTrails::EnabledPaths();
		paths.push_back("legs.hp");
		PathingTrails::SetEnabledPaths(paths);
	}

	/* Map-completion editions stay mutually exclusive. */
	void SetLadyEdition(bool bare, bool mounts, bool wp)
	{
		G::LadyBarefoot = bare;
		G::LadyWithMounts = mounts;
		G::LadyWpOnly = wp;
		if (bare || mounts || wp || G::LadyHearts)
			EnsureLadyCategories();
		if (G::LadyHeroPointTrain)
			EnsureLadyHpTrainCategories();
		PathingTrails::NotifyVisibilityFilterChanged();
		Settings::SetDirty();
	}

	void SetLadyExtra(bool& flag, bool on)
	{
		flag = on;
		if (G::LadyBarefoot || G::LadyWithMounts || G::LadyWpOnly || G::LadyHearts)
			EnsureLadyCategories();
		if (G::LadyHeroPointTrain)
			EnsureLadyHpTrainCategories();
		PathingTrails::NotifyVisibilityFilterChanged();
		Settings::SetDirty();
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
	PathingTrails::Update(mapId ? mapId : 1u);

	ImGui::TextUnformatted("Map Completion (Tekkit)");
	ImGui::TextDisabled("One route edition at a time.");
	const auto activeMc = PathingTrails::ActiveMapCompletionRoutes();
	using Mc = PathingTrails::MapCompletionRoutes;
	bool bareOn = (activeMc == Mc::Barefoot);
	bool griffOn = (activeMc == Mc::Griffon);
	bool skyOn = (activeMc == Mc::Skyscale);
	if (ImGui::Checkbox("Foot###gw2igh_feat_mc_bare", &bareOn))
	{
		if (bareOn)
			PathingTrails::EnableMapCompletionPreset(Mc::Barefoot);
		else
			PathingTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Griffon###gw2igh_feat_mc_griff", &griffOn))
	{
		if (griffOn)
			PathingTrails::EnableMapCompletionPreset(Mc::Griffon);
		else
			PathingTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Skyscale###gw2igh_feat_mc_sky", &skyOn))
	{
		if (skyOn)
			PathingTrails::EnableMapCompletionPreset(Mc::Skyscale);
		else
			PathingTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	ImGui::TextDisabled("Skyscale routes: HoT + SotO only. Elsewhere use Foot/Griffon.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Lady Elyssa — map routes");
	ImGui::TextDisabled("Current map only — one edition at a time.");

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
			"Foot routes on this map plus Barefoot Shortcut (bfs) trails/markers.\n"
			"Heart trails use the Hearts toggle.");
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
		ImGui::SetTooltip("Waypoint trails on this map only — no markers or mount icons.");
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
			"Mount route on this map plus mount-guide markers (raptor/springer/…).\n"
			"Barefoot Shortcuts stay on Barefoot. Hearts use the Hearts toggle.");

	ImGui::Spacing();
	ImGui::TextUnformatted("Lady Elyssa — extras");
	ImGui::TextDisabled("Independent — current map only.");
	bool ladyHearts = G::LadyHearts;
	bool ladyHp = G::LadyHeroPointTrain;
	if (ImGui::Checkbox("Hearts###gw2igh_feat_lady_hearts", &ladyHearts))
	{
		SetLadyExtra(G::LadyHearts, ladyHearts);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Heart trails (and heart markers) on this map.");
	ImGui::SameLine();
	if (ImGui::Checkbox("Hero Point Train###gw2igh_feat_lady_hp", &ladyHp))
	{
		SetLadyExtra(G::LadyHeroPointTrain, ladyHp);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Only Hero Point train trails and number/start/WP icons on this map\n"
			"(legs.hp) — nothing else from the Lady pack.");

	if (PathingTrails::IsLoading() || PathingPacks::IsUpdating())
		ImGui::TextDisabled("Indexing packs…");

	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::Button("Reset marker states###gw2igh_path_feat_reset"))
		PathingTrails::ResetMarkerBehaviorStates();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Clear Blish/TacO activation data (weekly chests, auto-triggers).\n"
			"Same idea as deleting Blish timers.txt.");
	ImGui::TextDisabled("Lua script-* features still need Blish HUD Pathing.");

	return dirty;
}
