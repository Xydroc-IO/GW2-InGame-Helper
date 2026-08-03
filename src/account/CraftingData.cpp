#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "InventoryData.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	std::mutex gMu;
	Plan gPlan;
	Plan gPendingPlan;
	std::vector<DailyRow> gDailies;
	std::vector<DailyRow> gPendingDailies;
	std::string gDailyStatus;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gDailyBusy{false};
	std::atomic<bool> gReady{false};
	std::atomic<bool> gDailyReady{false};
	HANDLE gThread = nullptr;
	HANDLE gDailyThread = nullptr;
	char gQuery[192] = {};
	char gThreadQuery[192] = {};
	DWORD gDailyFetchedAt = 0;
	std::atomic<bool> gFocusTab{false};
	std::atomic<unsigned> gPlanGen{0};

	std::mutex gWikiMu;
	std::unordered_map<std::string, std::string> gWikiTextCache;
	std::mutex gRecipeCacheMu;

	void DrawNode(const IngNode& n)
	{
		ImGui::PushID(n.itemId + n.depth * 1000003);
		const char* name = n.name.empty() ? "..." : n.name.c_str();
		const int miss = (std::max)(0, n.need - n.have);
		const bool ok = miss <= 0;
		char label[256];
		std::snprintf(label, sizeof(label), "%s  %d / %d", name, n.have, n.need);
		if (!n.kids.empty())
		{
			if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const IngNode& k : n.kids)
					DrawNode(k);
				ImGui::TreePop();
			}
		}
		else
		{
			if (ok)
				ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.f), "%s", label);
			else
				ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.40f, 1.f), "%s", label);
			if (miss > 0 && n.buyUnit >= 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
					"buy %s", FormatCoins(n.buyUnit * miss).c_str());
			}
			else if (miss > 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "no TP");
			}
		}
		ImGui::PopID();
	}
	void Tick()
	{
		if (gThread && !gBusy && WaitForSingleObject(gThread, 0) == WAIT_OBJECT_0)
		{
			CloseHandle(gThread);
			gThread = nullptr;
		}
		if (gReady)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gReady)
			{
				gPlan = std::move(gPendingPlan);
				gPendingPlan = {};
				gReady = false;
			}
		}
		if (gDailyReady)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gDailyReady)
			{
				gDailies = std::move(gPendingDailies);
				gPendingDailies.clear();
				gDailyReady = false;
			}
			if (gDailyThread)
			{
				WaitForSingleObject(gDailyThread, 0);
				CloseHandle(gDailyThread);
				gDailyThread = nullptr;
			}
		}
	}

} // namespace CraftingDetail

using namespace CraftingDetail;

void CraftingData::QueuePlan(const char* itemNameOrCode)
{
	if (!itemNameOrCode || !itemNameOrCode[0]) return;
	std::snprintf(gQuery, sizeof(gQuery), "%s", itemNameOrCode);
	gFocusTab = true;
	StartPlan();
}

bool CraftingData::ConsumeFocusTab()
{
	return gFocusTab.exchange(false);
}

void CraftingData::RenderContents()
{
	Tick();
	StartDailies(false);

	Plan plan;
	std::vector<DailyRow> dailies;
	std::string dailyStatus;
	{
		std::lock_guard<std::mutex> lock(gMu);
		plan = gPlan;
		dailies = gDailies;
		dailyStatus = gDailyStatus;
	}

	ImGui::TextUnformatted("Crafting planner");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Station crafts use the official recipe API. Legendaries / gifts use wiki "
		"Mystic Forge trees (expandable to depth %d) — gifts, sub-gifts, then mats. "
		"Owned counts: materials, bank, shared (API key).",
		kMaxDepth);
	ImGui::PopTextWrapPos();

	const float btnW = ImGui::CalcTextSize("Plan").x + ImGui::GetStyle().FramePadding.x * 2.f + 16.f;
	float fieldW = ImGui::GetContentRegionAvail().x - btnW - ImGui::GetStyle().ItemSpacing.x;
	if (fieldW < 120.f) fieldW = 120.f;
	ImGui::SetNextItemWidth(fieldW);
	if (ImGui::InputTextWithHint("###gw2igh_craft_q", "[&…] / ID / name",
			gQuery, sizeof(gQuery), ImGuiInputTextFlags_EnterReturnsTrue))
		StartPlan();
	ImGui::SameLine();
	if (ImGui::Button("Plan###gw2igh_craft_go", ImVec2(btnW, 0.f)))
		StartPlan();

	if (gBusy && !plan.ok)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
			plan.status.empty() ? "Planning…" : plan.status.c_str());
	else if (gBusy && plan.ok)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s", plan.status.c_str());
	else if (!plan.status.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", plan.status.c_str());

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.f), "Daily crafting");
	if (gDailyBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Loading…");
	else if (dailies.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
			"%s", dailyStatus.empty() ? "—" : dailyStatus.c_str());
	else
	{
		for (const DailyRow& d : dailies)
			ImGui::BulletText("%s", d.name.c_str());
	}

	ImGui::Separator();

	const float listH = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("###gw2igh_craft_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);

	if (plan.ok)
	{
		ImGui::TextColored(ImVec4(0.85f, 0.80f, 0.95f, 1.f), "%s",
			plan.outputName.empty() ? "Output" : plan.outputName.c_str());
		if (!plan.recipeSource.empty())
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
				"#%d · %s · crafts %d", plan.outputId, plan.recipeSource.c_str(),
				plan.outputCount);
		else
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
				"#%d · crafts %d per recipe", plan.outputId, plan.outputCount);
		if (plan.noTpMissing > 0)
		{
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f),
				"TP buy (instant): %s", FormatCoins(plan.buyTotal).c_str());
			ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f),
				"Some missing mats are account-bound / not on the TP.");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f),
				"TP buy (instant): %s", FormatCoins(plan.buyTotal).c_str());
		}
		ImGui::Spacing();
		for (const IngNode& k : plan.root.kids)
			DrawNode(k);
	}
	else if (!plan.nameHints.empty())
	{
		ImGui::TextUnformatted("Wiki results");
		for (size_t i = 0; i < plan.nameHints.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Selectable(plan.nameHints[i].c_str()))
			{
				std::snprintf(gQuery, sizeof(gQuery), "%s", plan.nameHints[i].c_str());
				StartPlan();
			}
			ImGui::PopID();
		}
	}
	else if (!gBusy)
	{
		ImGui::TextWrapped(
			"Try an ascended food, gift, or crafted gear name — or Shift+click a chat code.");
	}

	ImGui::EndChild();
}
