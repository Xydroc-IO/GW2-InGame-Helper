#include "CraftingData.h"
#include "CraftingPad.h"
#include "PadLayout.h"
#include "PadNav.h"

#include "CraftingShared.h"

#include "Gw2Ui.h"
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

	/* Inner Crafting tabs — Plan | Known | Browse | Craft cart. */
	enum CraftSub : int
	{
		kSubPlan = 0,
		kSubKnown,
		kSubBrowse,
		kSubCart,
		kSubCount
	};
	static int gCraftSub = kSubPlan;
	static std::atomic<int> gForceCraftSub{-1};

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

	static void DrawPlanTab(const Plan& plan, const std::vector<DailyRow>& dailies,
		const std::string& dailyStatus, const std::vector<Plan>& cartPlans)
	{
		PadNav::SectionTitle("Plan item");
		ImGui::SetNextItemWidth(48.f);
		ImGui::InputInt("Qty###gw2igh_craft_qty", &gPlanQty);
		if (gPlanQty < 1) gPlanQty = 1;
		ImGui::SameLine();
		const float btnW = PadLayout::GoldButtonWidth("Plan###gw2igh_craft_go");
		float fieldW = ImGui::GetContentRegionAvail().x - btnW - ImGui::GetStyle().ItemSpacing.x;
		if (fieldW < 100.f) fieldW = 100.f;
		ImGui::SetNextItemWidth(fieldW);
		if (ImGui::InputTextWithHint("###gw2igh_craft_q", "Item id, [&code], or name…",
				gQuery, sizeof(gQuery), ImGuiInputTextFlags_EnterReturnsTrue))
			StartPlan();
		ImGui::SameLine();
		if (PadLayout::GoldButton("Plan###gw2igh_craft_go", true, true))
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

		ImGui::Spacing();
		DrawOptsBar();

		int dailyDone = 0;
		for (const DailyRow& d : dailies)
			if (d.done) ++dailyDone;
		char dailyHdr[96];
		if (dailies.empty())
			std::snprintf(dailyHdr, sizeof(dailyHdr), "Daily crafting###gw2igh_craft_daily");
		else
			std::snprintf(dailyHdr, sizeof(dailyHdr),
				"Daily crafting (%d / %d)###gw2igh_craft_daily",
				dailyDone, static_cast<int>(dailies.size()));
		const ImGuiTreeNodeFlags dailyFlags = (plan.ok || (dailyDone >= static_cast<int>(dailies.size())
			&& !dailies.empty()))
			? 0 : ImGuiTreeNodeFlags_DefaultOpen;
		if (ImGui::CollapsingHeader(dailyHdr, dailyFlags))
		{
			if (gDailyBusy)
				PadNav::StatusBusy("Loading...");
			else if (dailies.empty())
				ImGui::TextColored(HelperTheme::Muted,
					"%s", dailyStatus.empty() ? "No dailies loaded yet." : dailyStatus.c_str());
			else
			{
				if (!dailyStatus.empty())
					ImGui::TextColored(HelperTheme::Muted, "%s", dailyStatus.c_str());
				const float prog = static_cast<float>(dailyDone)
					/ static_cast<float>(dailies.size());
				ImGui::ProgressBar(prog, ImVec2(-1.f, 0.f), "");
				if (ImGui::BeginTable("###gw2igh_craft_daily_tbl", 3,
						ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg
						| ImGuiTableFlags_PadOuterX))
				{
					const float planW = PadLayout::GoldButtonWidth(
						"Plan###gw2igh_craft_dplan", true);
					ImGui::TableSetupColumn("##st", ImGuiTableColumnFlags_WidthFixed, 48.f);
					ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("##go", ImGuiTableColumnFlags_WidthFixed, planW + 4.f);
					for (const DailyRow& d : dailies)
					{
						ImGui::TableNextRow();
						ImGui::PushID(d.slug.c_str());
						ImGui::TableSetColumnIndex(0);
						const char* chip = d.done ? "Done" : "Todo";
						const ImVec4 chipFill = d.done ? HelperTheme::Ok : HelperTheme::Header;
						const ImVec4 chipText = d.done ? HelperTheme::Ink : HelperTheme::GoldMuted;
						PadLayout::Chip(chip, chipFill, chipText);
						ImGui::TableSetColumnIndex(1);
						ImGui::TextUnformatted(d.name.c_str());
						ImGui::TableSetColumnIndex(2);
						if (PadLayout::GoldButton("Plan###gw2igh_craft_dplan", false, true, true))
						{
							std::snprintf(gQuery, sizeof(gQuery), "%s", d.name.c_str());
							StartPlan();
						}
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
			}
		}

		ImGui::Separator();
		PadLayout::BeginList("###gw2igh_craft_plan_list", 80.f);

		if (!cartPlans.empty())
		{
			ImGui::TextColored(HelperTheme::Muted,
				"Showing Craft cart project rollup — open Craft cart to edit the project.");
			DrawAggregatedResults(cartPlans, true);
		}
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
				"Plan an item here, browse Known for unlocks, or build a multi-item "
				"project on Craft cart.");
		}

		PadLayout::EndList();
	}

	static void DrawCartTabBody(const std::vector<Plan>& cartPlans)
	{
		DrawCartUi();
		ImGui::Separator();
		PadLayout::BeginList("###gw2igh_craft_cart_list", 80.f);
		if (!cartPlans.empty())
			DrawAggregatedResults(cartPlans, true);
		else if (CartPlanBusy())
			PadNav::StatusBusy(CartPlanStatus().c_str());
		else
			ImGui::TextColored(HelperTheme::Muted,
				"Add items with + on Known, or from the current Plan tab, then Plan project.");
		PadLayout::EndList();
	}

} // namespace CraftingDetail

