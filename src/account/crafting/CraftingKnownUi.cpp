#include "CraftingData.h"

#include "CraftingKnownInternal.h"
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

#include <windows.h>

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
				"API key needs progression + characters scopes.");
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
		/* In-pad cycle — Combo popup clicks often miss under Nexus capture. */
		{
			const int n = static_cast<int>(labels.size());
			auto applyIdx = [&](int idx) {
				gSelectedCharIdx = idx;
				if (gSelectedCharIdx <= 0)
					gSelectedChar[0] = 0;
				else
					std::snprintf(gSelectedChar, sizeof(gSelectedChar), "%s",
						chars[static_cast<size_t>(gSelectedCharIdx - 1)].c_str());
			};
			if (ImGui::ArrowButton("###gw2igh_known_prev", ImGuiDir_Left))
				applyIdx((gSelectedCharIdx + n - 1) % n);
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(labels[static_cast<size_t>(gSelectedCharIdx)]);
			ImGui::SameLine();
			if (ImGui::ArrowButton("###gw2igh_known_next", ImGuiDir_Right))
				applyIdx((gSelectedCharIdx + 1) % n);
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputTextWithHint("###gw2igh_known_filt", "Filter known outputs…",
			gKnownFilter, sizeof(gKnownFilter));

		const char* filterChar = gSelectedChar[0] ? gSelectedChar : nullptr;

		/* Cache recipe-id list — union+sort is expensive for big accounts. */
		static std::vector<int> sIds;
		static int sIdsCharIdx = -999;
		static char sIdsChar[64] = {};
		static DWORD sIdsAt = 0;
		const DWORD now = GetTickCount();
		const bool idsDirty = sIdsCharIdx != gSelectedCharIdx ||
			std::strcmp(sIdsChar, gSelectedChar) != 0 ||
			(now - sIdsAt) > 2000;
		if (idsDirty)
		{
			sIdsCharIdx = gSelectedCharIdx;
			std::snprintf(sIdsChar, sizeof(sIdsChar), "%s", gSelectedChar);
			sIds = KnownRecipeIdsForChar(filterChar);
			sIdsAt = now;
		}

		ImGui::TextColored(HelperTheme::Muted, "%zu recipes", sIds.size());
		const char* knownStatus = KnownStatus();
		if (knownStatus && knownStatus[0])
			ImGui::TextColored(HelperTheme::Muted, "%s", knownStatus);
		if (sIds.empty()) return;

		/* Cap enqueue per frame — scanning/queuing the full set under lock freezes Present. */
		EnsureNextKnownRecipeDetails(sIds, kDetailEnqueuePerFrame);

		/* Ready count is O(n) under the shared mutex — sample at most ~4×/sec. */
		static size_t sReady = 0;
		static DWORD sReadyAt = 0;
		if (sReadyAt == 0 || (now - sReadyAt) > 250)
		{
			sReady = KnownDetailsReadyCount(sIds);
			sReadyAt = now;
		}
		const size_t ready = sReady;
		if (ready < sIds.size())
			ImGui::TextColored(HelperTheme::Muted, "Loading details %zu / %zu…",
				ready, sIds.size());

		/* Rebuild grouped list only when data / filter changes — not every Present. */
		static size_t sReadyCached = static_cast<size_t>(-1);
		static DWORD sRebuildAt = 0;
		static int sUiCharIdx = -999;
		static char sUiFilter[64] = { '\x01' }; /* force first rebuild */
		static std::map<std::string, std::vector<KnownRecipeInfo>> sByDisc;

		const bool filterDirty = sUiCharIdx != gSelectedCharIdx ||
			std::strcmp(sUiFilter, gKnownFilter) != 0;
		const bool readyJump = ready != sReadyCached &&
			(ready >= sIds.size() ||
				ready >= sReadyCached + 200 ||
				(sRebuildAt != 0 && (now - sRebuildAt) > 500));
		if (filterDirty || readyJump || sReadyCached == static_cast<size_t>(-1))
		{
			sReadyCached = ready;
			sRebuildAt = now;
			sUiCharIdx = gSelectedCharIdx;
			std::snprintf(sUiFilter, sizeof(sUiFilter), "%s", gKnownFilter);
			sByDisc.clear();
			std::vector<KnownRecipeInfo> details;
			CopyKnownRecipeDetails(sIds, details, nullptr);
			for (KnownRecipeInfo& info : details)
			{
				const char* nm = info.outputName.empty() ? "" : info.outputName.c_str();
				if (!NameMatchesFilter(nm, gKnownFilter)) continue;
				const char* disc = info.discipline.empty() ? "Other" : info.discipline.c_str();
				sByDisc[disc].push_back(std::move(info));
			}
		}

		if (sByDisc.empty())
		{
			ImGui::TextColored(HelperTheme::Muted,
				ready < sIds.size() ? "Resolving recipe outputs…" : "No matches.");
			return;
		}

		for (auto& kv : sByDisc)
		{
			char header[128];
			std::snprintf(header, sizeof(header), "%s (%zu)###gw2igh_kd_%s",
				kv.first.c_str(), kv.second.size(), kv.first.c_str());
			/* Start collapsed — big accounts open dozens of discipline groups. */
			if (!ImGui::TreeNodeEx(header, 0))
				continue;

			std::vector<KnownRecipeInfo>& rows = kv.second;
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(rows.size()));
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				{
					KnownRecipeInfo& info = rows[static_cast<size_t>(i)];
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
			}
			ImGui::TreePop();
		}
	}

} // namespace CraftingDetail
