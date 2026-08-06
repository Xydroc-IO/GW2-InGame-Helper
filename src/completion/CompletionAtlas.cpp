#include "CompletionShared.h"
#include "CompletionInternal.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		bool MapMatchesFilter(const MapInfo& m)
		{
			if (gAtlasFavOnly && !IsFavorite(m.id))
				return false;
			if (!gAtlasFilter[0])
				return true;
			std::string hay = std::string(m.name) + " " + m.region + " " + m.release;
			std::string needle = gAtlasFilter;
			for (char& c : hay) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
			for (char& c : needle) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
			return hay.find(needle) != std::string::npos;
		}

		void DrawKindChips()
		{
			if (ImGui::SmallButton("All###gw2igh_ak_all"))
				SetAllKindsVisible(true);
			ImGui::SameLine();
			for (int i = 0; i < static_cast<int>(ObjKind::Count); ++i)
			{
				const ObjKind k = static_cast<ObjKind>(i);
				const bool on = KindVisible(k);
				if (i) ImGui::SameLine();
				char lab[32];
				std::snprintf(lab, sizeof(lab), "%s###ak%d", ObjKindChip(k), i);
				if (on)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.42f, 0.12f, 1.f));
				if (ImGui::SmallButton(lab))
					SetKindVisible(k, !on);
				if (on)
					ImGui::PopStyleColor();
			}
		}

		struct ZoneAgg
		{
			uint32_t id = 0;
			const MapInfo* map = nullptr;
			int done = 0;
			int total = 0;
		};

		void DrawZoneRow(const ZoneAgg& z)
		{
			const bool fav = IsFavorite(z.id);
			const bool sel = z.id == gFocusMapId;
			ImGui::PushID(static_cast<int>(z.id));

			if (ImGui::SmallButton(fav ? "Unfav###f" : "Fav###f"))
				ToggleFavorite(z.id);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(fav ? "Remove favorite" : "Add favorite");
			ImGui::SameLine();

			char row[192];
			if (z.total > 0)
				std::snprintf(row, sizeof(row), "%s  (%d/%d)###z", z.map->name, z.done, z.total);
			else
				std::snprintf(row, sizeof(row), "%s  (-)###z", z.map->name);

			if (sel)
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.45f, 0.35f, 0.12f, 0.9f));
			if (ImGui::Selectable(row, sel, ImGuiSelectableFlags_AllowDoubleClick))
			{
				SetFocusMap(z.id);
				if (ImGui::IsMouseDoubleClicked(0))
				{
					gTab = 0;
					gTabSelectOnce = true;
				}
			}
			if (sel)
				ImGui::PopStyleColor();

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextColored(HelperTheme::Gold, "%s", z.map->name);
				ImGui::Text("%s / %s",
					z.map->release[0] ? z.map->release : DefaultRelease(),
					z.map->region[0] ? z.map->region : DefaultRegion());
				ImGui::Separator();
				if (z.total > 0)
				{
					for (int i = 0; i < static_cast<int>(ObjKind::Count); ++i)
					{
						const ObjKind k = static_cast<ObjKind>(i);
						const int kt = CountTotalKind(z.id, k);
						if (kt <= 0) continue;
						ImGui::Text("%s: %d / %d", ObjKindName(k), CountDoneKind(z.id, k), kt);
					}
				}
				else
					ImGui::TextColored(HelperTheme::Muted,
						"No live index yet - open zone or Checklist to load waypoints.");
				ImGui::TextColored(HelperTheme::Muted, "Double-click opens Checklist.");
				ImGui::EndTooltip();
			}

			if (ImGui::BeginPopupContextItem("##azctx"))
			{
				if (ImGui::MenuItem(fav ? "Unfavorite" : "Favorite"))
					ToggleFavorite(z.id);
				if (ImGui::MenuItem("Open checklist"))
				{
					SetFocusMap(z.id);
					gTab = 0;
					gTabSelectOnce = true;
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}
	}

	void DrawAtlasTab()
	{
		LoadFavorites();
		ImGui::InputTextWithHint("##atlas", "Filter name / region / release...",
			gAtlasFilter, sizeof(gAtlasFilter));
		ImGui::Checkbox("Favorites only###gw2igh_afav", &gAtlasFavOnly);
		DrawKindChips();
		EnsureCatalog();

		std::map<std::string, std::map<std::string, std::vector<ZoneAgg>>> tree;
		std::vector<ZoneAgg> favs;
		const size_t n = MapCount();
		for (size_t i = 0; i < n; ++i)
		{
			const MapInfo* m = MapAt(i);
			if (!m || !m->name[0]) continue;
			if (!MapMatchesFilter(*m)) continue;
			ZoneAgg z;
			z.id = m->id;
			z.map = m;
			z.done = CountDone(m->id);
			z.total = CountTotal(m->id);
			if (IsFavorite(m->id))
				favs.push_back(z);
			const char* rel = m->release[0] ? m->release : DefaultRelease();
			const char* reg = m->region[0] ? m->region : DefaultRegion();
			tree[rel][reg].push_back(z);
		}

		ImGui::BeginChild("##atlas_hier", ImVec2(0.f, 0.f), true);

		if (!favs.empty() && !gAtlasFavOnly)
		{
			if (ImGui::TreeNodeEx("Favorites###atlas_fav", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const ZoneAgg& z : favs)
					DrawZoneRow(z);
				ImGui::TreePop();
			}
			ImGui::Separator();
		}

		if (tree.empty())
		{
			ImGui::TextColored(HelperTheme::Muted, "No zones match filter.");
			ImGui::EndChild();
			return;
		}

		for (auto& relPair : tree)
		{
			int relZones = 0;
			for (auto& regPair : relPair.second)
				relZones += static_cast<int>(regPair.second.size());
			char relLab[160];
			std::snprintf(relLab, sizeof(relLab), "%s  (%d zones)###arel_%s",
				relPair.first.c_str(), relZones, relPair.first.c_str());
			if (!ImGui::TreeNodeEx(relLab, ImGuiTreeNodeFlags_DefaultOpen))
				continue;

			for (auto& regPair : relPair.second)
			{
				char regLab[160];
				std::snprintf(regLab, sizeof(regLab), "%s  (%zu)###areg_%s",
					regPair.first.c_str(), regPair.second.size(), regPair.first.c_str());
				if (!ImGui::TreeNodeEx(regLab, ImGuiTreeNodeFlags_DefaultOpen))
					continue;
				for (const ZoneAgg& z : regPair.second)
					DrawZoneRow(z);
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		ImGui::EndChild();
	}
}
