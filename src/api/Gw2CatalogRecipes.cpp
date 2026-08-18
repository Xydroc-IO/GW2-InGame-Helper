#include "Gw2CatalogInternal.h"

using namespace Gw2CatalogDetail;

bool Gw2Catalog::RecipeById(int recipeId, Recipe* out)
{
	if (recipeId <= 0 || !out)
		return false;
	LoadDisk();
	std::lock_guard<std::mutex> lock(gMu);
	auto it = gRecipes.find(recipeId);
	if (it == gRecipes.end())
		return false;
	*out = it->second;
	return true;
}

bool Gw2Catalog::RecipeForOutput(int outputId, Recipe* out, const char* preferDiscipline)
{
	if (outputId <= 0 || !out)
		return false;
	LoadDisk();
	std::lock_guard<std::mutex> lock(gMu);
	auto it = gByOutput.find(outputId);
	if (it == gByOutput.end() || it->second.empty())
		return false;
	const Recipe* best = nullptr;
	bool bestPrefer = false;
	for (int rid : it->second)
	{
		auto rit = gRecipes.find(rid);
		if (rit == gRecipes.end())
			continue;
		const Recipe& rec = rit->second;
		const bool prefer = DiscMatch(rec.disciplines, preferDiscipline);
		const bool hasIngs = !rec.ings.empty();
		if (!best)
		{
			best = &rec;
			bestPrefer = prefer;
			continue;
		}
		const bool better = (prefer && !bestPrefer)
			|| (prefer == bestPrefer && rec.minRating < best->minRating)
			|| (prefer == bestPrefer && rec.minRating == best->minRating
				&& hasIngs && best->ings.empty());
		if (better)
		{
			best = &rec;
			bestPrefer = prefer;
		}
	}
	if (!best || best->ings.empty())
		return false;
	*out = *best;
	return true;
}

bool Gw2Catalog::RecipesForOutput(int outputId, std::vector<int>* ids)
{
	if (outputId <= 0 || !ids)
		return false;
	LoadDisk();
	std::lock_guard<std::mutex> lock(gMu);
	auto it = gByOutput.find(outputId);
	if (it == gByOutput.end() || it->second.empty())
		return false;
	*ids = it->second;
	return true;
}
