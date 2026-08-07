#include "WalletShared.h"

#include <string>

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
} // namespace WalletDetail
