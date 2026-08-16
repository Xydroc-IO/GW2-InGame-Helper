#include "CompletionInternal.h"

#include "JsonView.h"

#include <cstddef>
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

}
