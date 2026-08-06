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
	size_t FindCi(const std::string& hay, const char* needle, size_t from)
	{
		if (!needle || !needle[0]) return std::string::npos;
		const size_t nlen = std::strlen(needle);
		if (from >= hay.size() || nlen > hay.size()) return std::string::npos;
		for (size_t i = from; i + nlen <= hay.size(); ++i)
		{
			bool ok = true;
			for (size_t j = 0; j < nlen; ++j)
			{
				const unsigned char a = static_cast<unsigned char>(hay[i + j]);
				const unsigned char b = static_cast<unsigned char>(needle[j]);
				if (std::tolower(a) != std::tolower(b)) { ok = false; break; }
			}
			if (ok) return i;
		}
		return std::string::npos;
	}

	void AppendIdsAfterEquals(const std::string& wt, size_t eqPos, std::vector<int>& out)
	{
		size_t k = eqPos + 1;
		while (k < wt.size() && (wt[k] == ' ' || wt[k] == '\t')) ++k;
		/* Support "| id = 91737, 92443" - take each integer. */
		while (k < wt.size())
		{
			int id = 0;
			bool any = false;
			while (k < wt.size() && wt[k] >= '0' && wt[k] <= '9')
			{
				any = true;
				id = id * 10 + (wt[k] - '0');
				++k;
			}
			if (any && id > 0) out.push_back(id);
			while (k < wt.size() && (wt[k] == ' ' || wt[k] == '\t')) ++k;
			if (k < wt.size() && wt[k] == ',')
			{
				++k;
				while (k < wt.size() && (wt[k] == ' ' || wt[k] == '\t')) ++k;
				continue;
			}
			break;
		}
	}

	void CollectIdsInRange(const std::string& wt, size_t from, size_t to, std::vector<int>& out)
	{
		if (to > wt.size()) to = wt.size();
		size_t p = from;
		while (p < to)
		{
			size_t bar = wt.find('|', p);
			if (bar == std::string::npos || bar >= to) break;
			size_t k = bar + 1;
			while (k < to && (wt[k] == ' ' || wt[k] == '\t')) ++k;
			if (k + 2 < to &&
				(wt[k] == 'i' || wt[k] == 'I') &&
				(wt[k + 1] == 'd' || wt[k + 1] == 'D'))
			{
				const char after = (k + 2 < to) ? wt[k + 2] : 0;
				if (after == ' ' || after == '\t' || after == '=')
				{
					while (k < to && wt[k] != '=') ++k;
					if (k < to && wt[k] == '=')
						AppendIdsAfterEquals(wt, k, out);
				}
			}
			p = bar + 1;
		}
	}

	/* Wiki pages put effect ids / recipe ids in | id = before the real item id.
	   Prefer item/weapon/armor infobox blocks; skip {{recipe}} (recipe ids). */
	std::vector<int> CollectWikiItemIdCandidates(const std::string& wikitext)
	{
		std::vector<int> preferred;
		std::vector<int> fallback;
		const char* boxTags[] = {
			"{{item infobox", "{{weapon infobox", "{{armor infobox",
			"{{trinket infobox", "{{back item infobox", "{{accessory infobox",
		};
		size_t box = std::string::npos;
		for (const char* tag : boxTags)
		{
			const size_t at = FindCi(wikitext, tag);
			if (at != std::string::npos && (box == std::string::npos || at < box))
				box = at;
		}
		if (box != std::string::npos)
		{
			size_t recipe = FindCi(wikitext, "{{recipe", box + 1);
			const size_t end = (recipe != std::string::npos) ? recipe : wikitext.size();
			CollectIdsInRange(wikitext, box, end, preferred);
		}
		/* Whole page except recipe templates. */
		size_t p = 0;
		while (p < wikitext.size())
		{
			size_t recipe = FindCi(wikitext, "{{recipe", p);
			if (recipe == std::string::npos)
			{
				CollectIdsInRange(wikitext, p, wikitext.size(), fallback);
				break;
			}
			CollectIdsInRange(wikitext, p, recipe, fallback);
			/* Skip until matching }} after {{recipe - simple depth scan. */
			size_t i = recipe + 2;
			int depth = 1;
			while (i + 1 < wikitext.size() && depth > 0)
			{
				if (wikitext[i] == '{' && wikitext[i + 1] == '{') { depth++; i += 2; continue; }
				if (wikitext[i] == '}' && wikitext[i + 1] == '}') { depth--; i += 2; continue; }
				++i;
			}
			p = i;
		}
		/* Dedup preferred first, then fallback. */
		std::vector<int> out;
		auto addAll = [&](const std::vector<int>& src) {
			for (int id : src)
			{
				bool dup = false;
				for (int x : out) if (x == id) { dup = true; break; }
				if (!dup) out.push_back(id);
			}
		};
		addAll(preferred);
		addAll(fallback);
		return out;
	}
	std::string FetchWikiWikitext(const char* title)
	{
		if (!title || !title[0]) return {};
		const std::string key = ToLowerCopy(title);
		{
			std::lock_guard<std::mutex> lock(gWikiMu);
			auto it = gWikiTextCache.find(key);
			if (it != gWikiTextCache.end())
				return it->second;
		}
		std::string parseUrl =
			"https://wiki.guildwars2.com/api.php?action=parse&prop=wikitext&format=json"
			"&formatversion=2&page=";
		parseUrl += UrlEncode(title);
		auto pr = Gw2Http::Get(parseUrl.c_str(), nullptr, kHttpTimeoutMs);
		std::string wt;
		if (pr.ok)
		{
			wt = JsonStringAfterKey(pr.body, "wikitext", 0);
			if (wt.empty())
			{
				size_t wk = pr.body.find("\"wikitext\"");
				if (wk != std::string::npos)
					wt = JsonStringAfterKey(pr.body, "wikitext", wk);
			}
		}
		{
			std::lock_guard<std::mutex> lock(gWikiMu);
			/* Never cache misses - a timeout would poison Sync forever. */
			if (!wt.empty())
				gWikiTextCache[key] = wt;
		}
		return wt;
	}

	/* One MediaWiki query for many titles (legendary gift ingredients). */
	void FetchWikiWikitextBatch(const std::vector<std::string>& titles,
		std::unordered_map<std::string, std::string>& outByKey)
	{
		std::vector<std::string> miss;
		miss.reserve(titles.size());
		{
			std::lock_guard<std::mutex> lock(gWikiMu);
			for (const std::string& t : titles)
			{
				if (t.empty()) continue;
				const std::string key = ToLowerCopy(t);
				auto it = gWikiTextCache.find(key);
				if (it != gWikiTextCache.end())
					outByKey[key] = it->second;
				else
					miss.push_back(t);
			}
		}
		for (size_t off = 0; off < miss.size(); off += 8)
		{
			const size_t n = (std::min)(miss.size() - off, size_t{8});
			std::string url =
				"https://wiki.guildwars2.com/api.php?action=query&prop=revisions"
				"&rvprop=content&rvslots=main&format=json&formatversion=2&titles=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) url += '|';
				url += UrlEncode(miss[off + i].c_str());
			}
			auto pr = Gw2Http::Get(url.c_str(), nullptr, kBulkTimeoutMs);
			if (!pr.ok) continue;
			/* formatversion=2: pages:[{title,revisions:[{slots:{main:{content}}}]}] */
			size_t p = 0;
			while (p < pr.body.size())
			{
				size_t titleKey = pr.body.find("\"title\"", p);
				if (titleKey == std::string::npos) break;
				std::string title = JsonStringAfterKey(pr.body, "title", titleKey);
				size_t contentKey = pr.body.find("\"content\"", titleKey);
				size_t nextTitle = pr.body.find("\"title\"", titleKey + 7);
				std::string wt;
				if (contentKey != std::string::npos &&
					(nextTitle == std::string::npos || contentKey < nextTitle))
					wt = JsonStringAfterKey(pr.body, "content", contentKey);
				if (!title.empty())
				{
					const std::string key = ToLowerCopy(title);
					std::lock_guard<std::mutex> lock(gWikiMu);
					if (!wt.empty())
						gWikiTextCache[key] = wt;
					outByKey[key] = wt;
				}
				p = titleKey + 7;
			}
			/* Alias requested spellings - fetch misses in parallel. */
			std::vector<std::string> stillMiss;
			for (size_t i = 0; i < n; ++i)
			{
				const std::string reqKey = ToLowerCopy(miss[off + i]);
				if (outByKey.find(reqKey) != outByKey.end() && !outByKey[reqKey].empty())
					continue;
				stillMiss.push_back(miss[off + i]);
			}
			struct WikiJob
			{
				std::string title;
				std::string wt;
				static DWORD WINAPI Thunk(void* p)
				{
					auto* j = static_cast<WikiJob*>(p);
					j->wt = FetchWikiWikitext(j->title.c_str());
					return 0;
				}
			};
			std::vector<WikiJob> wjobs(stillMiss.size());
			std::vector<HANDLE> whs;
			whs.reserve(stillMiss.size());
			for (size_t i = 0; i < stillMiss.size(); ++i)
			{
				wjobs[i].title = stillMiss[i];
				HANDLE h = CreateThread(nullptr, 0, WikiJob::Thunk, &wjobs[i], 0, nullptr);
				if (h) whs.push_back(h);
				else WikiJob::Thunk(&wjobs[i]);
			}
			if (!whs.empty())
			{
				size_t woff = 0;
				while (woff < whs.size())
				{
					const DWORD chunk = static_cast<DWORD>(
						(whs.size() - woff > 64) ? 64 : (whs.size() - woff));
					WaitForMultipleObjects(chunk, whs.data() + woff, TRUE,
						static_cast<DWORD>(kHttpTimeoutMs + 4000));
					woff += chunk;
				}
				for (HANDLE h : whs)
					CloseHandle(h);
			}
			for (WikiJob& wj : wjobs)
			{
				const std::string reqKey = ToLowerCopy(wj.title);
				outByKey[reqKey] = wj.wt;
			}
		}
	}
	int ResolveWikiTitleToItemId(const char* title, std::string* nameOut)
	{
		const std::string wt = FetchWikiWikitext(title);
		if (wt.empty()) return 0;
		return FirstValidItemId(CollectWikiItemIdCandidates(wt), nameOut);
	}

	std::string CleanWikiLinkName(std::string s)
	{
		/* Trim + unwrap [[Name]] / [[Name|Label]] -> Name */
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
		if (s.size() >= 4 && s[0] == '[' && s[1] == '[')
		{
			s = s.substr(2);
			const size_t end = s.find("]]");
			if (end != std::string::npos) s = s.substr(0, end);
			const size_t pipe = s.find('|');
			if (pipe != std::string::npos) s = s.substr(0, pipe);
		}
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
		return s;
	}

	/* Legendaries / mystic forge are not on /v2/recipes - read {{recipe}} from the wiki. */
	bool LoadWikiRecipeForName(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut)
	{
		if (!pageTitle || !pageTitle[0]) return false;
		const std::string wt = FetchWikiWikitext(pageTitle);
		if (wt.empty()) return false;

		size_t searchFrom = 0;
		std::string bestSource;
		std::vector<RecipeIng> bestIngs;
		int bestQty = 1;
		int bestScore = -1;

		while (searchFrom < wt.size())
		{
			size_t recipe = FindCi(wt, "{{recipe", searchFrom);
			if (recipe == std::string::npos) break;
			size_t i = recipe + 2;
			int depth = 1;
			while (i + 1 < wt.size() && depth > 0)
			{
				if (wt[i] == '{' && wt[i + 1] == '{') { depth++; i += 2; continue; }
				if (wt[i] == '}' && wt[i + 1] == '}') { depth--; i += 2; continue; }
				++i;
			}
			const size_t blockEnd = i;
			const std::string block = wt.substr(recipe, blockEnd - recipe);
			searchFrom = blockEnd;

			std::string source;
			std::string recipeType;
			int qty = 1;
			bool hasUpperQty = false;
			struct NamedIng { int count = 1; std::string name; };
			std::vector<NamedIng> named;

			size_t p = 0;
			while (p < block.size())
			{
				size_t bar = block.find('|', p);
				if (bar == std::string::npos) break;
				size_t k = bar + 1;
				while (k < block.size() && (block[k] == ' ' || block[k] == '\t')) ++k;
				size_t keyStart = k;
				while (k < block.size() && block[k] != '=' && block[k] != '\n' && block[k] != '|')
					++k;
				if (k >= block.size() || block[k] != '=') { p = bar + 1; continue; }
				std::string key = block.substr(keyStart, k - keyStart);
				while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
				for (char& c : key)
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				++k;
				while (k < block.size() && (block[k] == ' ' || block[k] == '\t')) ++k;
				size_t valStart = k;
				while (k < block.size() && block[k] != '|' && block[k] != '\n') ++k;
				std::string val = block.substr(valStart, k - valStart);
				while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r'))
					val.pop_back();
				p = (k < block.size() && block[k] == '\n') ? k + 1 : k;

				if (key == "source")
					source = val;
				else if (key == "type")
					recipeType = val;
				else if (key == "upper quantity" || key == "upper qty")
					hasUpperQty = true;
				else if (key == "quantity" || key == "qty")
				{
					int v = 0;
					for (char c : val)
						if (c >= '0' && c <= '9') v = v * 10 + (c - '0');
					if (v > 0) qty = v;
				}
				else if (key.size() >= 10 && key.compare(0, 10, "ingredient") == 0)
				{
					/* "1 Zap" or "250 Glob of Ectoplasm" or "[[Zap]]" */
					size_t vi = 0;
					while (vi < val.size() && (val[vi] == ' ' || val[vi] == '\t')) ++vi;
					int count = 0;
					bool anyDigit = false;
					while (vi < val.size() && val[vi] >= '0' && val[vi] <= '9')
					{
						anyDigit = true;
						count = count * 10 + (val[vi] - '0');
						++vi;
					}
					if (!anyDigit) count = 1;
					while (vi < val.size() && (val[vi] == ' ' || val[vi] == '\t')) ++vi;
					std::string iname = CleanWikiLinkName(val.substr(vi));
					if (!iname.empty() && count > 0)
						named.push_back({ count, iname });
				}
			}

			if (named.empty()) continue;

			const std::string typeLow = ToLowerCopy(recipeType);
			const std::string srcLow = ToLowerCopy(source);
			const std::string pageLow = ToLowerCopy(pageTitle);
			/* Mystic Forge promotions (mithril->orichalcum, dust ladders) inflate plans. */
			if (typeLow.find("promotion") != std::string::npos) continue;
			if (hasUpperQty) continue;
			if (typeLow.find("salvage") != std::string::npos) continue;
			bool feedsSelf = false;
			for (const NamedIng& ni : named)
			{
				if (ToLowerCopy(ni.name) == pageLow) { feedsSelf = true; break; }
			}
			if (feedsSelf) continue;

			const bool forge = srcLow.find("mystic") != std::string::npos ||
				srcLow.find("forge") != std::string::npos;
			const bool station = srcLow.find("recipe") != std::string::npos ||
				srcLow.find("weaponsmith") != std::string::npos ||
				srcLow.find("artificer") != std::string::npos ||
				srcLow.find("huntsman") != std::string::npos ||
				srcLow.find("tailor") != std::string::npos ||
				srcLow.find("armorsmith") != std::string::npos ||
				srcLow.find("jeweler") != std::string::npos ||
				srcLow.find("chef") != std::string::npos ||
				srcLow.find("scribe") != std::string::npos;
			int score = 0;
			if (forge) score += 40;
			if (station) score += 55;
			if (typeLow.find("legendary") != std::string::npos) score += 50;
			if (typeLow.find("component") != std::string::npos) score += 15;
			if (score < 30) continue;

			if (score < bestScore) continue;

			std::vector<std::string> ingTitles;
			ingTitles.reserve(named.size());
			for (const NamedIng& ni : named)
				ingTitles.push_back(ni.name);
			std::unordered_map<std::string, std::string> pages;
			FetchWikiWikitextBatch(ingTitles, pages);

			std::vector<std::vector<int>> cands(named.size());
			std::vector<int> allIds;
			for (size_t ii = 0; ii < named.size(); ++ii)
			{
				const std::string key = ToLowerCopy(named[ii].name);
				auto pit = pages.find(key);
				const std::string& pageWt = (pit != pages.end()) ? pit->second : std::string{};
				if (pageWt.empty()) continue;
				cands[ii] = CollectWikiItemIdCandidates(pageWt);
				for (int id : cands[ii]) allIds.push_back(id);
			}
			std::unordered_map<int, std::string> idNames;
			FetchNames(idNames, allIds);

			std::vector<RecipeIng> resolved;
			for (size_t ii = 0; ii < named.size(); ++ii)
			{
				int id = 0;
				std::string resolvedName;
				for (int cand : cands[ii])
				{
					auto it = idNames.find(cand);
					if (it == idNames.end() || it->second.empty()) continue;
					id = cand;
					resolvedName = it->second;
					break;
				}
				if (id <= 0) continue;
				RecipeIng ri;
				ri.itemId = id;
				ri.count = named[ii].count;
				ri.name = resolvedName.empty() ? named[ii].name : resolvedName;
				resolved.push_back(ri);
			}
			if (resolved.empty()) continue;

			bestScore = score;
			bestQty = qty > 0 ? qty : 1;
			bestSource = source.empty() ? "Wiki recipe" : source;
			bestIngs = std::move(resolved);
			if (score >= 90) break; /* strong station/legendary hit */
		}

		if (bestIngs.empty()) return false;
		outCount = bestQty;
		ings = std::move(bestIngs);
		if (sourceOut) *sourceOut = bestSource;
		return true;
	}

} // namespace CraftingDetail
