#pragma once

#include <string>

/* Worker-safe craft tree snapshot for Live panels / shared Progress path. */
namespace CraftingPlanSnapshot
{
	struct Progress
	{
		bool ok = false;
		int outputId = 0;
		std::string outputName;
		std::string recipeSource;
		std::string status;
		int leafNeed = 0;   /* sum of leaf need */
		int leafHave = 0;   /* sum of min(have, need) on leaves */
		int pct = 0;        /* 0–100 */
		std::string treeHtml; /* nested <ul> rows */
		std::string leavesJson; /* [{id,name,need,have},…] for list caches */
	};

	/* Expand station/wiki/curated recipe to depth, apply inventory/bank/shared owned. */
	Progress Build(int itemId);
}
