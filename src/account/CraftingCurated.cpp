#include "CraftingData.h"

#include "CraftingShared.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace CraftingDetail
{
	struct CuratedMat
	{
		int itemId;
		int need;
		const char* name;
	};

	struct CuratedBill
	{
		const char* source;
		const CuratedMat* mats;
		size_t matCount;
	};

	/* Eikasia (all three weight-class armory ids share one vendor bill). */
	static const CuratedMat kEikasiaMats[] = {
		{ 100512, 1, "Gift of Magical Prosperity" },
		{ 100933, 1, "Gift of Mighty Prosperity" },
		{ 19676, 200, "Icy Runestone" },
		{ 105196, 1, "Fractalline Spark" },
	};

	static const CuratedBill kEikasiaBill = {
		"Vendor / wiki acquisition",
		kEikasiaMats,
		sizeof(kEikasiaMats) / sizeof(kEikasiaMats[0]),
	};

	static const CuratedBill* FindCuratedBill(int outputId)
	{
		switch (outputId)
		{
		case 105317: /* Eikasia, Mists-Grasper */
		case 105171:
		case 105293:
			return &kEikasiaBill;
		default:
			return nullptr;
		}
	}

	bool LoadCuratedBill(int outputId, int& outCount, std::vector<RecipeIng>& ings,
		std::string* sourceOut)
	{
		const CuratedBill* bill = FindCuratedBill(outputId);
		if (!bill || !bill->mats || bill->matCount == 0)
			return false;
		ings.clear();
		ings.reserve(bill->matCount);
		for (size_t i = 0; i < bill->matCount; ++i)
		{
			RecipeIng ri;
			ri.itemId = bill->mats[i].itemId;
			ri.count = bill->mats[i].need;
			ri.name = bill->mats[i].name ? bill->mats[i].name : "";
			ings.push_back(std::move(ri));
		}
		outCount = 1;
		if (sourceOut)
			*sourceOut = bill->source ? bill->source : "Curated acquisition";
		return !ings.empty();
	}
} // namespace CraftingDetail
