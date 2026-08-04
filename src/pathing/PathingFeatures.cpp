#include "PathingFeatures.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
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

	/* Map-completion editions are independent — each only gates its own trails. */
	void SetLadyRouteFlag(bool& flag, bool on)
	{
		flag = on;
		if (G::LadyBarefoot || G::LadyWithMounts || G::LadyWpOnly || G::LadyHearts)
			EnsureLadyCategories();
		if (G::LadyHeroPointTrain)
			EnsureLadyHpTrainCategories();
		PathingTrails::NotifyVisibilityFilterChanged();
		Settings::SetDirty();
	}

	void SetLadyExtra(bool& flag, bool on)
	{
		const bool isHpTrain = (&flag == &G::LadyHeroPointTrain);
		flag = on;
		if (isHpTrain)
		{
			if (on)
				EnsureLadyHpTrainCategories();
			else
				PathingTrails::SetCategoryEnabled("legs.hp", false);
		}
		else
		{
			if (G::LadyBarefoot || G::LadyWithMounts || G::LadyWpOnly || G::LadyHearts)
				EnsureLadyCategories();
			if (G::LadyHeroPointTrain)
				EnsureLadyHpTrainCategories();
		}
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
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted, "One route edition at a time.");
	PadNav::PopWrap();
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
	PadNav::WrapSameLine(PadNav::CheckboxWidth("Griffon"));
	if (ImGui::Checkbox("Griffon###gw2igh_feat_mc_griff", &griffOn))
	{
		if (griffOn)
			PathingTrails::EnableMapCompletionPreset(Mc::Griffon);
		else
			PathingTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	PadNav::WrapSameLine(PadNav::CheckboxWidth("Skyscale"));
	if (ImGui::Checkbox("Skyscale###gw2igh_feat_mc_sky", &skyOn))
	{
		if (skyOn)
			PathingTrails::EnableMapCompletionPreset(Mc::Skyscale);
		else
			PathingTrails::ClearMapCompletionCategories();
		dirty = true;
	}
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Skyscale routes: HoT + SotO only. Elsewhere use Foot/Griffon.");
	PadNav::PopWrap();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Lady Elyssa — map routes");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Current map — each toggle only its own trails (can combine).");
	PadNav::PopWrap();

	bool ladyBare = G::LadyBarefoot;
	bool ladyMounts = G::LadyWithMounts;
	bool ladyWp = G::LadyWpOnly;
	if (ImGui::Checkbox("Barefoot###gw2igh_feat_lady_bare", &ladyBare))
	{
		SetLadyRouteFlag(G::LadyBarefoot, ladyBare);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Barefoot foot routes on this map, plus Barefoot Shortcut (bfs)\n"
			"trails and shortcut markers. Hearts use the Hearts toggle.");
	PadNav::WrapSameLine(PadNav::CheckboxWidth("WP Only"));
	if (ImGui::Checkbox("WP Only###gw2igh_feat_lady_wp", &ladyWp))
	{
		SetLadyRouteFlag(G::LadyWpOnly, ladyWp);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Waypoint-only trails on this map (…map.<zone>.wp) — no markers.");
	PadNav::WrapSameLine(PadNav::CheckboxWidth("With Mounts"));
	if (ImGui::Checkbox("With Mounts###gw2igh_feat_lady_mounts", &ladyMounts))
	{
		SetLadyRouteFlag(G::LadyWithMounts, ladyMounts);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Mount route on this map plus mount-guide markers (raptor/springer/…).\n"
			"Barefoot Shortcuts stay on Barefoot. Hearts use the Hearts toggle.");

	ImGui::Spacing();
	ImGui::TextUnformatted("Lady Elyssa — extras");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Independent. Categories → Hero Points is the same tree as Hero Point Train.");
	PadNav::PopWrap();
	bool ladyHearts = G::LadyHearts;
	bool ladyHp = G::LadyHeroPointTrain;
	if (ImGui::Checkbox("Hearts###gw2igh_feat_lady_hearts", &ladyHearts))
	{
		SetLadyExtra(G::LadyHearts, ladyHearts);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Heart trails (and heart markers) on this map.");
	PadNav::WrapSameLine(PadNav::CheckboxWidth("Hero Point Train"));
	if (ImGui::Checkbox("Hero Point Train###gw2igh_feat_lady_hp", &ladyHp))
	{
		SetLadyExtra(G::LadyHeroPointTrain, ladyHp);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Hero Point train (legs.hp) — same as Categories → Hero Points.\n"
			"Number/start/WP icons along the HP train on this map.");

	if (PathingTrails::IsLoading() || PathingPacks::IsUpdating())
	{
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted, "Indexing packs…");
		PadNav::PopWrap();
	}

	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::Button("Reset marker states###gw2igh_path_feat_reset"))
		PathingTrails::ResetMarkerBehaviorStates();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Clear Blish/TacO activation data (weekly chests, auto-triggers).\n"
			"Same idea as deleting Blish timers.txt.");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted, "Lua script-* features still need Blish HUD Pathing.");
	PadNav::PopWrap();

	return dirty;
}
