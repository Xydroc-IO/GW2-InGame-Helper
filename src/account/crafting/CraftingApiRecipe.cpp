#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "InventoryData.h"

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
	void AddOwnedCounts(std::unordered_map<int, int>& owned, const std::string& body)
	{
		size_t p = 0;
		while (p < body.size())
		{
			size_t brace = body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(body, "id", brace);
			long long cnt = JsonIntAfterKey(body, "count", brace);
			if (id > 0 && cnt > 0)
				owned[static_cast<int>(id)] += static_cast<int>(cnt);
			p = end + 1;
		}
	}

	struct OwnedJob
	{
		const char* path = nullptr;
		const char* key = nullptr;
		Gw2Http::Result result;
	};

	DWORD WINAPI OwnedProc(void* param)
	{
		auto* j = static_cast<OwnedJob*>(param);
		j->result = Gw2Http::Api(j->path, j->key, kHttpTimeoutMs);
		return 0;
	}

	void LoadOwned(std::unordered_map<int, int>& owned)
	{
		InventoryData::Tick();
		if (!InventoryData::Ready())
		{
			InventoryData::RefreshIfNeeded(false);
			/* Fall back to a quick materials/bank/shared pull if inventory module
			   is still cold - same endpoints, keeps planner usable. */
			if (!G::Gw2ApiKey[0]) return;
			const char* key = G::Gw2ApiKey;
			OwnedJob jobs[3] = {
				{ "/v2/account/materials", key, {} },
				{ "/v2/account/bank", key, {} },
				{ "/v2/account/inventory", key, {} },
			};
			HANDLE hs[3]{};
			for (int i = 0; i < 3; ++i)
				hs[i] = CreateThread(nullptr, 0, OwnedProc, &jobs[i], 0, nullptr);
			if (hs[0] && hs[1] && hs[2])
				WaitForMultipleObjects(3, hs, TRUE, 12000);
			for (int i = 0; i < 3; ++i)
			{
				if (hs[i])
				{
					WaitForSingleObject(hs[i], 0);
					CloseHandle(hs[i]);
				}
				if (jobs[i].result.ok)
					AddOwnedCounts(owned, jobs[i].result.body);
			}
			return;
		}
		InventoryData::FillOwnedMap(owned);
	}

	bool LoadApiRecipeById(int recipeId, int& outputId, int& outCount, std::vector<RecipeIng>& ings,
		std::string* disciplineOut)
	{
		if (recipeId <= 0) return false;
		char path[64];
		std::snprintf(path, sizeof(path), "/v2/recipes/%d", recipeId);
		auto rr = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (!rr.ok) return false;
		outputId = static_cast<int>(JsonIntAfterKey(rr.body, "output_item_id", 0));
		outCount = static_cast<int>(JsonIntAfterKey(rr.body, "output_item_count", 0));
		if (outCount <= 0) outCount = 1;
		if (disciplineOut)
		{
			*disciplineOut = {};
			size_t dkey = rr.body.find("\"disciplines\"");
			if (dkey != std::string::npos)
			{
				size_t bracket = rr.body.find('[', dkey);
				size_t q1 = (bracket == std::string::npos) ? std::string::npos : rr.body.find('"', bracket);
				if (q1 != std::string::npos)
				{
					++q1;
					size_t q2 = rr.body.find('"', q1);
					if (q2 != std::string::npos)
						*disciplineOut = rr.body.substr(q1, q2 - q1);
				}
			}
		}
		ings.clear();
		size_t ingKey = rr.body.find("\"ingredients\"");
		if (ingKey == std::string::npos) return outputId > 0;
		size_t arr = rr.body.find('[', ingKey);
		if (arr == std::string::npos) return outputId > 0;
		size_t p = arr;
		while (p < rr.body.size())
		{
			size_t brace = rr.body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(rr.body, brace);
			if (end == std::string::npos) break;
			long long iid = JsonIntAfterKey(rr.body, "item_id", brace);
			long long cnt = JsonIntAfterKey(rr.body, "count", brace);
			if (iid > 0 && cnt > 0)
			{
				RecipeIng ri;
				ri.itemId = static_cast<int>(iid);
				ri.count = static_cast<int>(cnt);
				ings.push_back(ri);
			}
			p = end + 1;
			const size_t nextBrace = rr.body.find('{', p);
			const size_t nextClose = rr.body.find(']', p);
			if (nextClose != std::string::npos &&
				(nextBrace == std::string::npos || nextClose < nextBrace))
				break;
		}
		return outputId > 0 && !ings.empty();
	}

	bool LoadApiRecipeForOutput(int outputId, int& outCount, std::vector<RecipeIng>& ings, int& recipeId,
		std::string* disciplineOut, const char* preferDiscipline)
	{
		char path[80];
		std::snprintf(path, sizeof(path), "/v2/recipes/search?output=%d", outputId);
		auto sr = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (!sr.ok || sr.body.empty()) return false;

		std::vector<int> candidates;
		ParseIntArray(sr.body, candidates);
		if (candidates.empty())
		{
			/* Fallback: scan digits if body is a bare id list we failed to parse. */
			size_t p = 0;
			while (p < sr.body.size() && candidates.size() < 10)
			{
				while (p < sr.body.size() && (sr.body[p] < '0' || sr.body[p] > '9')) ++p;
				if (p >= sr.body.size()) break;
				int v = 0;
				while (p < sr.body.size() && sr.body[p] >= '0' && sr.body[p] <= '9')
				{
					v = v * 10 + (sr.body[p] - '0');
					++p;
				}
				if (v > 0) candidates.push_back(v);
			}
		}
		if (candidates.size() > 10)
			candidates.resize(10);
		if (candidates.empty()) return false;

		struct Cand
		{
			int id = 0;
			int minRating = 999999;
			bool preferMatch = false;
			bool hasIngs = false;
			int unusedOut = 0;
			int outCnt = 1;
			std::vector<RecipeIng> ings;
			std::string disc;
		};

		Cand best{};
		bool haveBest = false;
		const bool wantPrefer = preferDiscipline && preferDiscipline[0];

		for (int rid : candidates)
		{
			if (rid <= 0) continue;
			char rpath[64];
			std::snprintf(rpath, sizeof(rpath), "/v2/recipes/%d", rid);
			auto rr = Gw2Http::Api(rpath, nullptr, kHttpTimeoutMs);
			if (!rr.ok) continue;

			Cand c;
			c.id = rid;
			c.minRating = static_cast<int>(JsonIntAfterKey(rr.body, "min_rating", 0));
			if (c.minRating < 0) c.minRating = 0;
			c.unusedOut = static_cast<int>(JsonIntAfterKey(rr.body, "output_item_id", 0));
			c.outCnt = static_cast<int>(JsonIntAfterKey(rr.body, "output_item_count", 0));
			if (c.outCnt <= 0) c.outCnt = 1;

			size_t dkey = rr.body.find("\"disciplines\"");
			if (dkey != std::string::npos)
			{
				size_t bracket = rr.body.find('[', dkey);
				size_t close = (bracket == std::string::npos) ? std::string::npos
					: rr.body.find(']', bracket);
				if (bracket != std::string::npos && close != std::string::npos)
				{
					const std::string arr = rr.body.substr(bracket, close - bracket + 1);
					size_t q1 = arr.find('"');
					if (q1 != std::string::npos)
					{
						++q1;
						size_t q2 = arr.find('"', q1);
						if (q2 != std::string::npos)
							c.disc = arr.substr(q1, q2 - q1);
					}
					if (wantPrefer)
					{
						const std::string needle = std::string("\"") + preferDiscipline + "\"";
						c.preferMatch = arr.find(needle) != std::string::npos;
					}
				}
			}

			c.ings.clear();
			size_t ingKey = rr.body.find("\"ingredients\"");
			if (ingKey != std::string::npos)
			{
				size_t arr = rr.body.find('[', ingKey);
				if (arr != std::string::npos)
				{
					size_t p = arr;
					while (p < rr.body.size())
					{
						size_t brace = rr.body.find('{', p);
						if (brace == std::string::npos) break;
						size_t end = JsonObjectEnd(rr.body, brace);
						if (end == std::string::npos) break;
						long long iid = JsonIntAfterKey(rr.body, "item_id", brace);
						long long cnt = JsonIntAfterKey(rr.body, "count", brace);
						if (iid > 0 && cnt > 0)
						{
							RecipeIng ri;
							ri.itemId = static_cast<int>(iid);
							ri.count = static_cast<int>(cnt);
							c.ings.push_back(ri);
						}
						p = end + 1;
						const size_t nextBrace = rr.body.find('{', p);
						const size_t nextClose = rr.body.find(']', p);
						if (nextClose != std::string::npos &&
							(nextBrace == std::string::npos || nextClose < nextBrace))
							break;
					}
				}
			}
			c.hasIngs = !c.ings.empty();
			if (c.unusedOut <= 0) continue;

			const bool better = !haveBest
				|| (c.preferMatch && !best.preferMatch)
				|| (c.preferMatch == best.preferMatch && c.minRating < best.minRating)
				|| (c.preferMatch == best.preferMatch && c.minRating == best.minRating
					&& c.hasIngs && !best.hasIngs);
			if (better)
			{
				best = std::move(c);
				haveBest = true;
			}
		}

		if (!haveBest)
		{
			recipeId = candidates[0];
			int unusedOut = 0;
			return LoadApiRecipeById(recipeId, unusedOut, outCount, ings, disciplineOut);
		}

		recipeId = best.id;
		outCount = best.outCnt;
		ings = std::move(best.ings);
		if (disciplineOut)
			*disciplineOut = best.disc;
		return best.unusedOut > 0 && !ings.empty();
	}

	/* TP / gather mats - buy these; do not chase promotion ladders (ore/dust/T6 blood...). */
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
		if (has("blood") && !has("gift")) return true; /* T5/T6 blood, bloodstone */
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

	/* Gifts / tributes almost never have station recipes - skip failed API search. */
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

	/* Station API first (or wiki-first for gifts); never wiki-expand raw mats. */
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
	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

} // namespace CraftingDetail
