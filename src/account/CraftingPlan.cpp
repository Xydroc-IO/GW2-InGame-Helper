#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "InventoryData.h"

#include <algorithm>
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
	void PublishLivePlan(const Plan& plan)
	{
		std::lock_guard<std::mutex> lock(gMu);
		gPlan = plan;
	}

	void FinishPrices(Plan& plan, std::unordered_map<int, std::string>& names)
	{
		std::vector<int> allIds;
		allIds.push_back(plan.outputId);
		CollectLeafIds(plan.root, allIds);
		for (const IngNode& k : plan.root.kids)
		{
			allIds.push_back(k.itemId);
			CollectLeafIds(k, allIds);
		}
		std::sort(allIds.begin(), allIds.end());
		allIds.erase(std::unique(allIds.begin(), allIds.end()), allIds.end());
		FetchNames(names, allIds);
		std::vector<IngNode*> stack;
		stack.push_back(&plan.root);
		while (!stack.empty())
		{
			IngNode* cur = stack.back();
			stack.pop_back();
			if (names.count(cur->itemId))
				cur->name = names[cur->itemId];
			for (IngNode& c : cur->kids) stack.push_back(&c);
		}

		std::vector<int> leafIds;
		CollectLeafIds(plan.root, leafIds);
		std::sort(leafIds.begin(), leafIds.end());
		leafIds.erase(std::unique(leafIds.begin(), leafIds.end()), leafIds.end());
		std::unordered_map<int, long long> sells;
		FetchPrices(sells, leafIds);
		plan.buyTotal = 0;
		plan.noTpMissing = 0;
		ApplyPrices(plan.root, sells, plan.buyTotal, plan.noTpMissing);

		char buf[192];
		const char* src = plan.recipeSource.empty() ? "recipe" : plan.recipeSource.c_str();
		if (plan.noTpMissing > 0)
		{
			std::snprintf(buf, sizeof(buf),
				"%s · TP buy ~ %s (+ bound/no-TP mats)",
				src, FormatCoins(plan.buyTotal).c_str());
		}
		else
		{
			std::snprintf(buf, sizeof(buf),
				"%s · TP buy ~ %s",
				src, FormatCoins(plan.buyTotal).c_str());
		}
		plan.status = buf;
	}

	/* Expand one frontier of leaves in parallel (keeps legendary gift trees snappy). */
	void ExpandFrontier(Plan& plan, std::unordered_map<int, int>& owned,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, RecipeCacheEntry>& recipeCache, int childDepth)
	{
		struct Front
		{
			IngNode* node = nullptr;
			bool ok = false;
			int outCount = 1;
			std::vector<RecipeIng> ings;
		};
		std::vector<Front> fronts;
		std::vector<IngNode*> stack;
		stack.push_back(&plan.root);
		while (!stack.empty())
		{
			IngNode* n = stack.back();
			stack.pop_back();
			if (!n->kids.empty())
			{
				for (IngNode& c : n->kids) stack.push_back(&c);
				continue;
			}
			if (n->depth != childDepth - 1) continue;
			if (n->have >= n->need) continue;
			if (IsTerminalMaterial(n->name)) continue;
			fronts.push_back({ n, false, 1, {} });
		}
		if (fronts.empty()) return;

		/* Cap parallelism — wall clock ≈ slowest call (API Check style), not the sum. */
		constexpr size_t kMaxParallel = 8;
		for (size_t off = 0; off < fronts.size(); off += kMaxParallel)
		{
			const size_t batch = (std::min)(fronts.size() - off, kMaxParallel);
			struct Job
			{
				Front* f = nullptr;
				std::unordered_map<int, RecipeCacheEntry>* cache = nullptr;
				static DWORD WINAPI Thunk(void* p)
				{
					auto* j = static_cast<Job*>(p);
					Front& f = *j->f;
					std::string hint = f.node->name;
					int recipeId = 0;
					f.ok = TryLoadRecipe(f.node->itemId, hint, f.outCount, f.ings, recipeId,
						*j->cache, nullptr);
					return 0;
				}
			};
			std::vector<Job> jobs(batch);
			std::vector<HANDLE> hs;
			hs.reserve(batch);
			for (size_t i = 0; i < batch; ++i)
			{
				jobs[i].f = &fronts[off + i];
				jobs[i].cache = &recipeCache;
				HANDLE h = CreateThread(nullptr, 0, Job::Thunk, &jobs[i], 0, nullptr);
				if (h) hs.push_back(h);
				else Job::Thunk(&jobs[i]);
			}
			if (!hs.empty())
				WaitForMultipleObjects(static_cast<DWORD>(hs.size()), hs.data(), TRUE,
					static_cast<DWORD>(kHttpTimeoutMs + 4000));
			for (HANDLE h : hs)
			{
				WaitForSingleObject(h, 1000);
				CloseHandle(h);
			}
		}

		for (Front& f : fronts)
		{
			if (!f.ok || f.ings.empty()) continue;
			f.node->crafted = true;
			const int deficit = f.node->need - f.node->have;
			const int crafts = (deficit + f.outCount - 1) / f.outCount;
			for (const RecipeIng& ri : f.ings)
			{
				if (!ri.name.empty())
					names[ri.itemId] = ri.name;
				IngNode kid;
				kid.itemId = ri.itemId;
				kid.need = ri.count * crafts;
				kid.depth = childDepth;
				kid.name = ri.name;
				kid.have = owned.count(ri.itemId) ? owned[ri.itemId] : 0;
				if (kid.name.empty() && names.count(ri.itemId))
					kid.name = names[ri.itemId];
				f.node->kids.push_back(std::move(kid));
			}
		}
	}

	void BuildTree(IngNode& node, int depth, std::unordered_map<int, int>& owned,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, RecipeCacheEntry>& cache)
	{
		node.have = owned.count(node.itemId) ? owned[node.itemId] : 0;
		if (names.count(node.itemId))
			node.name = names[node.itemId];
		if (depth >= kMaxDepth) return;
		if (node.have >= node.need) return;
		if (IsTerminalMaterial(node.name)) return;

		std::string nameHint = node.name;
		if (nameHint.empty() && names.count(node.itemId))
			nameHint = names[node.itemId];
		if (nameHint.empty())
			nameHint = ItemName(node.itemId);
		if (!nameHint.empty())
			names[node.itemId] = nameHint;

		int outCount = 1;
		std::vector<RecipeIng> ings;
		int recipeId = 0;
		if (!TryLoadRecipe(node.itemId, nameHint, outCount, ings, recipeId, cache, nullptr))
			return;
		node.crafted = true;
		const int deficit = node.need - node.have;
		const int crafts = (deficit + outCount - 1) / outCount;
		for (const RecipeIng& ri : ings)
		{
			IngNode kid;
			kid.itemId = ri.itemId;
			kid.need = ri.count * crafts;
			kid.depth = depth + 1;
			if (!ri.name.empty())
			{
				kid.name = ri.name;
				names[ri.itemId] = ri.name;
			}
			BuildTree(kid, depth + 1, owned, names, cache);
			node.kids.push_back(std::move(kid));
		}
	}

	void CollectLeafIds(const IngNode& n, std::vector<int>& ids)
	{
		if (n.kids.empty())
		{
			ids.push_back(n.itemId);
			return;
		}
		for (const IngNode& k : n.kids)
			CollectLeafIds(k, ids);
	}

	void ApplyPrices(IngNode& n, const std::unordered_map<int, long long>& sells,
		long long& total, int& noTpMissing)
	{
		if (n.kids.empty())
		{
			const int miss = (std::max)(0, n.need - n.have);
			if (miss <= 0) return;
			auto it = sells.find(n.itemId);
			if (it != sells.end())
			{
				n.buyUnit = it->second;
				total += n.buyUnit * miss;
			}
			else
			{
				n.buyUnit = -1;
				noTpMissing += miss;
			}
			return;
		}
		for (IngNode& k : n.kids)
			ApplyPrices(k, sells, total, noTpMissing);
	}
	void ApplyOwnedCounts(IngNode& n, const std::unordered_map<int, int>& owned)
	{
		auto it = owned.find(n.itemId);
		n.have = (it != owned.end()) ? it->second : 0;
		for (IngNode& k : n.kids)
			ApplyOwnedCounts(k, owned);
	}
} // namespace CraftingDetail
