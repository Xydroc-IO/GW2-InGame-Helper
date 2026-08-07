#include "CraftingData.h"

#include "CraftingShared.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	std::mutex gCartPlanMu;
	std::vector<Plan> gCartPlans;
	std::vector<Plan> gPendingCartPlans;
	std::atomic<bool> gCartPlanBusy{false};
	std::atomic<bool> gCartPlanReady{false};
	std::string gCartPlanStatus;
	static HANDLE gCartPlanThread = nullptr;
	static std::atomic<unsigned> gCartPlanGen{0};

	DWORD WINAPI CartPlanProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		const unsigned gen = gCartPlanGen.load();
		std::vector<CartItem> items = CartItems(nullptr);
		std::vector<Plan> plans;
		plans.reserve(items.size());

		for (size_t i = 0; i < items.size(); ++i)
		{
			if (gen != gCartPlanGen.load())
				break;
			const CartItem& it = items[i];
			{
				std::lock_guard<std::mutex> lock(gCartPlanMu);
				char st[128];
				std::snprintf(st, sizeof(st), "Planning %zu / %zu: %s...",
					i + 1, items.size(), it.name[0] ? it.name : "item");
				gCartPlanStatus = st;
			}
			Plan plan;
			const std::string name = it.name[0] ? it.name : ItemName(it.id);
			ExpandAndPricePlan(plan, it.id, name, it.qty < 1 ? 1 : it.qty,
				0, false, false);
			if (plan.ok)
				plans.push_back(std::move(plan));
			else
			{
				Plan fail;
				fail.outputId = it.id;
				fail.outputName = name;
				fail.wantQty = it.qty;
				fail.status = plan.status.empty() ? "Could not plan." : plan.status;
				plans.push_back(std::move(fail));
			}
		}

		{
			std::lock_guard<std::mutex> lock(gCartPlanMu);
			if (gen == gCartPlanGen.load())
			{
				gPendingCartPlans = std::move(plans);
				char st[96];
				std::snprintf(st, sizeof(st), "Project plan ready (%zu items).",
					gPendingCartPlans.size());
				gCartPlanStatus = st;
				gCartPlanReady = true;
			}
			gCartPlanBusy = false;
		}
		return 0;
	}

	void StartCartProjectPlan()
	{
		CartEnsureLoaded();
		if (CartItems(nullptr).empty()) return;
		if (gCartPlanBusy.exchange(true)) return;
		++gCartPlanGen;
		{
			std::lock_guard<std::mutex> lock(gCartPlanMu);
			gCartPlanStatus = "Planning project...";
			gCartPlans.clear();
		}
		if (gCartPlanThread)
		{
			WaitForSingleObject(gCartPlanThread, 0);
			CloseHandle(gCartPlanThread);
			gCartPlanThread = nullptr;
		}
		gCartPlanThread = CreateThread(nullptr, 0, CartPlanProc, nullptr, 0, nullptr);
		if (!gCartPlanThread) gCartPlanBusy = false;
	}

	bool CartPlanBusy() { return gCartPlanBusy.load(); }

	void CartPlanTick()
	{
		if (!gCartPlanReady) return;
		std::lock_guard<std::mutex> lock(gCartPlanMu);
		if (!gCartPlanReady) return;
		gCartPlans = std::move(gPendingCartPlans);
		gPendingCartPlans.clear();
		gCartPlanReady = false;
		if (gCartPlanThread)
		{
			WaitForSingleObject(gCartPlanThread, 0);
			CloseHandle(gCartPlanThread);
			gCartPlanThread = nullptr;
		}
	}

	std::vector<Plan> CartPlansCopy()
	{
		CartPlanTick();
		std::lock_guard<std::mutex> lock(gCartPlanMu);
		return gCartPlans;
	}

	std::string CartPlanStatus()
	{
		std::lock_guard<std::mutex> lock(gCartPlanMu);
		return gCartPlanStatus;
	}

} // namespace CraftingDetail
