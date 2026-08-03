#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "InventoryData.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
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

		/* Cap parallelism — fewer threads, less crash surface under WinHTTP. */
		constexpr size_t kMaxParallel = 4;
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
				WaitForMultipleObjects(static_cast<DWORD>(hs.size()), hs.data(), TRUE, 60000);
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
	void TokenizeQuery(const char* q, std::vector<std::string>& tokens)
	{
		tokens.clear();
		std::string cur;
		auto flush = [&]() {
			if (cur.size() < 2) { cur.clear(); return; }
			const std::string low = ToLowerCopy(cur);
			if (low == "of" || low == "the" || low == "a" || low == "an" ||
				low == "and" || low == "with" || low == "for")
			{
				cur.clear();
				return;
			}
			tokens.push_back(low);
			cur.clear();
		};
		for (const char* p = q; *p; ++p)
		{
			if (std::isalnum(static_cast<unsigned char>(*p)) || *p == '\'')
				cur.push_back(*p);
			else
				flush();
		}
		flush();
	}

	/* Higher = closer title match. Reject weak wiki noise (e.g. "bow" → Emblem…). */
	int ScoreTitleMatch(const std::string& title, const std::vector<std::string>& tokens,
		const std::string& qLow)
	{
		const std::string tLow = ToLowerCopy(title);
		if (tLow == qLow) return 1000;
		if (!qLow.empty() && tLow.find(qLow) != std::string::npos) return 800;
		if (tokens.empty()) return 0;
		int matched = 0;
		int score = 0;
		for (const std::string& tok : tokens)
		{
			if (tLow.find(tok) != std::string::npos)
			{
				++matched;
				score += 10 + static_cast<int>(tok.size()); /* longer tokens weigh more */
			}
		}
		/* Require most of the meaningful words, not just "of"/one lucky hit. */
		const int need = (tokens.size() <= 2) ? static_cast<int>(tokens.size())
			: (static_cast<int>(tokens.size()) * 2 + 2) / 3; /* ceil(2/3) */
		if (matched < need) return 0;
		if (matched == static_cast<int>(tokens.size())) score += 50;
		return score;
	}

	void WikiSearchTitles(const char* query, std::vector<std::string>& titles, size_t maxN)
	{
		std::string url =
			"https://wiki.guildwars2.com/api.php?action=query&list=search&srnamespace=0"
			"&srlimit=8&format=json&formatversion=2&srsearch=";
		url += UrlEncode(query);
		auto wr = Gw2Http::Get(url.c_str(), nullptr, kHttpTimeoutMs);
		if (!wr.ok) return;
		size_t p = 0;
		while (titles.size() < maxN && p < wr.body.size())
		{
			size_t t = wr.body.find("\"title\"", p);
			if (t == std::string::npos) break;
			std::string title = JsonStringAfterKey(wr.body, "title", t);
			p = t + 7;
			if (title.empty()) continue;
			bool dup = false;
			for (const auto& h : titles)
				if (h == title) { dup = true; break; }
			if (!dup) titles.push_back(title);
		}
	}

	/* Common food typo: "bow of …" → "bowl of …". */
	std::string TypoHintsQuery(const char* q)
	{
		std::string s = q ? q : "";
		std::string low = ToLowerCopy(s);
		/* whole-word bow → bowl (not already bowl) */
		std::string out;
		size_t i = 0;
		bool changed = false;
		while (i < low.size())
		{
			if ((i == 0 || !std::isalnum(static_cast<unsigned char>(low[i - 1]))) &&
				i + 3 <= low.size() && low.compare(i, 3, "bow") == 0 &&
				(i + 3 == low.size() || !std::isalnum(static_cast<unsigned char>(low[i + 3]))) &&
				!(i + 4 <= low.size() && low.compare(i, 4, "bowl") == 0))
			{
				out += "bowl";
				/* copy casing-agnostic replacement into original-length stream from s */
				i += 3;
				changed = true;
				continue;
			}
			out.push_back(s[i]);
			++i;
		}
		return changed ? out : std::string{};
	}

	int ResolveQueryToItemId(const char* q, Plan& plan, std::string* nameOut)
	{
		const int id = ParseItemId(q);
		if (id > 0)
		{
			std::string name = ItemName(id);
			if (!name.empty())
			{
				if (nameOut) *nameOut = name;
				return id;
			}
			return 0;
		}

		std::vector<std::string> tokens;
		TokenizeQuery(q, tokens);
		const std::string qLow = ToLowerCopy(q ? q : "");

		std::vector<std::string> titles;
		WikiSearchTitles(q, titles, 8);

		auto bestScoreAmong = [&](const std::vector<std::string>& list, const char* queryStr) {
			std::vector<std::string> toks;
			TokenizeQuery(queryStr, toks);
			const std::string ql = ToLowerCopy(queryStr);
			int best = 0;
			for (const std::string& title : list)
			{
				const int s = ScoreTitleMatch(title, toks, ql);
				if (s > best) best = s;
			}
			return best;
		};

		/* Skip second wiki round-trip when the first search already looks solid. */
		std::string typo;
		if (bestScoreAmong(titles, q) < 80)
		{
			typo = TypoHintsQuery(q);
			if (!typo.empty())
				WikiSearchTitles(typo.c_str(), titles, 12);
		}

		struct Ranked { std::string title; int score = 0; };
		std::vector<Ranked> ranked;
		ranked.reserve(titles.size());
		std::vector<std::string> typoTok;
		if (!typo.empty())
			TokenizeQuery(typo.c_str(), typoTok);
		const std::string typoLow = typo.empty() ? std::string{} : ToLowerCopy(typo);
		for (const std::string& title : titles)
		{
			Ranked r;
			r.title = title;
			r.score = ScoreTitleMatch(title, tokens, qLow);
			if (!typo.empty())
			{
				const int s2 = ScoreTitleMatch(title, typoTok, typoLow);
				if (s2 > r.score) r.score = s2;
			}
			ranked.push_back(std::move(r));
		}
		std::sort(ranked.begin(), ranked.end(),
			[](const Ranked& a, const Ranked& b) { return a.score > b.score; });

		plan.nameHints.clear();
		for (const Ranked& r : ranked)
		{
			if (plan.nameHints.size() >= 8) break;
			plan.nameHints.push_back(r.title);
		}

		/* Auto-resolve only a confident title match — never first-hit roulette. */
		constexpr int kMinAutoScore = 30;
		for (const Ranked& r : ranked)
		{
			if (r.score < kMinAutoScore) break;
			const int resolved = ResolveWikiTitleToItemId(r.title.c_str(), nameOut);
			if (resolved > 0)
			{
				/* Extra guard: resolved item name should also look like the query. */
				std::string itemName = nameOut && !nameOut->empty() ? *nameOut : ItemName(resolved);
				const int nameScore = ScoreTitleMatch(itemName, tokens, qLow);
				int nameScoreTypo = 0;
				if (!typo.empty())
				{
					std::vector<std::string> typoTok;
					TokenizeQuery(typo.c_str(), typoTok);
					nameScoreTypo = ScoreTitleMatch(itemName, typoTok, ToLowerCopy(typo));
				}
				if (nameScore >= kMinAutoScore || nameScoreTypo >= kMinAutoScore)
				{
					if (nameOut && nameOut->empty()) *nameOut = itemName;
					return resolved;
				}
			}
		}
		return 0;
	}

	DWORD WINAPI PlanProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		for (;;)
		{
			const unsigned gen = gPlanGen.load();
			char q[192];
			std::snprintf(q, sizeof(q), "%s", gThreadQuery);
			Plan plan;
			if (!q[0])
			{
				plan.status = "Paste a chat code, item ID, or name.";
			}
			else
			{
				plan.status = "Resolving item…";
				PublishLivePlan(plan);
				std::string resolvedName;
				const int itemId = ResolveQueryToItemId(q, plan, &resolvedName);
				if (gen != gPlanGen.load())
					goto restart_or_done;
				if (itemId <= 0)
				{
					plan.status = plan.nameHints.empty()
						? "Could not resolve that item."
						: "No close name match — click a wiki title below (check spelling), "
						  "or paste a chat code / ID.";
				}
				else
				{
					plan.outputId = itemId;
					plan.outputName = resolvedName.empty() ? ItemName(itemId) : resolvedName;
					plan.nameHints.clear();
					plan.status = "Fetching recipe…";
					PublishLivePlan(plan);
					std::unordered_map<int, RecipeCacheEntry> recipeCache;
					int outCount = 1;
					std::vector<RecipeIng> ings;
					int recipeId = 0;
					std::string recipeSource;
					if (!TryLoadRecipe(itemId, plan.outputName, outCount, ings, recipeId,
							recipeCache, &recipeSource))
					{
						char buf[192];
						std::snprintf(buf, sizeof(buf),
							"Found %s (#%d) — no station or wiki forge recipe.",
							plan.outputName.empty() ? "item" : plan.outputName.c_str(),
							itemId);
						plan.status = buf;
						plan.outputCount = outCount;
					}
					else if (gen == gPlanGen.load())
					{
						plan.outputCount = outCount;
						plan.recipeSource = recipeSource;
						std::unordered_map<int, std::string> names;
						names[itemId] = plan.outputName;

						/* Paint top recipe immediately — stash / gifts catch up after. */
						plan.root = {};
						plan.root.itemId = itemId;
						plan.root.need = 1;
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
							kid.need = ri.count;
							kid.depth = 1;
							kid.name = ri.name;
							kid.have = 0;
							plan.root.kids.push_back(std::move(kid));
						}
						plan.ok = true;
						plan.nameHints.clear();
						plan.status = std::string(plan.recipeSource.empty() ? "Recipe" : plan.recipeSource)
							+ " · loading stash…";
						PublishLivePlan(plan);

						std::unordered_map<int, int> owned;
						LoadOwned(owned);
						if (gen != gPlanGen.load())
							goto restart_or_done;
						ApplyOwnedCounts(plan.root, owned);
						plan.status = std::string(plan.recipeSource.empty() ? "Recipe" : plan.recipeSource)
							+ " · expanding gifts…";
						PublishLivePlan(plan);

						for (int depth = 2; depth <= kMaxDepth; ++depth)
						{
							if (gen != gPlanGen.load())
								break;
							char st[96];
							std::snprintf(st, sizeof(st), "Expanding gifts (depth %d/%d)…",
								depth, kMaxDepth);
							plan.status = st;
							PublishLivePlan(plan);
							ExpandFrontier(plan, owned, names, recipeCache, depth);
							ApplyOwnedCounts(plan.root, owned);
							plan.status = st;
							if (gen == gPlanGen.load())
								PublishLivePlan(plan);
						}

						if (gen == gPlanGen.load())
						{
							plan.status = "Pricing materials…";
							PublishLivePlan(plan);
							FinishPrices(plan, names);
							PublishLivePlan(plan);
						}
					}
				}
			}
			if (!plan.ok)
			{
				std::lock_guard<std::mutex> lock(gMu);
				if (gen == gPlanGen.load())
					gPlan = std::move(plan);
			}

		restart_or_done:
			if (gen != gPlanGen.load())
				continue; /* newer Plan() — reuse this worker */
			gReady = false;
			gBusy = false;
			/* Lost the race with StartPlan after clearing busy? */
			if (gen != gPlanGen.load() && !gBusy.exchange(true))
				continue;
			return 0;
		}
	}
	void StartPlan()
	{
		std::snprintf(gThreadQuery, sizeof(gThreadQuery), "%s", gQuery);
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPlan.status = "Planning…";
			gPlan.ok = false;
			gPlan.root = {};
			gPlan.nameHints.clear();
		}
		++gPlanGen; /* cancel in-flight expand; worker restarts if still alive */
		if (gBusy.exchange(true))
			return; /* existing PlanProc loop will pick up new gen/query */
		if (gThread)
		{
			/* Reap finished worker; if still winding down, wait briefly (rare). */
			if (WaitForSingleObject(gThread, 8000) == WAIT_OBJECT_0)
			{
				CloseHandle(gThread);
				gThread = nullptr;
			}
		}
		gThread = CreateThread(nullptr, 0, PlanProc, nullptr, 0, nullptr);
		if (!gThread) gBusy = false;
	}

} // namespace CraftingDetail
