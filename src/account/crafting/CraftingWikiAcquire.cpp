#include "CraftingData.h"

#include "CraftingShared.h"

#include <cctype>
#include <string>
#include <vector>

namespace CraftingDetail
{
	/* Parse wiki "tp total placeholder" lines: "59; Mystic Coin" */
	bool ParseTpPlaceholderMats(const std::string& wt, std::vector<RecipeIng>& ings)
	{
		const size_t marker = FindCi(wt, "tp total placeholder");
		if (marker == std::string::npos)
			return false;
		size_t pipe = wt.find('|', marker);
		if (pipe == std::string::npos)
			return false;
		/* Walk until direct cost / closing braces — collect "N; Name" lines. */
		size_t end = wt.find("direct cost", pipe);
		if (end == std::string::npos)
			end = wt.find("}}", pipe);
		if (end == std::string::npos)
			end = wt.size();
		const std::string block = wt.substr(pipe, end - pipe);
		size_t i = 0;
		while (i < block.size())
		{
			while (i < block.size() && (block[i] == ' ' || block[i] == '\t' ||
				block[i] == '\r' || block[i] == '\n' || block[i] == '|'))
				++i;
			if (i >= block.size())
				break;
			int count = 0;
			bool anyDigit = false;
			while (i < block.size() && block[i] >= '0' && block[i] <= '9')
			{
				anyDigit = true;
				count = count * 10 + (block[i] - '0');
				++i;
			}
			if (!anyDigit || count <= 0)
			{
				while (i < block.size() && block[i] != '\n')
					++i;
				continue;
			}
			while (i < block.size() && (block[i] == ' ' || block[i] == '\t' || block[i] == ';'))
				++i;
			size_t nameStart = i;
			while (i < block.size() && block[i] != '\n' && block[i] != '|')
				++i;
			std::string name = CleanWikiLinkName(block.substr(nameStart, i - nameStart));
			while (!name.empty() && (name.back() == ' ' || name.back() == '\t' || name.back() == '\r'))
				name.pop_back();
			if (name.empty() || name.find("info") == 0 || name.find("buys") != std::string::npos)
				continue;
			/* Skip template keys */
			bool looksLikeKey = true;
			for (char c : name)
			{
				if (c == ' ' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
					c == '\'' || c == '-' || c == ',')
					continue;
				looksLikeKey = false;
				break;
			}
			if (name.find('=') != std::string::npos)
				continue;
			(void)looksLikeKey;

			std::string resolvedName;
			const int itemId = ResolveWikiTitleToItemId(name.c_str(), &resolvedName);
			if (itemId <= 0)
				continue;
			RecipeIng ri;
			ri.itemId = itemId;
			ri.count = count;
			ri.name = resolvedName.empty() ? name : resolvedName;
			ings.push_back(std::move(ri));
		}
		return !ings.empty();
	}

	bool LoadWikiAcquisitionBill(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut)
	{
		if (!pageTitle || !pageTitle[0])
			return false;
		const std::string wt = FetchWikiWikitext(pageTitle);
		if (wt.empty())
			return false;
		ings.clear();
		if (!ParseTpPlaceholderMats(wt, ings))
			return false;
		outCount = 1;
		if (sourceOut)
			*sourceOut = "Vendor / wiki acquisition";
		return true;
	}
} // namespace CraftingDetail
