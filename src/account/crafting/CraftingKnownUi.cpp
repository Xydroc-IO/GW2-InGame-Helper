#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace CraftingDetail
{
	static char gSelectedChar[64] = {};
	static int gSelectedCharIdx = 0;
	static char gKnownFilter[64] = {};

	const char* SelectedKnownChar()
	{
		return gSelectedChar;
	}

	static bool NameMatchesFilter(const char* name, const char* filter)
	{
		if (!filter || !filter[0]) return true;
		if (!name) return false;
		std::string n = name;
		std::string f = filter;
		for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		for (char& c : f) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return n.find(f) != std::string::npos;
	}

	void DrawKnownRail()
	{
		PadNav::SectionTitle("Known recipes");
		if (!G::Gw2ApiKey[0])
		{
			ImGui::TextColored(HelperTheme::Muted,
				"API key needs unlocks + characters + inventories scopes.");
			return;
		}
		if (KnownBusy() && !KnownHasFetched())
		{
			PadNav::StatusBusy("Loading known recipes...");
			return;
		}
		if (!KnownHasFetched())
		{
			ImGui::TextColored(HelperTheme::Muted, "No known-recipe data yet.");
			return;
		}

		std::vector<std::string> chars = KnownCharacterNames();
		std::vector<std::string> labelStore;
		labelStore.push_back("All characters");
		for (const std::string& c : chars)
			labelStore.push_back(c);
		std::vector<const char*> labels;
		for (const std::string& s : labelStore)
			labels.push_back(s.c_str());

		if (gSelectedCharIdx >= static_cast<int>(labels.size()))
			gSelectedCharIdx = 0;
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if (ImGui::Combo("###gw2igh_known_char", &gSelectedCharIdx, labels.data(),
				static_cast<int>(labels.size())))
		{
			if (gSelectedCharIdx <= 0)
				gSelectedChar[0] = 0;
			else
				std::snprintf(gSelectedChar, sizeof(gSelectedChar), "%s",
					chars[static_cast<size_t>(gSelectedCharIdx - 1)].c_str());
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputTextWithHint("###gw2igh_known_filt", "Filter known outputs…",
			gKnownFilter, sizeof(gKnownFilter));

		const char* filterChar = gSelectedChar[0] ? gSelectedChar : nullptr;
		std::vector<int> ids = KnownRecipeIdsForChar(filterChar);
		ImGui::TextColored(HelperTheme::Muted, "%zu recipes", ids.size());
		if (ids.empty()) return;

		static int sDetIdx = -999;
		static char sDetChar[64] = {};
		static size_t sDetOff = 0;
		if (sDetIdx != gSelectedCharIdx || std::strcmp(sDetChar, gSelectedChar) != 0)
		{
			sDetIdx = gSelectedCharIdx;
			std::snprintf(sDetChar, sizeof(sDetChar), "%s", gSelectedChar);
			sDetOff = 0;
		}
		/* Progressive detail fill (20 / frame burst on selection) so rail stays full. */
		if (sDetOff < ids.size())
		{
			std::vector<int> batch;
			const size_t end = (std::min)(ids.size(), sDetOff + 20);
			for (size_t i = sDetOff; i < end; ++i)
				batch.push_back(ids[i]);
			sDetOff = end;
			EnsureKnownRecipeDetails(batch);
			if (sDetOff < ids.size())
				ImGui::TextColored(HelperTheme::Muted, "Loading details %zu / %zu…",
					sDetOff, ids.size());
		}

		std::map<std::string, std::vector<KnownRecipeInfo>> byDisc;
		for (int id : ids)
		{
			KnownRecipeInfo info;
			if (!GetKnownRecipeDetail(id, info)) continue;
			const char* nm = info.outputName.empty() ? "" : info.outputName.c_str();
			if (!NameMatchesFilter(nm, gKnownFilter)) continue;
			const char* disc = info.discipline.empty() ? "Other" : info.discipline.c_str();
			byDisc[disc].push_back(info);
		}

		if (byDisc.empty())
		{
			ImGui::TextColored(HelperTheme::Muted,
				sDetOff < ids.size() ? "Resolving recipe outputs…" : "No matches.");
			return;
		}

		for (auto& kv : byDisc)
		{
			char header[128];
			std::snprintf(header, sizeof(header), "%s (%zu)###gw2igh_kd_%s",
				kv.first.c_str(), kv.second.size(), kv.first.c_str());
			if (!ImGui::TreeNodeEx(header, ImGuiTreeNodeFlags_DefaultOpen))
				continue;
			for (const KnownRecipeInfo& info : kv.second)
			{
				ImGui::PushID(info.recipeId);
				const char* nm = info.outputName.empty() ? "Item" : info.outputName.c_str();
				ImGui::TextColored(HelperTheme::Ok, "✓");
				ImGui::SameLine();
				if (ImGui::Selectable(nm))
				{
					std::snprintf(gQuery, sizeof(gQuery), "%s", nm);
					StartPlan();
				}
				if (ImGui::IsItemHovered())
				{
					auto who = CharsKnowing(info.recipeId);
					ImGui::BeginTooltip();
					ImGui::Text("Recipe #%d → item #%d", info.recipeId, info.outputId);
					if (KnownByAccount(info.recipeId))
						ImGui::TextUnformatted("Account unlock");
					if (!who.empty())
					{
						ImGui::TextUnformatted("Known by:");
						for (const std::string& c : who)
							ImGui::BulletText("%s", c.c_str());
					}
					ImGui::EndTooltip();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("+"))
					CartAdd(info.outputId, nm, gPlanQty < 1 ? 1 : gPlanQty, nullptr);
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
	}

} // namespace CraftingDetail
