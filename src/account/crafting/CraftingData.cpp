#include "CraftingData.h"
#include "PadLayout.h"
#include "PadNav.h"

#include "CraftingShared.h"

#include "HelperTheme.h"

#include "imgui/imgui.h"

#include <atomic>
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
	int gThreadQty = 1;
	int gPlanQty = 1;
	DWORD gDailyFetchedAt = 0;
	std::atomic<bool> gFocusTab{false};
	std::atomic<unsigned> gPlanGen{0};
	PlanOpts gOpts{};

	std::mutex gWikiMu;
	std::unordered_map<std::string, std::string> gWikiTextCache;
	std::mutex gRecipeCacheMu;

	void Tick()
	{
		KnownTick();
		CartPlanTick();
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
	StartPlanWithQty(1);
}

void CraftingData::RequestFocusTab()
{
	gFocusTab = true;
}

bool CraftingData::ConsumeFocusTab()
{
	return gFocusTab.exchange(false);
}

void CraftingData::RenderContents()
{
	static bool optsLoaded = false;
	if (!optsLoaded)
	{
		optsLoaded = true;
		LoadCraftOpts();
	}

	Tick();
	StartDailies(false);
	StartKnown(false);
	CartEnsureLoaded();

	Plan plan;
	std::vector<DailyRow> dailies;
	std::string dailyStatus;
	{
		std::lock_guard<std::mutex> lock(gMu);
		plan = gPlan;
		dailies = gDailies;
		dailyStatus = gDailyStatus;
	}
	std::vector<Plan> cartPlans = CartPlansCopy();

	PadNav::Blurb(
		"Known recipes (per character), multi-item craft cart with project rollup, "
		"buy-vs-craft plans, shopping check-offs, and craft steps. "
		"API key needs unlocks + characters + inventories.");

	DrawOptsBar();

	ImGui::SetNextItemWidth(56.f);
	ImGui::InputInt("###gw2igh_craft_qty", &gPlanQty);
	if (gPlanQty < 1) gPlanQty = 1;
	ImGui::SameLine();
	const float btnW = ImGui::CalcTextSize("Plan").x + ImGui::GetStyle().FramePadding.x * 2.f + 16.f;
	float fieldW = ImGui::GetContentRegionAvail().x - btnW - ImGui::GetStyle().ItemSpacing.x;
	if (fieldW < 100.f) fieldW = 100.f;
	ImGui::SetNextItemWidth(fieldW);
	if (ImGui::InputTextWithHint("###gw2igh_craft_q", "[&...] / ID / name",
			gQuery, sizeof(gQuery), ImGuiInputTextFlags_EnterReturnsTrue))
		StartPlan();
	ImGui::SameLine();
	if (ImGui::Button("Plan###gw2igh_craft_go", ImVec2(btnW, 0.f)))
		StartPlan();

	if (gBusy && !plan.ok)
		PadNav::StatusBusy(plan.status.empty() ? "Planning..." : plan.status.c_str());
	else if (gBusy && plan.ok)
		PadNav::StatusBusy(plan.status.c_str());
	else if (!plan.status.empty())
		PadNav::StatusOk(plan.status.c_str());
	if (CartPlanBusy())
		PadNav::StatusBusy(CartPlanStatus().c_str());
	else if (!CartPlanStatus().empty() && !cartPlans.empty())
		PadNav::StatusOk(CartPlanStatus().c_str());

	ImGui::Separator();
	PadNav::SectionTitle("Daily crafting");
	if (gDailyBusy)
		PadNav::StatusBusy("Loading...");
	else if (dailies.empty())
		ImGui::TextColored(HelperTheme::Muted,
			"%s", dailyStatus.empty() ? "-" : dailyStatus.c_str());
	else
	{
		if (!dailyStatus.empty())
			ImGui::TextColored(HelperTheme::Muted, "%s", dailyStatus.c_str());
		for (const DailyRow& d : dailies)
		{
			if (d.done)
				ImGui::TextColored(HelperTheme::Ok, "Done  %s", d.name.c_str());
			else
				ImGui::TextColored(HelperTheme::Warn, "Todo  %s", d.name.c_str());
		}
	}

	ImGui::Separator();
	DrawKnownRail();
	ImGui::Separator();
	DrawCartUi();
	ImGui::Separator();

	PadLayout::BeginList("###gw2igh_craft_list", 80.f);

	if (!cartPlans.empty())
		DrawAggregatedResults(cartPlans, true);
	else if (plan.ok)
		DrawPlanResults(plan, true);
	else if (!plan.nameHints.empty())
	{
		PadNav::Meta("Wiki results");
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
	else if (!gBusy && !CartPlanBusy())
	{
		ImGui::TextWrapped(
			"Plan an item, browse Known, or Plan project on the craft cart.");
	}

	PadLayout::EndList();
}
