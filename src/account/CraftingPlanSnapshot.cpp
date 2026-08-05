#include "CraftingPlanSnapshot.h"

#include "CraftingShared.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace CraftingPlanSnapshot
{
	using namespace CraftingDetail;

	namespace
	{
		void AccumulateLeaves(const IngNode& n, int& needSum, int& haveSum)
		{
			if (n.kids.empty())
			{
				const int need = (std::max)(0, n.need);
				needSum += need;
				haveSum += (std::min)(n.have, need);
				return;
			}
			for (const IngNode& k : n.kids)
				AccumulateLeaves(k, needSum, haveSum);
		}

		void AppendLeafJson(const IngNode& n, std::string& json, bool& first)
		{
			if (n.kids.empty())
			{
				if (!first)
					json += ',';
				first = false;
				char buf[48];
				std::snprintf(buf, sizeof(buf), "{\"id\":%d,\"need\":%d,\"have\":%d,\"name\":\"",
					n.itemId, n.need, n.have);
				json += buf;
				for (char c : n.name)
				{
					if (c == '"' || c == '\\')
						json += '\\';
					if (static_cast<unsigned char>(c) < 32)
						continue;
					json += c;
				}
				json += "\"}";
				return;
			}
			for (const IngNode& k : n.kids)
				AppendLeafJson(k, json, first);
		}

		void AppendNodeHtml(const IngNode& n, std::string& html)
		{
			const int miss = (std::max)(0, n.need - n.have);
			const bool done = miss <= 0;
			html += "<li class=\"";
			html += done ? "done" : (n.crafted ? "craft" : "mat");
			html += "\"><span class=\"row\"><span class=\"nm\">";
			for (char c : n.name.empty() ? std::to_string(n.itemId) : n.name)
			{
				if (c == '<')
					html += "&lt;";
				else if (c == '>')
					html += "&gt;";
				else if (c == '&')
					html += "&amp;";
				else
					html += c;
			}
			html += "</span> <span class=\"qty\">";
			char q[64];
			std::snprintf(q, sizeof(q), "%d / %d", n.have, n.need);
			html += q;
			html += "</span></span>";
			if (!n.kids.empty())
			{
				html += "<ul>";
				for (const IngNode& k : n.kids)
					AppendNodeHtml(k, html);
				html += "</ul>";
			}
			html += "</li>";
		}
	} // namespace

	Progress Build(int itemId)
	{
		Progress out;
		out.outputId = itemId;
		if (itemId <= 0)
		{
			out.status = "Invalid item id.";
			return out;
		}

		out.outputName = ItemName(itemId);
		if (out.outputName.empty())
		{
			out.status = "Could not resolve item name.";
			return out;
		}

		std::unordered_map<int, RecipeCacheEntry> recipeCache;
		int outCount = 1;
		std::vector<RecipeIng> ings;
		int recipeId = 0;
		std::string recipeSource;
		if (!TryLoadRecipe(itemId, out.outputName, outCount, ings, recipeId, recipeCache,
				&recipeSource))
		{
			char buf[256];
			std::snprintf(buf, sizeof(buf),
				"No station, wiki forge, or acquisition bill for %s (#%d).",
				out.outputName.c_str(), itemId);
			out.status = buf;
			return out;
		}

		Plan plan;
		plan.ok = true;
		plan.outputId = itemId;
		plan.outputName = out.outputName;
		plan.outputCount = outCount;
		plan.recipeSource = recipeSource;
		plan.root = {};
		plan.root.itemId = itemId;
		plan.root.need = 1;
		plan.root.have = 0;
		plan.root.name = out.outputName;
		plan.root.crafted = true;
		plan.root.depth = 0;

		std::unordered_map<int, std::string> names;
		names[itemId] = out.outputName;
		for (const RecipeIng& ri : ings)
		{
			if (!ri.name.empty())
				names[ri.itemId] = ri.name;
			IngNode kid;
			kid.itemId = ri.itemId;
			kid.need = ri.count;
			kid.depth = 1;
			kid.name = ri.name;
			plan.root.kids.push_back(std::move(kid));
		}

		std::unordered_map<int, int> owned;
		LoadOwned(owned);
		ApplyOwnedCounts(plan.root, owned);

		for (int depth = 2; depth <= kMaxDepth; ++depth)
			ExpandFrontier(plan, owned, names, recipeCache, depth);

		ApplyOwnedCounts(plan.root, owned);

		/* Wiki/API recipes often omit ingredient names — bulk-resolve like FinishPrices. */
		{
			std::vector<int> allIds;
			allIds.push_back(plan.outputId);
			std::vector<IngNode*> stack;
			stack.push_back(&plan.root);
			while (!stack.empty())
			{
				IngNode* cur = stack.back();
				stack.pop_back();
				allIds.push_back(cur->itemId);
				for (IngNode& c : cur->kids)
					stack.push_back(&c);
			}
			std::sort(allIds.begin(), allIds.end());
			allIds.erase(std::unique(allIds.begin(), allIds.end()), allIds.end());
			FetchNames(names, allIds);
			stack.push_back(&plan.root);
			while (!stack.empty())
			{
				IngNode* cur = stack.back();
				stack.pop_back();
				auto it = names.find(cur->itemId);
				if (it != names.end() && !it->second.empty())
					cur->name = it->second;
				for (IngNode& c : cur->kids)
					stack.push_back(&c);
			}
			if (names.count(itemId) && !names[itemId].empty())
				plan.outputName = names[itemId];
		}

		out.ok = true;
		out.recipeSource = recipeSource.empty() ? "Recipe" : recipeSource;
		out.outputName = plan.outputName;
		AccumulateLeaves(plan.root, out.leafNeed, out.leafHave);
		if (out.leafNeed > 0)
			out.pct = (out.leafHave * 100) / out.leafNeed;
		else
			out.pct = 100;

		out.treeHtml = "<ul class=\"craft-tree\">";
		for (const IngNode& k : plan.root.kids)
			AppendNodeHtml(k, out.treeHtml);
		out.treeHtml += "</ul>";

		out.leavesJson = "[";
		bool first = true;
		for (const IngNode& k : plan.root.kids)
			AppendLeafJson(k, out.leavesJson, first);
		out.leavesJson += "]";

		char st[160];
		std::snprintf(st, sizeof(st), "%s · %d%% mats (%d / %d on leaves)",
			out.recipeSource.c_str(), out.pct, out.leafHave, out.leafNeed);
		out.status = st;
		return out;
	}
} // namespace CraftingPlanSnapshot