using namespace CraftingDetail;

void CraftingData::SetKnownDetailsActive(bool active)
{
	SetKnownDetailsPump(active);
}

void CraftingData::QueuePlan(const char* itemNameOrCode)
{
	if (!itemNameOrCode || !itemNameOrCode[0]) return;
	std::snprintf(gQuery, sizeof(gQuery), "%s", itemNameOrCode);
	gForceCraftSub = kSubPlan;
	gFocusTab = true;
	StartPlanWithQty(1);
	CraftingPad::OpenAndRefresh();
}

void CraftingData::RequestFocusTab()
{
	gFocusTab = true;
	CraftingPad::OpenAndRefresh();
}

void CraftingData::RequestFocusCraftCart()
{
	gForceCraftSub = kSubCart;
	gFocusTab = true;
	CraftingPad::OpenAndRefresh();
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

	const int forced = gForceCraftSub.exchange(-1);
	if (forced >= 0 && forced < kSubCount)
		gCraftSub = forced;

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

	static const char* kSubs[] = { "Plan", "Known", "Browse", "Craft cart" };
	static const int kSubIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Bag),
		static_cast<int>(Gw2Ui::Icon::Check),
		static_cast<int>(Gw2Ui::Icon::Story),
		static_cast<int>(Gw2Ui::Icon::Inventory),
	};
	gCraftSub = PadNav::DrawSideRail("###gw2igh_craft_nav", kSubs, kSubCount, gCraftSub,
		0.f, kSubIcons);

	/* Only resolve known-recipe names while the Known sub-tab is open. */
	SetKnownDetailsActive(gCraftSub == kSubKnown);

	ImGui::BeginChild("###gw2igh_craft_body", ImVec2(0.f, 0.f), true);

	switch (gCraftSub)
	{
	case kSubPlan:
		PadNav::Blurb(
			"Plan buy-vs-craft costs, track dailies, and build a shopping list.");
		DrawPlanTab(plan, dailies, dailyStatus, cartPlans);
		break;
	case kSubKnown:
		PadNav::Blurb(
			"Per-character known recipes. Details resolve in the background while this tab is open.");
		DrawKnownRail();
		break;
	case kSubBrowse:
		PadNav::Blurb("Recipe browser and leveling paths.");
		DrawRecipeBrowser();
		ImGui::Separator();
		DrawLevelingPaths();
		break;
	case kSubCart:
		PadNav::Blurb(
			"Multi-item craft projects (not the Economy TP Cart). Plan project rolls shopping + steps.");
		DrawCartTabBody(cartPlans);
		break;
	default:
		break;
	}

	ImGui::EndChild();
}
