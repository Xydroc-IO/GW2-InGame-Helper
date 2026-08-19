#include "CraftingData.h"

#include "CraftingShared.h"

#include "Gw2Catalog.h"
#include "Gw2Http.h"
#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace CraftingDetail
{
	namespace
	{
		char gBrowseQ[96] = {};
		std::mutex gBrowseMu;
		std::vector<std::pair<int, std::string>> gBrowseHits;
		std::vector<std::pair<int, std::string>> gBrowsePending;
		std::atomic<bool> gBrowseBusy{false};
		std::atomic<bool> gBrowseReady{false};
		char gBrowseStatus[96] = {};
		char gBrowsePendingStatus[96] = {};
	}

	void DrawRecipeBrowser()
	{
		if (gBrowseReady.exchange(false))
		{
			std::lock_guard<std::mutex> lock(gBrowseMu);
			gBrowseHits = std::move(gBrowsePending);
			std::snprintf(gBrowseStatus, sizeof(gBrowseStatus), "%s", gBrowsePendingStatus);
		}

		PadNav::SectionTitle("Recipe browser");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Search by output item id (catalog, then /v2/recipes/search).");
		PadNav::PopWrap();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
		ImGui::InputTextWithHint("###gw2igh_rb_q", "Output item id…", gBrowseQ, sizeof(gBrowseQ));
		ImGui::SameLine();
		const bool canGo = gBrowseQ[0] && !gBrowseBusy.load();
		if (ImGui::Button("Search###gw2igh_rb_go") && canGo)
		{
			const int outId = std::atoi(gBrowseQ);
			if (outId <= 0)
			{
				std::snprintf(gBrowseStatus, sizeof(gBrowseStatus), "Enter a numeric item id.");
			}
			else if (!gBrowseBusy.exchange(true))
			{
				std::snprintf(gBrowseStatus, sizeof(gBrowseStatus), "Searching…");
				std::thread([outId]() {
					std::vector<std::pair<int, std::string>> hits;
					char status[96] = {};
					std::vector<int> ids;
					if (Gw2Catalog::RecipesForOutput(outId, &ids))
					{
						for (int rid : ids)
						{
							if (rid <= 0) continue;
							char label[96];
							std::snprintf(label, sizeof(label), "Recipe #%d  item %d", rid, outId);
							hits.emplace_back(rid, label);
						}
						std::snprintf(status, sizeof(status), "%d recipe(s).",
							static_cast<int>(hits.size()));
					}
					else
					{
						char path[96];
						std::snprintf(path, sizeof(path), "/v2/recipes/search?output=%d", outId);
						auto r = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
						if (!r.ok)
							std::snprintf(status, sizeof(status), "Search failed.");
						else
						{
							ParseIntArray(r.body, ids);
							for (int rid : ids)
							{
								if (rid <= 0) continue;
								char label[96];
								std::snprintf(label, sizeof(label), "Recipe #%d  item %d", rid, outId);
								hits.emplace_back(rid, label);
							}
							std::snprintf(status, sizeof(status), "%d recipe(s).",
								static_cast<int>(hits.size()));
						}
					}
					{
						std::lock_guard<std::mutex> lock(gBrowseMu);
						gBrowsePending = std::move(hits);
						std::snprintf(gBrowsePendingStatus, sizeof(gBrowsePendingStatus), "%s", status);
						gBrowseReady = true;
					}
					gBrowseBusy = false;
				}).detach();
			}
		}
		if (gBrowseBusy.load())
			PadNav::StatusBusy("Searching…");
		else if (gBrowseStatus[0])
			ImGui::TextColored(HelperTheme::Muted, "%s", gBrowseStatus);
		for (size_t i = 0; i < gBrowseHits.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(gBrowseHits[i].first));
			if (ImGui::Selectable(gBrowseHits[i].second.c_str()) || ImGui::SmallButton("Plan"))
			{
				std::snprintf(gQuery, sizeof(gQuery), "%d", std::atoi(gBrowseQ));
				StartPlan();
			}
			ImGui::PopID();
		}
	}
}
