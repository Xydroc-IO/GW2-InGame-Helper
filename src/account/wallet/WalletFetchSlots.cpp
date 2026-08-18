#include "WalletShared.h"

#include "Gw2Catalog.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace WalletDetail
{
	/* Int for a key on THIS object only (depth 1) — ignores nested stats/ids. */
	static long long IntKeyInObject(const std::string& json, size_t brace, size_t end, const char* key)
	{
		if (!key || brace >= end || brace >= json.size() || json[brace] != '{')
			return -1;
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		int depth = 0;
		bool inStr = false, esc = false;
		for (size_t i = brace; i < end && i < json.size(); ++i)
		{
			const char c = json[i];
			if (inStr)
			{
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"')
			{
				if (depth == 1 && i + pat.size() <= end &&
					json.compare(i, pat.size(), pat) == 0)
				{
					size_t k = json.find(':', i + pat.size());
					if (k == std::string::npos || k >= end)
						return -1;
					++k;
					while (k < end && (json[k] == ' ' || json[k] == '\t')) ++k;
					bool neg = false;
					if (k < end && json[k] == '-') { neg = true; ++k; }
					long long v = 0;
					bool any = false;
					while (k < end && json[k] >= '0' && json[k] <= '9')
					{
						any = true;
						v = v * 10 + (json[k] - '0');
						++k;
					}
					if (!any) return -1;
					return neg ? -v : v;
				}
				inStr = true;
				continue;
			}
			if (c == '{') ++depth;
			else if (c == '}')
			{
				--depth;
				if (depth <= 0)
					return -1;
			}
		}
		return -1;
	}

	void CollectSlots(const std::string& body, QtyMap& m)
	{
		/* Walk every '{' in the document. Character bags nest items under
		   inventory[]; advancing only to brace+1 visits those children.
		   Count only objects that have both id and count at the same level
		   (skips bag wrappers and stats blobs). */
		size_t p = 0;
		while (p < body.size())
		{
			const size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			const size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos)
				break;

			const long long id = IntKeyInObject(body, brace, end, "id");
			const long long count = IntKeyInObject(body, brace, end, "count");
			if (id > 0 && count > 0)
				m[static_cast<int>(id)] += static_cast<int>(
					count > 2147483647LL ? 2147483647LL : count);

			p = brace + 1;
		}
	}

	void CollectOrderedSlots(const std::string& json, size_t openBracket,
		std::vector<SlotCell>& out)
	{
		if (openBracket >= json.size() || json[openBracket] != '[')
			return;
		size_t i = openBracket + 1;
		while (i < json.size())
		{
			while (i < json.size() && (json[i] == ' ' || json[i] == '\t' ||
				json[i] == '\n' || json[i] == '\r' || json[i] == ','))
				++i;
			if (i >= json.size() || json[i] == ']')
				break;
			if (i + 4 <= json.size() && json.compare(i, 4, "null") == 0)
			{
				out.push_back({});
				i += 4;
				continue;
			}
			if (json[i] == '{')
			{
				const size_t end = JsonObjectEnd(json, i);
				if (end == std::string::npos)
					break;
				SlotCell c;
				const long long id = IntKeyInObject(json, i, end, "id");
				const long long count = IntKeyInObject(json, i, end, "count");
				if (id > 0)
				{
					c.id = static_cast<int>(id);
					c.count = count > 0 ? static_cast<int>(count) : 1;
				}
				out.push_back(c);
				i = end + 1;
				continue;
			}
			++i;
		}
	}

	void FinishSection(SlotSection& s)
	{
		s.capacity = static_cast<int>(s.slots.size());
		s.filled = 0;
		for (const SlotCell& c : s.slots)
		{
			if (c.id > 0 && c.count > 0)
				++s.filled;
		}
	}

	void CollectBankTabs(const std::string& body, std::vector<SlotSection>& out)
	{
		const size_t br = body.find('[');
		if (br == std::string::npos)
			return;
		std::vector<SlotCell> slots;
		CollectOrderedSlots(body, br, slots);
		constexpr int kTab = 30;
		int tab = 1;
		for (size_t i = 0; i < slots.size(); i += static_cast<size_t>(kTab))
		{
			SlotSection s;
			s.kind = Loc_Bank;
			char title[40];
			std::snprintf(title, sizeof(title), "Bank Tab %d", tab++);
			s.title = title;
			const size_t n = (std::min)(static_cast<size_t>(kTab), slots.size() - i);
			s.slots.assign(slots.begin() + static_cast<std::ptrdiff_t>(i),
				slots.begin() + static_cast<std::ptrdiff_t>(i + n));
			FinishSection(s);
			out.push_back(std::move(s));
		}
	}

	void CollectSharedSlots(const std::string& body, std::vector<SlotSection>& out)
	{
		const size_t br = body.find('[');
		if (br == std::string::npos)
			return;
		SlotSection s;
		s.kind = Loc_Shared;
		s.title = "Shared Inventory Slots";
		CollectOrderedSlots(body, br, s.slots);
		FinishSection(s);
		if (!s.slots.empty())
			out.push_back(std::move(s));
	}

	void CollectMaterialSections(const std::string& body, const std::string& catJson,
		std::vector<SlotSection>& out)
	{
		std::unordered_map<int, std::string> catNames;
		size_t p = 0;
		while (p < catJson.size())
		{
			const size_t brace = catJson.find('{', p);
			if (brace == std::string::npos)
				break;
			const size_t end = JsonObjectEnd(catJson, brace);
			if (end == std::string::npos)
				break;
			const long long id = JsonIntAfterKey(catJson, "id", brace);
			const std::string name = JsonStringAfterKey(catJson, "name", brace);
			if (id > 0 && !name.empty())
				catNames[static_cast<int>(id)] = name;
			p = end + 1;
		}

		std::unordered_map<int, SlotSection> byCat;
		std::vector<int> order;
		p = 0;
		while (p < body.size())
		{
			const size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			const size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos)
				break;
			const long long id = JsonIntAfterKey(body, "id", brace);
			const long long count = JsonIntAfterKey(body, "count", brace);
			long long cat = JsonIntAfterKey(body, "category", brace);
			if (id > 0)
			{
				const int cid = cat > 0 ? static_cast<int>(cat) : 0;
				auto it = byCat.find(cid);
				if (it == byCat.end())
				{
					SlotSection s;
					s.kind = Loc_Materials;
					auto nm = catNames.find(cid);
					if (nm != catNames.end())
						s.title = nm->second;
					else
					{
						std::string cat;
						if (cid > 0 && Gw2Catalog::MaterialCategoryName(cid, &cat))
							s.title = std::move(cat);
						else if (cid > 0)
						{
							char buf[40];
							std::snprintf(buf, sizeof(buf), "Materials %d", cid);
							s.title = buf;
						}
						else
							s.title = "Materials";
					}
					order.push_back(cid);
					it = byCat.emplace(cid, std::move(s)).first;
				}
				SlotCell c;
				c.id = static_cast<int>(id);
				c.count = count > 0 ? static_cast<int>(count) : 0;
				it->second.slots.push_back(c);
			}
			p = end + 1;
		}
		for (int cid : order)
		{
			SlotSection& s = byCat[cid];
			FinishSection(s);
			out.push_back(std::move(s));
		}
	}

	void CollectCharBagSections(const std::string& body, const std::string& charName,
		std::vector<SlotSection>& out)
	{
		const size_t bagsKey = body.find("\"bags\"");
		if (bagsKey == std::string::npos)
			return;
		const size_t br = body.find('[', bagsKey);
		if (br == std::string::npos)
			return;
		size_t i = br + 1;
		int bagN = 1;
		while (i < body.size())
		{
			while (i < body.size() && (body[i] == ' ' || body[i] == '\t' ||
				body[i] == '\n' || body[i] == '\r' || body[i] == ','))
				++i;
			if (i >= body.size() || body[i] == ']')
				break;
			if (i + 4 <= body.size() && body.compare(i, 4, "null") == 0)
			{
				i += 4;
				++bagN;
				continue;
			}
			if (body[i] != '{')
			{
				++i;
				continue;
			}
			const size_t end = JsonObjectEnd(body, i);
			if (end == std::string::npos)
				break;
			const long long bagId = IntKeyInObject(body, i, end, "id");
			const size_t inv = body.find("\"inventory\"", i);
			SlotSection s;
			s.kind = Loc_Character;
			char title[128];
			if (bagId > 0)
			{
				const std::string nm = LookupName(static_cast<int>(bagId),
					static_cast<int>(bagId), false);
				if (!nm.empty())
					std::snprintf(title, sizeof(title), "%s — %s", charName.c_str(), nm.c_str());
				else
					std::snprintf(title, sizeof(title), "%s — Bag %d", charName.c_str(), bagN);
			}
			else
				std::snprintf(title, sizeof(title), "%s — Bag %d", charName.c_str(), bagN);
			s.title = title;
			if (inv != std::string::npos && inv < end)
			{
				const size_t ibr = body.find('[', inv);
				if (ibr != std::string::npos && ibr < end)
					CollectOrderedSlots(body, ibr, s.slots);
			}
			FinishSection(s);
			if (!s.slots.empty())
				out.push_back(std::move(s));
			i = end + 1;
			++bagN;
		}
	}
} // namespace WalletDetail
