#include "CraftingData.h"

#include "CraftingShared.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace CraftingDetail
{
	bool ExpandAndPricePlan(Plan& plan, int itemId, const std::string& name, int wantQty,
		unsigned gen, bool publishLive, bool honorPlanGen)
	{
		auto cancelled = [&]() {
			return honorPlanGen && gen != gPlanGen.load();
		};

		const int qty = wantQty < 1 ? 1 : wantQty;
		plan.outputId = itemId;
		plan.outputName = name;
		plan.wantQty = qty;
		plan.nameHints.clear();
		plan.status = "Fetching recipe...";
		if (publishLive) PublishLivePlan(plan);

		std::unordered_map<int, RecipeCacheEntry> recipeCache;
		int outCount = 1;
		std::vector<RecipeIng> ings;
		int recipeId = 0;
		std::string recipeSource;
		if (!TryLoadRecipe(itemId, plan.outputName, outCount, ings, recipeId,
				recipeCache, &recipeSource))
		{
			char buf[256];
			std::snprintf(buf, sizeof(buf),
				"Found %s (#%d) - no station, wiki forge, or acquisition bill.",
				plan.outputName.empty() ? "item" : plan.outputName.c_str(),
				itemId);
			plan.status = buf;
			plan.outputCount = outCount;
			return false;
		}
		if (cancelled()) return false;

		plan.outputCount = outCount;
		plan.recipeId = recipeId;
		plan.recipeSource = recipeSource;
		plan.recipeDiscipline = recipeSource;
		std::unordered_map<int, std::string> names;
		names[itemId] = plan.outputName;

		const int crafts = (qty + outCount - 1) / outCount;
		plan.root = {};
		plan.root.itemId = itemId;
		plan.root.need = qty;
		plan.root.have = 0;
		plan.root.name = plan.outputName;
		plan.root.crafted = true;
		plan.root.depth = 0;
		for (const RecipeIng& ri : ings)
		{
			if (!ri.name.empty())
				names[ri.itemId] = ri.name;
			IngNode kid;
			kid.itemId = ri.itemId;
			kid.need = ri.count * crafts;
			kid.depth = 1;
			kid.name = ri.name;
			kid.have = 0;
			plan.root.kids.push_back(std::move(kid));
		}
		plan.ok = true;
		plan.status = std::string(plan.recipeSource.empty() ? "Recipe" : plan.recipeSource)
			+ " | loading stash...";
		if (publishLive) PublishLivePlan(plan);

		std::unordered_map<int, int> owned;
		if (gOpts.useOwnMaterials)
			LoadOwned(owned);
		if (cancelled()) return false;
		ApplyOwnedCounts(plan.root, owned);
		plan.status = std::string(plan.recipeSource.empty() ? "Recipe" : plan.recipeSource)
			+ " | expanding gifts...";
		if (publishLive) PublishLivePlan(plan);

		for (int depth = 2; depth <= kMaxDepth; ++depth)
		{
			if (cancelled()) return false;
			char st[96];
			std::snprintf(st, sizeof(st), "Expanding gifts (depth %d/%d)...",
				depth, kMaxDepth);
			plan.status = st;
			if (publishLive) PublishLivePlan(plan);
			ExpandFrontier(plan, owned, names, recipeCache, depth);
			ApplyOwnedCounts(plan.root, owned);
			if (publishLive) PublishLivePlan(plan);
		}

		if (cancelled()) return false;
		plan.status = "Pricing materials...";
		if (publishLive) PublishLivePlan(plan);
		FinishPrices(plan, names);
		if (publishLive) PublishLivePlan(plan);
		return plan.ok;
	}

} // namespace CraftingDetail
