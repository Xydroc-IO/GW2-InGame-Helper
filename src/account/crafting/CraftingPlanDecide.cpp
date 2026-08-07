#include "CraftingData.h"

#include "CraftingShared.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace CraftingDetail
{
	static long long SubtreeCraftCost(const IngNode& n)
	{
		if (n.kids.empty())
		{
			const int miss = (std::max)(0, n.need - n.have);
			if (miss <= 0) return 0;
			if (n.buyUnit < 0) return -1;
			return n.buyUnit * miss;
		}
		long long sum = 0;
		for (const IngNode& k : n.kids)
		{
			const long long c = SubtreeCraftCost(k);
			if (c < 0) return -1;
			sum += c;
		}
		return sum;
	}

	static void CollapseNode(IngNode& n, bool isRoot,
		const std::unordered_map<int, long long>& sells, const PlanOpts& opts)
	{
		for (IngNode& k : n.kids)
			CollapseNode(k, false, sells, opts);

		if (n.kids.empty())
		{
			n.crafted = false;
			return;
		}

		if (isRoot)
		{
			n.crafted = true;
			return;
		}

		long long buy = -1;
		auto it = sells.find(n.itemId);
		if (it != sells.end())
			buy = it->second;

		const int miss = (std::max)(0, n.need - n.have);
		const long long buyCost = (buy >= 0) ? buy * (std::max)(1, miss > 0 ? miss : n.need) : -1;
		const long long craft = SubtreeCraftCost(n);
		bool doCraft = false;
		if (opts.craftSubComponents)
			doCraft = (buyCost < 0) || (craft >= 0 && craft < buyCost);
		else
			doCraft = (buyCost < 0); /* buy when possible */

		if (doCraft)
		{
			n.crafted = true;
		}
		else
		{
			n.kids.clear();
			n.crafted = false;
			if (buy >= 0)
				n.buyUnit = buy;
		}
	}

	void ApplyBuyVsCraft(Plan& plan, const std::unordered_map<int, long long>& sells,
		const PlanOpts& opts)
	{
		if (!plan.ok) return;
		CollapseNode(plan.root, true, sells, opts);
	}

	static void CollectStepsRec(const IngNode& n, std::vector<StepRow>& steps)
	{
		for (const IngNode& k : n.kids)
			CollectStepsRec(k, steps);
		if (!n.crafted || n.kids.empty()) return;

		StepRow st;
		st.outId = n.itemId;
		st.name = n.name;
		st.depth = n.depth;
		st.outCnt = 1;
		/* crafts ≈ need (each craft yields ~1 for wiki trees; station outCnt applied at root) */
		st.crafts = (std::max)(1, n.need);
		for (const IngNode& k : n.kids)
		{
			RecipeIng ri;
			ri.itemId = k.itemId;
			ri.count = k.need;
			ri.name = k.name;
			st.ings.push_back(ri);
		}
		steps.push_back(st);
	}

	static void CollectShopRec(const IngNode& n, std::unordered_map<int, ShopRow>& agg)
	{
		if (n.kids.empty())
		{
			const int miss = (std::max)(0, n.need - n.have);
			if (miss <= 0) return;
			ShopRow& row = agg[n.itemId];
			row.itemId = n.itemId;
			row.name = n.name;
			row.qty += miss;
			row.unitSell = n.buyUnit;
			if (n.buyUnit >= 0)
			{
				row.priced = true;
				row.total = (row.total < 0 ? 0 : row.total) + n.buyUnit * miss;
			}
			return;
		}
		for (const IngNode& k : n.kids)
			CollectShopRec(k, agg);
	}

	void BuildShoppingAndSteps(Plan& plan)
	{
		plan.shopping.clear();
		plan.steps.clear();
		if (!plan.ok) return;

		std::unordered_map<int, ShopRow> shopAgg;
		for (const IngNode& k : plan.root.kids)
			CollectShopRec(k, shopAgg);
		plan.shopping.reserve(shopAgg.size());
		for (auto& kv : shopAgg)
			plan.shopping.push_back(std::move(kv.second));
		std::sort(plan.shopping.begin(), plan.shopping.end(),
			[](const ShopRow& a, const ShopRow& b) { return a.total > b.total; });

		CollectStepsRec(plan.root, plan.steps);
		/* deepest-first already from post-order; reverse for root-last build order display:
		   Tyrian shows deepest first — CollectStepsRec already emits deep before parents. */

		if (!plan.root.kids.empty() && plan.root.crafted)
		{
			/* ensure root craft is last step if not already */
			bool hasRoot = false;
			for (const StepRow& s : plan.steps)
				if (s.outId == plan.root.itemId) { hasRoot = true; break; }
			if (!hasRoot)
			{
				StepRow st;
				st.outId = plan.root.itemId;
				st.name = plan.root.name;
				st.depth = 0;
				st.crafts = 1;
				st.outCnt = plan.outputCount;
				st.disc = plan.recipeDiscipline.empty() ? plan.recipeSource : plan.recipeDiscipline;
				for (const IngNode& k : plan.root.kids)
				{
					RecipeIng ri;
					ri.itemId = k.itemId;
					ri.count = k.need;
					ri.name = k.name;
					st.ings.push_back(ri);
				}
				plan.steps.push_back(st);
			}
		}

		for (StepRow& s : plan.steps)
		{
			if (s.disc.empty())
			{
				if (s.outId == plan.outputId && !plan.recipeDiscipline.empty())
					s.disc = plan.recipeDiscipline;
				else if (s.outId == plan.outputId)
					s.disc = plan.recipeSource;
				else
					s.disc = "Craft";
			}
		}
	}

} // namespace CraftingDetail
