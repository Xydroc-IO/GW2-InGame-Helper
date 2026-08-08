#include "CompletionShared.h"
#include "CompletionInternal.h"

#include "HelperTheme.h"
#include "PadNav.h"
#include "WaypointsData.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		void DrawKindChips()
		{
			if (ImGui::SmallButton("All###gw2igh_ck_all"))
				SetAllKindsVisible(true);
			ImGui::SameLine();
			for (int i = 0; i < static_cast<int>(ObjKind::Count); ++i)
			{
				const ObjKind k = static_cast<ObjKind>(i);
				if (!IsMapCompletionRouteKind(k))
					continue;
				const bool on = KindVisible(k);
				if (i) ImGui::SameLine();
				char lab[32];
				std::snprintf(lab, sizeof(lab), "%s###ck%d", ObjKindChip(k), i);
				if (on)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.42f, 0.12f, 1.f));
				if (ImGui::SmallButton(lab))
					SetKindVisible(k, !on);
				if (on)
					ImGui::PopStyleColor();
			}
		}

		void DrawObjectiveRows(uint32_t mapId, ObjKind kind)
		{
			std::vector<size_t> idxs;
			ObjectivesForMap(mapId, idxs);
			for (size_t idx : idxs)
			{
				Objective* o = ObjectiveAt(idx);
				if (!o || o->kind != kind) continue;
				if (!KindVisible(o->kind)) continue;
				ImGui::PushID(static_cast<int>(o->id));
				bool done = o->done;
				if (ImGui::Checkbox("##d", &done))
					ToggleDone(idx);
				ImGui::SameLine();
				const bool sel = gFocusObjective == static_cast<int>(idx);
				const float gpsW = o->hasCoord
					? (ImGui::CalcTextSize("GPS").x + ImGui::GetStyle().FramePadding.x * 2.f +
						ImGui::GetStyle().ItemSpacing.x)
					: 0.f;
				const float nameW = ImGui::GetContentRegionAvail().x - gpsW;
				if (ImGui::Selectable(o->name, sel,
						ImGuiSelectableFlags_AllowDoubleClick,
						ImVec2(nameW > 40.f ? nameW : 40.f, 0.f)))
				{
					gFocusObjective = static_cast<int>(idx);
					if (ImGui::IsMouseDoubleClicked(0))
						GuideToObjective(idx);
				}
				if (o->hasCoord)
				{
					ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x);
					if (ImGui::SmallButton("GPS"))
						GuideToObjective(idx);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(
							"Orange GPS pathfind to this point (pack trails + waypoints).\n"
							"Does not enable pathing pack categories.");
				}
				ImGui::PopID();
			}
		}

		struct ZoneAgg
		{
			uint32_t id = 0;
			const MapInfo* map = nullptr;
			int done = 0;
			int total = 0;
		};

		void BuildHierarchy(
			std::map<std::string, std::map<std::string, std::vector<ZoneAgg>>>& tree)
		{
			tree.clear();
			const size_t n = MapCount();
			for (size_t i = 0; i < n; ++i)
			{
				const MapInfo* m = MapAt(i);
				if (!m || !m->name[0]) continue;
				const int total = CountTotal(m->id);
				if (total <= 0 && m->id != gFocusMapId)
				{
					/* Keep curated Strikes / Festival shells visible (Atlas scopes). */
					const bool strike = std::strncmp(m->release, "Strikes", 7) == 0;
					const bool fest = std::strncmp(m->release, "Festival", 8) == 0;
					if (!strike && !fest)
						continue;
				}
				ZoneAgg z;
				z.id = m->id;
				z.map = m;
				z.done = CountDone(m->id);
				z.total = total;
				const char* rel = m->release[0] ? m->release : DefaultRelease();
				const char* reg = m->region[0] ? m->region : DefaultRegion();
				tree[rel][reg].push_back(z);
			}
		}
	}

	void DrawChecklistTab()
	{
		LoadFavorites();
		if (ImGui::Button("Use current map###gw2igh_cmp_cur"))
		{
			const int cur = WaypointsData::CurrentMapId();
			if (cur > 0) SetFocusMap(static_cast<uint32_t>(cur));
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear zone ticks###gw2igh_cmp_clr"))
			ClearDoneForMap(gFocusMapId);
		ImGui::TextColored(HelperTheme::Muted, "Focus zone: %d / %d",
			CountDone(gFocusMapId), CountTotal(gFocusMapId));
		{
			char apiLine[160]{};
			if (FormatApOverlayLine(gFocusMapId, nullptr, apiLine, sizeof(apiLine)))
			{
				ImGui::TextColored(HelperTheme::GoldMuted, "%s", apiLine);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(
						"Account Explorer achievement (official API).\n"
						"Local checklist ticks are separate — not map %% sync.");
			}
		}
		DrawKindChips();

		std::map<std::string, std::map<std::string, std::vector<ZoneAgg>>> tree;
		BuildHierarchy(tree);

		ImGui::BeginChild("##cmp_hier", ImVec2(0.f, 0.f), true);
		if (tree.empty())
		{
			ImGui::TextColored(HelperTheme::Muted,
				"No objectives yet — waiting for live waypoint index, or pick a zone in Atlas.");
			ImGui::EndChild();
			return;
		}

		for (auto& relPair : tree)
		{
			int relDone = 0, relTotal = 0;
			for (auto& regPair : relPair.second)
				for (const ZoneAgg& z : regPair.second)
				{ relDone += z.done; relTotal += z.total; }
			char relLab[160];
			std::snprintf(relLab, sizeof(relLab), "%s  (%d/%d)###rel_%s",
				relPair.first.c_str(), relDone, relTotal, relPair.first.c_str());
			if (!ImGui::TreeNodeEx(relLab, ImGuiTreeNodeFlags_DefaultOpen))
				continue;

			for (auto& regPair : relPair.second)
			{
				int regDone = 0, regTotal = 0;
				for (const ZoneAgg& z : regPair.second)
				{ regDone += z.done; regTotal += z.total; }
				char regLab[160];
				std::snprintf(regLab, sizeof(regLab), "%s  (%d/%d)###reg_%s",
					regPair.first.c_str(), regDone, regTotal, regPair.first.c_str());
				if (!ImGui::TreeNodeEx(regLab, ImGuiTreeNodeFlags_DefaultOpen))
					continue;

				for (const ZoneAgg& z : regPair.second)
				{
					char zoneLab[160];
					std::snprintf(zoneLab, sizeof(zoneLab), "%s  (%d/%d)###z%u",
						z.map->name, z.done, z.total, z.id);
					const ImGuiTreeNodeFlags zflags =
						(z.id == gFocusMapId ? ImGuiTreeNodeFlags_Selected : 0) |
						ImGuiTreeNodeFlags_OpenOnArrow |
						ImGuiTreeNodeFlags_OpenOnDoubleClick;
					const bool zOpen = ImGui::TreeNodeEx(zoneLab, zflags);
					if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
						SetFocusMap(z.id);
					if (ImGui::BeginPopupContextItem("##zctx"))
					{
						if (ImGui::MenuItem(IsFavorite(z.id) ? "Unfavorite" : "Favorite"))
							ToggleFavorite(z.id);
						if (ImGui::MenuItem("Clear ticks"))
							ClearDoneForMap(z.id);
						ImGui::EndPopup();
					}
					if (!zOpen)
						continue;

					for (int ki = 0; ki < static_cast<int>(ObjKind::Count); ++ki)
					{
						const ObjKind k = static_cast<ObjKind>(ki);
						if (!IsMapCompletionRouteKind(k) || !KindVisible(k)) continue;
						const int kt = CountTotalKind(z.id, k);
						if (kt <= 0) continue;
						const int kd = CountDoneKind(z.id, k);
						char typeLab[96];
						std::snprintf(typeLab, sizeof(typeLab), "%s  (%d/%d)###t%d_%u",
							ObjKindName(k), kd, kt, ki, z.id);
						if (ImGui::TreeNodeEx(typeLab, ImGuiTreeNodeFlags_DefaultOpen))
						{
							DrawObjectiveRows(z.id, k);
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}
		ImGui::EndChild();
	}
}
