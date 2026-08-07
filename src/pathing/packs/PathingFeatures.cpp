#include "PathingFeatures.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PathingLua.h"
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

	/* Map Completion Features - only legs.map / leag.map (not bounty/fishing/...). */
	void EnsureLadyMapCompletionCategories()
	{
		PathingTrails::EnableLadyMapCompletionCategories();
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

	/* Each Map Completion toggle only gates its own trails/markers/shortcuts. */
	void SetLadyMcFlag(bool& flag, bool on)
	{
		flag = on;
		if (G::LadyBarefoot || G::LadyWithMounts || G::LadyWpOnly || G::LadyHearts)
			EnsureLadyMapCompletionCategories();
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
				EnsureLadyMapCompletionCategories();
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

	ImGui::Separator();
	ImGui::TextUnformatted("Lady Elyssa - extras");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Independent toggles. Map Completion is Lady-only (not Tekkit).");
	PadNav::PopWrap();

	ImGui::TextUnformatted("Map Completion");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Current map only - each toggle its own trails, markers, shortcuts.");
	PadNav::PopWrap();

	bool ladyBare = G::LadyBarefoot;
	bool ladyMounts = G::LadyWithMounts;
	bool ladyWp = G::LadyWpOnly;
	if (ImGui::Checkbox("Barefoot###gw2igh_feat_lady_bare", &ladyBare))
	{
		SetLadyMcFlag(G::LadyBarefoot, ladyBare);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Barefoot map-completion trails and markers on this map,\n"
			"plus Barefoot Shortcut (bfs) trails and shortcut markers.");
	PadNav::WrapSameLine(PadNav::CheckboxWidth("WP Only"));
	if (ImGui::Checkbox("WP Only###gw2igh_feat_lady_wp", &ladyWp))
	{
		SetLadyMcFlag(G::LadyWpOnly, ladyWp);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Waypoint-only edition on this map (...map.<zone>.wp):\n"
			"trails, markers, and shortcuts under that edition.");
	PadNav::WrapSameLine(PadNav::CheckboxWidth("With Mounts"));
	if (ImGui::Checkbox("With Mounts###gw2igh_feat_lady_mounts", &ladyMounts))
	{
		SetLadyMcFlag(G::LadyWithMounts, ladyMounts);
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Mount map-completion trails on this map, plus mount-guide\n"
			"markers and shortcuts (raptor/springer/...). Not barefoot/bfs.");

	ImGui::TextUnformatted("Other");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Categories -> Hero Points is the same tree as Hero Point Train.");
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
			"Hero Point train (legs.hp) - same as Categories -> Hero Points.\n"
			"Number/start/WP icons along the HP train on this map.");

	if (PathingTrails::IsLoading() || PathingPacks::IsUpdating())
	{
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted, "Indexing packs...");
		PadNav::PopWrap();
	}

	ImGui::Separator();
	if (ImGui::Button("Reset marker states###gw2igh_path_feat_reset"))
		PathingTrails::ResetMarkerBehaviorStates();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Clear Blish/TacO activation data (weekly chests, auto-triggers).\n"
			"Same idea as deleting Blish timers.txt.");
	PadNav::PushWrap();
	ImGui::Checkbox("Enable Lua scripts (Blish-shaped subset)###gw2igh_path_lua",
		&G::EnablePathingLua);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Opt-in. Packs may execute .lua + script-* attrs.\n"
			"API: Marker/World/Pack/Mumble/Event/Vector3 (libdef-shaped).\n"
			"Leave OFF unless you trust the pack.");
	ImGui::TextColored(HelperTheme::Muted,
		"Supported: Marker/Trail mutators, Menu.Add, CDN SetTexture(id), "
		"GetBehavior, World:TrailByGuid/GetClosestTrail(s), Pack:CreateMarker, "
		"Mumble/Event/User. Opt-in only.");
	PathingLua::DrawScriptMenus();
	PadNav::PopWrap();

	return dirty;
}
