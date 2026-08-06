#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"

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

	/* Higher = closer title match. Reject weak wiki noise (e.g. "bow" -> Emblem...). */
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

	/* Common food typo: "bow of ..." -> "bowl of ...". */
	std::string TypoHintsQuery(const char* q)
	{
		std::string s = q ? q : "";
		std::string low = ToLowerCopy(s);
		/* whole-word bow -> bowl (not already bowl) */
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

		/* Auto-resolve only a confident title match - never first-hit roulette. */
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
				plan.status = "Resolving item...";
				PublishLivePlan(plan);
				std::string resolvedName;
				const int itemId = ResolveQueryToItemId(q, plan, &resolvedName);
				if (gen != gPlanGen.load())
					goto restart_or_done;
				if (itemId <= 0)
				{
					plan.status = plan.nameHints.empty()
						? "Could not resolve that item."
						: "No close name match - click a wiki title below (check spelling), "
						  "or paste a chat code / ID.";
				}
				else
				{
					plan.outputId = itemId;
					plan.outputName = resolvedName.empty() ? ItemName(itemId) : resolvedName;
					plan.nameHints.clear();
					plan.status = "Fetching recipe...";
					PublishLivePlan(plan);
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
					}
					else if (gen == gPlanGen.load())
					{
						plan.outputCount = outCount;
						plan.recipeSource = recipeSource;
						std::unordered_map<int, std::string> names;
						names[itemId] = plan.outputName;

						/* Paint top recipe immediately - stash / gifts catch up after. */
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
							+ " | loading stash...";
						PublishLivePlan(plan);

						std::unordered_map<int, int> owned;
						LoadOwned(owned);
						if (gen != gPlanGen.load())
							goto restart_or_done;
						ApplyOwnedCounts(plan.root, owned);
						plan.status = std::string(plan.recipeSource.empty() ? "Recipe" : plan.recipeSource)
							+ " | expanding gifts...";
						PublishLivePlan(plan);

						for (int depth = 2; depth <= kMaxDepth; ++depth)
						{
							if (gen != gPlanGen.load())
								break;
							char st[96];
							std::snprintf(st, sizeof(st), "Expanding gifts (depth %d/%d)...",
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
							plan.status = "Pricing materials...";
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
				continue; /* newer Plan() - reuse this worker */
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
			gPlan.status = "Planning...";
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
