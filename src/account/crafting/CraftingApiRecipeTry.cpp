#include "CraftingData.h"

#include "CraftingShared.h"

#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace CraftingDetail
{
	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	bool IsTerminalMaterial(const std::string& name)
	{
		if (name.empty()) return false;
		const std::string n = ToLowerCopy(name);
		if (n.rfind("gift of ", 0) == 0) return false;
		if (n.rfind("recipe:", 0) == 0) return false;
		if (n.find("legendary") != std::string::npos) return false;
		auto has = [&](const char* s) {
			return n.find(s) != std::string::npos;
		};
		if (has(" ore") || has(" log") || has(" scrap")) return true;
		if (has("pile of ") || has("glob of ectoplasm") || has("obsidian shard")) return true;
		if (has(" dust")) return true;
		if (has("vial of ") || has("venom sac")) return true;
		if (has("blood") && !has("gift")) return true;
		if (has("fang") || has("claw") || has("scale") || has("bone") ||
			has("totem"))
			return true;
		if (has("crystal") && !has("gift")) return true;
		if (has("lodestone") || has("mystic coin") || has("philosopher") ||
			has("icy runestone") || has("spirit shard") || has("karma") ||
			has("tale of dungeon") || has("badge of honor"))
			return true;
		if (has("quartz crystal") || has("thermocatalytic") || has("spool of ") ||
			has("lump of ") || has("glob of elder"))
			return true;
		return false;
	}

	bool PreferWikiRecipe(const std::string& nameHint)
	{
		if (nameHint.empty()) return false;
		const std::string n = ToLowerCopy(nameHint);
		if (n.rfind("gift of ", 0) == 0) return true;
		if (n.find("tribute") != std::string::npos) return true;
		if (n.find("legendary") != std::string::npos) return true;
		if (n.find("aurene") != std::string::npos) return true;
		if (n.find("draconic") != std::string::npos) return true;
		return false;
	}

	bool TryLoadRecipe(int outputId, const std::string& nameHint, int& outCount,
		std::vector<RecipeIng>& ings, int& recipeId,
		std::unordered_map<int, RecipeCacheEntry>& cache, std::string* sourceOut)
	{
		std::unique_lock<std::mutex> cacheLock(gRecipeCacheMu);
		RecipeCacheEntry& e = cache[outputId];
		if (e.ok)
		{
			outCount = e.outCount;
			ings = e.ings;
			recipeId = e.recipeId;
			if (sourceOut) *sourceOut = e.source;
			return true;
		}

		auto tryWiki = [&]() -> bool {
			RecipeCacheEntry& ew = cache[outputId];
			if (ew.wikiTried || nameHint.empty())
				return false;
			ew.wikiTried = true;
			if (IsTerminalMaterial(nameHint))
				return false;
			std::string src;
			cacheLock.unlock();
			const bool wikiOk = LoadWikiRecipeForName(nameHint.c_str(), outCount, ings, &src);
			cacheLock.lock();
			RecipeCacheEntry& eWiki = cache[outputId];
			if (!wikiOk)
				return false;
			eWiki.ok = true;
			eWiki.outCount = outCount;
			eWiki.recipeId = 0;
			eWiki.ings = ings;
			eWiki.source = src.empty() ? "Wiki recipe" : src;
			if (sourceOut) *sourceOut = eWiki.source;
			recipeId = 0;
			return true;
		};

		const bool wikiFirst = PreferWikiRecipe(nameHint);
		if (wikiFirst && tryWiki())
			return true;

		if (!e.apiTried)
		{
			e.apiTried = true;
			std::string disc;
			cacheLock.unlock();
			const bool apiOk = LoadApiRecipeForOutput(outputId, outCount, ings, recipeId, &disc);
			cacheLock.lock();
			RecipeCacheEntry& eApi = cache[outputId];
			if (apiOk)
			{
				eApi.ok = true;
				eApi.outCount = outCount;
				eApi.recipeId = recipeId;
				eApi.ings = ings;
				eApi.discipline = disc;
				eApi.source = disc.empty() ? "Crafting station" : disc;
				if (sourceOut) *sourceOut = eApi.source;
				return true;
			}
		}

		if (!wikiFirst && tryWiki())
			return true;

		RecipeCacheEntry& eCurGate = cache[outputId];
		if (!eCurGate.curatedTried)
		{
			eCurGate.curatedTried = true;
			std::string src;
			cacheLock.unlock();
			const bool curOk = LoadCuratedBill(outputId, outCount, ings, &src);
			cacheLock.lock();
			RecipeCacheEntry& eCur = cache[outputId];
			if (curOk)
			{
				eCur.ok = true;
				eCur.outCount = outCount;
				eCur.recipeId = 0;
				eCur.ings = ings;
				eCur.source = src.empty() ? "Curated acquisition" : src;
				if (sourceOut) *sourceOut = eCur.source;
				return true;
			}
		}

		RecipeCacheEntry& eAcqGate = cache[outputId];
		if (!eAcqGate.acquireTried && !nameHint.empty() && !IsTerminalMaterial(nameHint))
		{
			eAcqGate.acquireTried = true;
			std::string src;
			cacheLock.unlock();
			const bool acqOk = LoadWikiAcquisitionBill(nameHint.c_str(), outCount, ings, &src);
			cacheLock.lock();
			RecipeCacheEntry& eAcq = cache[outputId];
			if (acqOk)
			{
				eAcq.ok = true;
				eAcq.outCount = outCount;
				eAcq.recipeId = 0;
				eAcq.ings = ings;
				eAcq.source = src.empty() ? "Vendor / wiki acquisition" : src;
				if (sourceOut) *sourceOut = eAcq.source;
				return true;
			}
		}

		RecipeCacheEntry& eEnd = cache[outputId];
		outCount = eEnd.outCount;
		ings = eEnd.ings;
		recipeId = eEnd.recipeId;
		return false;
	}
}
