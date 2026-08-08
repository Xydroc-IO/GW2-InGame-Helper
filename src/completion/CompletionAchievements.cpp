#include "CompletionShared.h"

#include "HelperTheme.h"
#include "PadNav.h"
#include "PathingTrails.h"

#include "imgui/imgui.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		std::string ToLowerCopy(std::string s)
		{
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		}

		bool TypePrefixMatch(const char* type, const char* prefix)
		{
			if (!type || !prefix || !prefix[0])
				return false;
			const std::string t = ToLowerCopy(type);
			const std::string p = ToLowerCopy(prefix);
			if (t == p)
				return true;
			if (t.size() > p.size() && t.compare(0, p.size(), p) == 0 && t[p.size()] == '.')
				return true;
			return false;
		}

		bool IsApBrowserCategory(const PathingTrails::Category& c)
		{
			const std::string low = ToLowerCopy(c.path);
			if (low.empty())
				return false;
			/* Lady achievement guides live under leag.* except map-completion / HP. */
			if (low == "legs" || (low.size() > 5 && low.compare(0, 5, "legs.") == 0))
				return false;
			if (low == "leag.map" || (low.size() > 9 && low.compare(0, 9, "leag.map.") == 0))
				return false;
			if (low == "leag.hp" || (low.size() > 8 && low.compare(0, 8, "leag.hp.") == 0))
				return false;
			if (low == "leag" || (low.size() > 5 && low.compare(0, 5, "leag.") == 0))
				return true;
			return false;
		}

		bool DrawApCategoryNode(const PathingTrails::Category& c, int depth)
		{
			if (c.hidden || c.separator)
				return false;
			if (!IsApBrowserCategory(c))
			{
				bool any = false;
				for (const PathingTrails::Category& ch : c.children)
					any = DrawApCategoryNode(ch, depth + 1) || any;
				return any;
			}

			const char* label = c.label.empty() ? c.path.c_str() : c.label.c_str();
			ImGui::PushID(c.path.c_str());
			const bool sel = gApCategoryPath[0] &&
				std::strcmp(gApCategoryPath, c.path.c_str()) == 0;
			if (c.children.empty())
			{
				if (ImGui::Selectable(label, sel))
					std::snprintf(gApCategoryPath, sizeof(gApCategoryPath), "%s", c.path.c_str());
			}
			else
			{
				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
					ImGuiTreeNodeFlags_SpanAvailWidth |
					(sel ? ImGuiTreeNodeFlags_Selected : 0);
				const bool open = ImGui::TreeNodeEx("##n", flags, "%s", label);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
					std::snprintf(gApCategoryPath, sizeof(gApCategoryPath), "%s", c.path.c_str());
				if (open)
				{
					for (const PathingTrails::Category& ch : c.children)
						DrawApCategoryNode(ch, depth + 1);
					ImGui::TreePop();
				}
			}
			ImGui::PopID();
			return true;
		}

		void DrawAchievementRows()
		{
			if (!gApCategoryPath[0])
			{
				ImGui::TextColored(HelperTheme::Muted,
					"Pick a category on the left.");
				return;
			}
			const size_t n = ObjectiveCount();
			int shown = 0;
			for (size_t i = 0; i < n; ++i)
			{
				Objective* o = ObjectiveAt(i);
				if (!o || o->kind != ObjKind::Achievement)
					continue;
				if (!TypePrefixMatch(o->packType, gApCategoryPath))
					continue;
				++shown;
				ImGui::PushID(static_cast<int>(o->id));
				bool done = o->done;
				if (ImGui::Checkbox("##d", &done))
					ToggleDone(i);
				ImGui::SameLine();
				const bool sel = gFocusObjective == static_cast<int>(i);
				const float gpsW = o->hasCoord
					? (ImGui::CalcTextSize("GPS").x + ImGui::GetStyle().FramePadding.x * 2.f +
						ImGui::GetStyle().ItemSpacing.x)
					: 0.f;
				char row[160];
				if (const MapInfo* m = FindMap(o->mapId))
					std::snprintf(row, sizeof(row), "%s  [%s]", o->name, m->name);
				else
					std::snprintf(row, sizeof(row), "%s", o->name);
				const float nameW = ImGui::GetContentRegionAvail().x - gpsW;
				if (ImGui::Selectable(row, sel,
						ImGuiSelectableFlags_AllowDoubleClick,
						ImVec2(nameW > 40.f ? nameW : 40.f, 0.f)))
				{
					gFocusObjective = static_cast<int>(i);
					if (o->mapId)
						SetFocusMap(o->mapId);
					if (ImGui::IsMouseDoubleClicked(0))
						GuideToObjective(i);
				}
				if (o->hasCoord)
				{
					ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x);
					if (ImGui::SmallButton("GPS"))
						GuideToObjective(i);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(
							"Orange GPS pathfind (pack trails + waypoints).\n"
							"Does not enable pathing pack categories.");
				}
				ImGui::PopID();
			}
			if (shown == 0)
				ImGui::TextColored(HelperTheme::Muted,
					"No markers under this category yet (wait for pack index).");
		}
	}

	void DrawAchievementsTab()
	{
		EnsureCatalog();
		ApplyApOverlayResult();

		ImGui::TextWrapped(
			"Lady AP pack markers (local ticks) + optional Explorer API overlay. "
			"Does not sync character map completion %%.");
		if (PadNav::RefreshButton("###gw2igh_ap_api"))
			BeginApOverlayRefresh();
		ImGui::SameLine();
		if (ApOverlayBusy())
			ImGui::TextColored(HelperTheme::Muted, "Loading API...");
		{
			char apiLine[160]{};
			if (FormatApOverlayLine(gFocusMapId, gApCategoryPath, apiLine, sizeof(apiLine)))
				ImGui::TextColored(HelperTheme::Ok, "%s", apiLine);
			else
				ImGui::TextColored(HelperTheme::Muted,
					"API: set key + refresh for Explorer / Been There overlay.");
		}

		if (PathingTrails::IndexedMarkerCount() == 0)
		{
			ImGui::TextWrapped(
				"No pack markers indexed yet. Open Pathing once so curated "
				"LadyElyssaAP.taco can download/index — no extra GPS packs needed.");
			return;
		}

		if (ImGui::Button("Open in Pathing###gw2igh_ap_open"))
			OpenLadyAchievementPathing(gApCategoryPath);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Enable the selected Lady AP category path and open Pathing.\n"
				"Per-row GPS never flips categories.");

		ImGui::Separator();

		const float treeW = ImGui::GetContentRegionAvail().x * 0.42f;
		ImGui::BeginChild("###gw2igh_ap_tree", ImVec2(treeW, 0.f), true);
		std::vector<PathingTrails::Category> tree = PathingTrails::CategoryTree();
		bool any = false;
		for (const PathingTrails::Category& root : tree)
			any = DrawApCategoryNode(root, 0) || any;
		if (!any)
			ImGui::TextColored(HelperTheme::Muted,
				"No Lady AP categories in the pack menu yet.");
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("###gw2igh_ap_list", ImVec2(0.f, 0.f), true);
		if (gApCategoryPath[0])
			ImGui::TextColored(HelperTheme::Muted, "%s", gApCategoryPath);
		DrawAchievementRows();
		ImGui::EndChild();
	}
}
