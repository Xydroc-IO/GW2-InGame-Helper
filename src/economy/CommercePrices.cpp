#include "CommerceShared.h"

#include "Gw2Http.h"
#include "JsonView.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace Commerce
{
	namespace
	{
		struct CacheEntry
		{
			Quote q;
			DWORD atMs = 0;
		};

		std::mutex gMu;
		std::unordered_map<int, CacheEntry> gCache;

		size_t JsonObjectEnd(const std::string& json, size_t openBrace)
		{
			if (openBrace >= json.size() || json[openBrace] != '{')
				return std::string::npos;
			int depth = 0;
			bool inStr = false, esc = false;
			for (size_t i = openBrace; i < json.size(); ++i)
			{
				const char c = json[i];
				if (inStr)
				{
					if (esc) esc = false;
					else if (c == '\\') esc = true;
					else if (c == '"') inStr = false;
					continue;
				}
				if (c == '"') inStr = true;
				else if (c == '{') ++depth;
				else if (c == '}')
				{
					--depth;
					if (depth == 0) return i;
				}
			}
			return std::string::npos;
		}

		void ParsePricesBody(const std::string& body, std::unordered_map<int, Quote>& out)
		{
			size_t p = 0;
			while (p < body.size())
			{
				const size_t brace = body.find('{', p);
				if (brace == std::string::npos) break;
				const size_t end = JsonObjectEnd(body, brace);
				if (end == std::string::npos) break;

				const auto idLL = JsonView::IntAfterKey(JsonView::AsView(body), "id", brace);
				if (idLL <= 0)
				{
					p = end + 1;
					continue;
				}
				Quote q{};
				q.id = static_cast<int>(idLL);
				const size_t buys = body.find("\"buys\"", brace);
				const size_t sells = body.find("\"sells\"", brace);
				if (buys != std::string::npos && buys < end)
				{
					const long long up = JsonView::IntAfterKey(JsonView::AsView(body), "unit_price", buys);
					const long long qty = JsonView::IntAfterKey(JsonView::AsView(body), "quantity", buys);
					if (up >= 0) q.buy = up;
					if (qty >= 0) q.demand = static_cast<int>(qty);
				}
				if (sells != std::string::npos && sells < end)
				{
					const long long up = JsonView::IntAfterKey(JsonView::AsView(body), "unit_price", sells);
					const long long qty = JsonView::IntAfterKey(JsonView::AsView(body), "quantity", sells);
					if (up >= 0) q.sell = up;
					if (qty >= 0) q.supply = static_cast<int>(qty);
				}
				q.ok = (q.buy > 0 || q.sell > 0);
				out[q.id] = q;
				p = end + 1;
			}
		}
	}

	bool PeekQuote(int id, Quote& out)
	{
		std::lock_guard<std::mutex> lock(gMu);
		const auto it = gCache.find(id);
		if (it == gCache.end()) return false;
		if ((GetTickCount() - it->second.atMs) > kPriceTtlMs) return false;
		out = it->second.q;
		return true;
	}

	void PutQuotes(const std::unordered_map<int, Quote>& quotes)
	{
		const DWORD now = GetTickCount();
		std::lock_guard<std::mutex> lock(gMu);
		for (const auto& kv : quotes)
		{
			CacheEntry e;
			e.q = kv.second;
			e.atMs = now;
			gCache[kv.first] = e;
		}
	}

	void FetchQuotes(const std::vector<int>& ids, std::unordered_map<int, Quote>& out, bool force)
	{
		out.clear();
		std::vector<int> need;
		need.reserve(ids.size());
		{
			std::lock_guard<std::mutex> lock(gMu);
			const DWORD now = GetTickCount();
			for (int id : ids)
			{
				if (id <= 0) continue;
				if (!force)
				{
					const auto it = gCache.find(id);
					if (it != gCache.end() && (now - it->second.atMs) <= kPriceTtlMs)
					{
						out[id] = it->second.q;
						continue;
					}
				}
				need.push_back(id);
			}
		}

		for (size_t off = 0; off < need.size(); off += static_cast<size_t>(kBulkChunk))
		{
			const size_t n = (std::min)(need.size() - off, static_cast<size_t>(kBulkChunk));
			std::string path = "/v2/commerce/prices?ids=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) path += ',';
				path += std::to_string(need[off + i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, 12000);
			if (!r.ok) continue;
			std::unordered_map<int, Quote> batch;
			ParsePricesBody(r.body, batch);
			PutQuotes(batch);
			for (auto& kv : batch)
				out[kv.first] = std::move(kv.second);
		}
	}

	void FetchSellBuyMaps(const std::vector<int>& ids,
		std::unordered_map<int, long long>& sells,
		std::unordered_map<int, long long>* buys,
		bool force)
	{
		std::unordered_map<int, Quote> quotes;
		FetchQuotes(ids, quotes, force);
		for (const auto& kv : quotes)
		{
			if (kv.second.sell > 0)
				sells[kv.first] = kv.second.sell;
			if (buys && kv.second.buy > 0)
				(*buys)[kv.first] = kv.second.buy;
		}
	}
}
