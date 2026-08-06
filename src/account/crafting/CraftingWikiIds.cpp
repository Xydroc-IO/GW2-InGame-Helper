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

	int ResolveWikiTitleToItemId(const char* title, std::string* nameOut)
	{
		const std::string wt = FetchWikiWikitext(title);
		if (wt.empty()) return 0;
		return FirstValidItemId(CollectWikiItemIdCandidates(wt), nameOut);
	}

} // namespace CraftingDetail
