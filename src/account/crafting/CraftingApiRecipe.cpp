#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Catalog.h"
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
		{
			Gw2Catalog::Recipe rec;
			if (Gw2Catalog::RecipeById(recipeId, &rec) && rec.outputId > 0 && !rec.ings.empty())
			{
				outputId = rec.outputId;
				outCount = rec.outputCount > 0 ? rec.outputCount : 1;
				ings.clear();
				for (const auto& p : rec.ings)
				{
					RecipeIng ri;
					ri.itemId = p.first;
					ri.count = p.second;
					ings.push_back(ri);
				}
				if (disciplineOut)
				{
					const size_t bar = rec.disciplines.find('|');
					*disciplineOut = bar == std::string::npos ? rec.disciplines
						: rec.disciplines.substr(0, bar);
				}
				return true;
			}
		}
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
		{
			Gw2Catalog::Recipe rec;
			if (Gw2Catalog::RecipeForOutput(outputId, &rec, preferDiscipline) && !rec.ings.empty())
			{
				recipeId = rec.recipeId;
				outCount = rec.outputCount > 0 ? rec.outputCount : 1;
				ings.clear();
				for (const auto& p : rec.ings)
				{
					RecipeIng ri;
					ri.itemId = p.first;
					ri.count = p.second;
					ings.push_back(ri);
				}
				if (disciplineOut)
				{
					const size_t bar = rec.disciplines.find('|');
					*disciplineOut = bar == std::string::npos ? rec.disciplines
						: rec.disciplines.substr(0, bar);
				}
				return true;
			}
		}
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
} // namespace CraftingDetail
