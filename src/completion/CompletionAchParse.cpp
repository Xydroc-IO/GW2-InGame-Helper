#include "CompletionInternal.h"

#include "JsonView.h"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		void CollectIntsInArray(const std::string& json, size_t openBracket, size_t limit,
			std::vector<int>& out)
		{
			if (openBracket >= json.size() || json[openBracket] != '[')
				return;
			size_t k = openBracket + 1;
			while (k < limit && k < json.size() && json[k] != ']')
			{
				int id = 0;
				size_t after = k;
				if (JsonView::ParseInt32(JsonView::AsView(json), k, &id, &after) && id > 0)
					out.push_back(id);
				k = (after > k) ? after : k + 1;
				while (k < limit && k < json.size() && json[k] != ']' &&
					(json[k] < '0' || json[k] > '9') && json[k] != '-')
					++k;
			}
		}

		void CollectIntsAfterKey(const std::string& json, const char* key, size_t from, size_t objEnd,
			std::vector<int>& out)
		{
			const size_t vs = JsonView::ValueStartAfterKey(JsonView::AsView(json),
				JsonView::View(key), from);
			if (vs == JsonView::View::npos || vs >= objEnd)
				return;
			CollectIntsInArray(json, vs, objEnd, out);
		}

	}

	void ParseAchGroups(const std::string& body, std::vector<AchGroup>& out)
		{
			size_t p = 0;
			while (p < body.size())
			{
				const size_t brace = body.find('{', p);
				if (brace == std::string::npos)
					break;
				const size_t end = JsonView::ObjectEnd(body, brace);
				if (end == std::string::npos)
					break;
				AchGroup g;
				g.id = JsonView::StringAfterKey(body, "id", brace);
				g.name = JsonView::StringAfterKey(body, "name", brace);
				const long long ord = JsonView::IntAfterKey(body, "order", brace);
				g.order = ord > 0 ? static_cast<int>(ord) : 0;
				CollectIntsAfterKey(body, "categories", brace, end, g.categoryIds);
				if (!g.id.empty() && !g.name.empty())
					out.push_back(std::move(g));
				p = end + 1;
			}
		}

	void ParseAchCategories(const std::string& body, std::unordered_map<int, AchCategory>& out)
		{
			size_t p = 0;
			while (p < body.size())
			{
				const size_t brace = body.find('{', p);
				if (brace == std::string::npos)
					break;
				const size_t end = JsonView::ObjectEnd(body, brace);
				if (end == std::string::npos)
					break;
				const long long id = JsonView::IntAfterKey(body, "id", brace);
				if (id > 0)
				{
					AchCategory c;
					c.id = static_cast<int>(id);
					c.name = JsonView::StringAfterKey(body, "name", brace);
					const long long ord = JsonView::IntAfterKey(body, "order", brace);
					c.order = ord > 0 ? static_cast<int>(ord) : 0;
					CollectIntsAfterKey(body, "achievements", brace, end, c.achievementIds);
					out[c.id] = std::move(c);
				}
				p = end + 1;
			}
		}

	void ParseAchDefs(const std::string& body, std::unordered_map<int, AchDef>& out)
		{
			size_t p = 0;
			while (p < body.size())
			{
				const size_t brace = body.find('{', p);
				if (brace == std::string::npos)
					break;
				const size_t end = JsonView::ObjectEnd(body, brace);
				if (end == std::string::npos)
					break;
				const long long id = JsonView::IntAfterKey(body, "id", brace);
				if (id > 0)
				{
					AchDef d;
					d.id = static_cast<int>(id);
					d.name = JsonView::StringAfterKey(body, "name", brace);
					d.requirement = JsonView::StringAfterKey(body, "requirement", brace);
					d.description = JsonView::StringAfterKey(body, "description", brace);
					d.lockedText = JsonView::StringAfterKey(body, "locked_text", brace);
					const long long pts = JsonView::IntAfterKey(body, "points", brace);
					d.points = pts > 0 ? static_cast<int>(pts) : 0;
					const std::string slice = body.substr(brace, end - brace + 1);
					d.hidden = slice.find("Hidden") != std::string::npos;
					d.repeatable = slice.find("Repeatable") != std::string::npos;
					size_t bp = JsonView::ValueStartAfterKey(JsonView::AsView(body),
						JsonView::View("bits"), brace);
					if (bp != JsonView::View::npos && bp < end && body[bp] == '[')
					{
						const size_t arrEnd = JsonView::ArrayEnd(body, bp);
						const size_t limit = (arrEnd != JsonView::View::npos && arrEnd <= end)
							? arrEnd : end;
						size_t q = bp + 1;
						while (q < limit)
						{
							const size_t b2 = body.find('{', q);
							if (b2 == std::string::npos || b2 >= limit)
								break;
							const size_t e2 = JsonView::ObjectEnd(body, b2);
							if (e2 == std::string::npos || e2 >= limit)
								break;
							const std::string bitJson = body.substr(b2, e2 - b2 + 1);
							AchBit bit;
							bit.text = JsonView::StringAfterKey(bitJson, "text", 0);
							const std::string ty = JsonView::StringAfterKey(bitJson, "type", 0);
							const long long tid = JsonView::IntAfterKey(bitJson, "id", 0);
							if (tid > 0)
								bit.targetId = static_cast<int>(tid);
							if (ty == "Item")
								bit.kind = AchBitKind::Item;
							else if (ty == "Skin")
								bit.kind = AchBitKind::Skin;
							else if (ty == "Minipet")
								bit.kind = AchBitKind::Mini;
							else if (ty == "Achievement")
								bit.kind = AchBitKind::Achievement;
							else if (ty == "Text" || ty.empty())
								bit.kind = AchBitKind::Text;
							else
								bit.kind = AchBitKind::Other;
							d.bits.push_back(std::move(bit));
							q = e2 + 1;
						}
					}
					size_t tp = JsonView::ValueStartAfterKey(JsonView::AsView(body),
						JsonView::View("tiers"), brace);
					if (tp != JsonView::View::npos && tp < end && body[tp] == '[')
					{
						const size_t arrEnd = JsonView::ArrayEnd(body, tp);
						const size_t limit = (arrEnd != JsonView::View::npos && arrEnd <= end)
							? arrEnd : end;
						size_t q = tp + 1;
						while (q < limit)
						{
							const size_t b2 = body.find('{', q);
							if (b2 == std::string::npos || b2 >= limit)
								break;
							const size_t e2 = JsonView::ObjectEnd(body, b2);
							if (e2 == std::string::npos || e2 >= limit)
								break;
							const std::string tj = body.substr(b2, e2 - b2 + 1);
							AchTier tier;
							const long long cnt = JsonView::IntAfterKey(tj, "count", 0);
							const long long pts = JsonView::IntAfterKey(tj, "points", 0);
							if (cnt > 0)
								tier.count = static_cast<int>(cnt);
							if (pts > 0)
								tier.points = static_cast<int>(pts);
							if (tier.count > 0)
								d.tiers.push_back(tier);
							q = e2 + 1;
						}
					}
					out[d.id] = std::move(d);
				}
				p = end + 1;
			}
		}

	static void SplitTabs(const std::string& line, std::vector<std::string>& out)
	{
		out.clear();
		size_t i = 0;
		while (i <= line.size())
		{
			const size_t tab = line.find('\t', i);
			if (tab == std::string::npos)
			{
				out.push_back(line.substr(i));
				break;
			}
			out.push_back(line.substr(i, tab - i));
			i = tab + 1;
		}
	}

	static void ParseIntCsv(const std::string& s, std::vector<int>& out)
	{
		size_t i = 0;
		while (i < s.size())
		{
			int id = 0;
			bool any = false;
			while (i < s.size() && s[i] >= '0' && s[i] <= '9')
			{
				id = id * 10 + (s[i++] - '0');
				any = true;
			}
			if (any && id > 0)
				out.push_back(id);
			if (i < s.size() && s[i] == ',')
				++i;
			else if (i < s.size())
				++i;
			else
				break;
		}
	}

	static AchBitKind BitKindFromChar(char c)
	{
		switch (c)
		{
		case 'i': return AchBitKind::Item;
		case 's': return AchBitKind::Skin;
		case 'n': return AchBitKind::Mini;
		case 'a': return AchBitKind::Achievement;
		case 't': return AchBitKind::Text;
		default: return AchBitKind::Other;
		}
	}

	static void ParseBitsField(const std::string& s, std::vector<AchBit>& out)
	{
		size_t i = 0;
		while (i < s.size())
		{
			size_t bar = s.find('|', i);
			if (bar == std::string::npos)
				bar = s.size();
			const std::string piece = s.substr(i, bar - i);
			if (!piece.empty())
			{
				AchBit bit;
				bit.kind = BitKindFromChar(piece[0]);
				size_t c1 = piece.find(',');
				if (c1 != std::string::npos)
				{
					bit.targetId = std::atoi(piece.c_str() + static_cast<int>(c1) + 1);
					if (bit.targetId < 0)
						bit.targetId = 0;
					const size_t c2 = piece.find(',', c1 + 1);
					if (c2 != std::string::npos)
						bit.text = piece.substr(c2 + 1);
				}
				out.push_back(std::move(bit));
			}
			if (bar == s.size())
				break;
			i = bar + 1;
		}
	}

	static void ParseTiersField(const std::string& s, std::vector<AchTier>& out)
	{
		size_t i = 0;
		while (i < s.size())
		{
			int cnt = 0, pts = 0;
			while (i < s.size() && s[i] >= '0' && s[i] <= '9')
				cnt = cnt * 10 + (s[i++] - '0');
			if (i < s.size() && s[i] == ':')
			{
				++i;
				while (i < s.size() && s[i] >= '0' && s[i] <= '9')
					pts = pts * 10 + (s[i++] - '0');
			}
			if (cnt > 0)
			{
				AchTier t;
				t.count = cnt;
				t.points = pts > 0 ? pts : 0;
				out.push_back(t);
			}
			if (i < s.size() && s[i] == ',')
				++i;
			else
				break;
		}
	}

	void ParseAchGroupsTsv(const std::string& tsv, std::vector<AchGroup>& out)
	{
		std::vector<std::string> cols;
		size_t i = 0;
		while (i < tsv.size())
		{
			size_t eol = tsv.find('\n', i);
			if (eol == std::string::npos)
				eol = tsv.size();
			std::string line = tsv.substr(i, eol - i);
			i = eol + 1;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty() || line[0] == '#' || line[0] != 'g')
				continue;
			SplitTabs(line, cols);
			if (cols.size() < 4)
				continue;
			AchGroup g;
			g.id = cols[1];
			g.order = std::atoi(cols[2].c_str());
			if (g.order < 0)
				g.order = 0;
			g.name = cols[3];
			if (cols.size() >= 5)
				ParseIntCsv(cols[4], g.categoryIds);
			if (!g.id.empty() && !g.name.empty())
				out.push_back(std::move(g));
		}
	}

	void ParseAchCategoriesTsv(const std::string& tsv, std::unordered_map<int, AchCategory>& out)
	{
		std::vector<std::string> cols;
		size_t i = 0;
		while (i < tsv.size())
		{
			size_t eol = tsv.find('\n', i);
			if (eol == std::string::npos)
				eol = tsv.size();
			std::string line = tsv.substr(i, eol - i);
			i = eol + 1;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty() || line[0] == '#' || line[0] != 'c')
				continue;
			SplitTabs(line, cols);
			if (cols.size() < 4)
				continue;
			AchCategory c;
			c.id = std::atoi(cols[1].c_str());
			if (c.id <= 0)
				continue;
			c.order = std::atoi(cols[2].c_str());
			if (c.order < 0)
				c.order = 0;
			c.name = cols[3];
			if (cols.size() >= 5)
				ParseIntCsv(cols[4], c.achievementIds);
			out[c.id] = std::move(c);
		}
	}

	void ParseAchDefsTsv(const std::string& tsv, std::unordered_map<int, AchDef>& out)
	{
		std::vector<std::string> cols;
		size_t i = 0;
		while (i < tsv.size())
		{
			size_t eol = tsv.find('\n', i);
			if (eol == std::string::npos)
				eol = tsv.size();
			std::string line = tsv.substr(i, eol - i);
			i = eol + 1;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty() || line[0] == '#' || line[0] != 'd')
				continue;
			SplitTabs(line, cols);
			if (cols.size() < 5)
				continue;
			AchDef d;
			d.id = std::atoi(cols[1].c_str());
			if (d.id <= 0)
				continue;
			d.points = std::atoi(cols[2].c_str());
			if (d.points < 0)
				d.points = 0;
			const std::string& flags = cols[3];
			d.hidden = flags.find('h') != std::string::npos;
			d.repeatable = flags.find('r') != std::string::npos;
			d.name = cols[4];
			if (cols.size() >= 6)
				d.requirement = cols[5];
			if (cols.size() >= 7)
				d.description = cols[6];
			if (cols.size() >= 8)
				d.lockedText = cols[7];
			if (cols.size() >= 9)
				ParseBitsField(cols[8], d.bits);
			if (cols.size() >= 10)
				ParseTiersField(cols[9], d.tiers);
			out[d.id] = std::move(d);
		}
	}

}
