#include "PathingFeatures.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PathingPacks.h"
#include "TekkitTrails.h"

#include "imgui/imgui.h"

#include <string>
#include <vector>

namespace
{
	bool PathEnabled(const std::vector<std::string>& enabled, const char* path)
	{
		if (!path || !path[0] || enabled.empty())
			return false;
		std::string want = path;
		for (char& c : want)
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
		for (const std::string& p : enabled)
		{
			std::string el = p;
			for (char& c : el)
				if (c >= 'A' && c <= 'Z')
					c = static_cast<char>(c - 'A' + 'a');
			if (el == want)
				return true;
			if (el.size() > want.size() && el.compare(0, want.size(), want) == 0 &&
				el[want.size()] == '.')
				return true;
			if (want.size() > el.size() && want.compare(0, el.size(), el) == 0 &&
				want[el.size()] == '.')
				return true;
		}
		return false;
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

	ImGui::TextColored(HelperTheme::Gold, "QUICK ENABLE");
	ImGui::TextDisabled("Check a pack to turn its whole tree on. Drill down in Categories.");

	const std::vector<std::string> enabled = TekkitTrails::EnabledPaths();
	bool tekkitOn = PathEnabled(enabled, "tw_guides");
	bool ladyOn = PathEnabled(enabled, "legs") || PathEnabled(enabled, "leag");
	bool heroOn = PathEnabled(enabled, "HMP") || PathEnabled(enabled, "hmpSim");

	/* Additive toggles — checking one pack does not clear the others. */
	if (ImGui::Checkbox("Tekkit's All-In-One###gw2igh_feat_tekkit", &tekkitOn))
	{
		TekkitTrails::SetCategoryEnabled("tw_guides", tekkitOn);
		dirty = true;
	}
	if (ImGui::Checkbox("Lady Elyssa (Guides + Achievements)###gw2igh_feat_lady", &ladyOn))
	{
		TekkitTrails::SetCategoryEnabled("legs", ladyOn);
		TekkitTrails::SetCategoryEnabled("leag", ladyOn);
		dirty = true;
	}
	if (ImGui::Checkbox("Hero's Marker Pack###gw2igh_feat_hero", &heroOn))
	{
		TekkitTrails::SetCategoryEnabled("HMP", heroOn);
		TekkitTrails::SetCategoryEnabled("hmpSim", heroOn);
		dirty = true;
	}
	if (ImGui::Button("All packs off###gw2igh_feat_alloff"))
	{
		TekkitTrails::DisableAllCategories();
		dirty = true;
	}

	ImGui::Spacing();
	ImGui::Separator();
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
