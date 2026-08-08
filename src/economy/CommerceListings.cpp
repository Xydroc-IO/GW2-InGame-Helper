#include "CommerceShared.h"

#include "Gw2Http.h"
#include "JsonView.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Commerce
{
	namespace
	{
		std::mutex gListMu;
		std::unordered_map<int, ListingBook> gListCache;
		std::unordered_set<int> gListPending;

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

		void ParseLevels(const std::string& body, size_t arrStart, size_t until,
			std::vector<ListingLevel>& out, size_t maxN)
		{
			out.clear();
			size_t p = arrStart;
			while (p < until && out.size() < maxN)
			{
				const size_t brace = body.find('{', p);
				if (brace == std::string::npos || brace >= until) break;
				const size_t end = JsonObjectEnd(body, brace);
				if (end == std::string::npos || end >= until) break;
				ListingLevel lv{};
				const long long up = JsonView::IntAfterKey(body, "unit_price", brace);
				const long long qty = JsonView::IntAfterKey(body, "quantity", brace);
				if (up >= 0) lv.unitPrice = up;
				if (qty >= 0) lv.quantity = static_cast<int>(qty);
				if (lv.unitPrice > 0 || lv.quantity > 0)
					out.push_back(lv);
				p = end + 1;
			}
		}

		void ParseListingObj(const std::string& body, size_t brace, size_t end, ListingBook& book)
		{
			book = {};
			const long long id = JsonView::IntAfterKey(body, "id", brace);
			if (id <= 0) return;
			book.id = static_cast<int>(id);
			const size_t buysKey = body.find("\"buys\"", brace);
			const size_t sellsKey = body.find("\"sells\"", brace);
			if (buysKey != std::string::npos && buysKey < end)
			{
				const size_t arr = body.find('[', buysKey);
				const size_t stop = (sellsKey != std::string::npos && sellsKey < end) ? sellsKey : end;
				if (arr != std::string::npos && arr < stop)
					ParseLevels(body, arr, stop, book.buys, 8);
			}
			if (sellsKey != std::string::npos && sellsKey < end)
			{
				const size_t arr = body.find('[', sellsKey);
				if (arr != std::string::npos && arr < end)
					ParseLevels(body, arr, end, book.sells, 8);
			}
			book.ok = !book.buys.empty() || !book.sells.empty();
		}
	}

	void FetchListings(const std::vector<int>& ids, std::unordered_map<int, ListingBook>& out)
	{
		out.clear();
		std::vector<int> need;
		for (int id : ids)
			if (id > 0) need.push_back(id);
		for (size_t off = 0; off < need.size(); off += static_cast<size_t>(kBulkChunk))
		{
			const size_t n = (std::min)(need.size() - off, static_cast<size_t>(kBulkChunk));
			std::string path = "/v2/commerce/listings?ids=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) path += ',';
				path += std::to_string(need[off + i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, 12000);
			if (!r.ok) continue;
			size_t p = 0;
			while (p < r.body.size())
			{
				const size_t brace = r.body.find('{', p);
				if (brace == std::string::npos) break;
				const size_t end = JsonObjectEnd(r.body, brace);
				if (end == std::string::npos) break;
				ListingBook book;
				ParseListingObj(r.body, brace, end, book);
				if (book.id > 0)
					out[book.id] = std::move(book);
				p = end + 1;
			}
		}
		std::lock_guard<std::mutex> lock(gListMu);
		for (const auto& kv : out)
			gListCache[kv.first] = kv.second;
	}

	bool FetchListing(int id, ListingBook& out)
	{
		std::unordered_map<int, ListingBook> m;
		FetchListings({ id }, m);
		const auto it = m.find(id);
		if (it == m.end())
		{
			out = {};
			out.id = id;
			return false;
		}
		out = it->second;
		return out.ok;
	}

	void RequestListingAsync(int id)
	{
		if (id <= 0) return;
		{
			std::lock_guard<std::mutex> lock(gListMu);
			if (gListCache.count(id) || gListPending.count(id))
				return;
			gListPending.insert(id);
		}
		std::thread([id]() {
			ListingBook book;
			FetchListing(id, book);
			std::lock_guard<std::mutex> lock(gListMu);
			gListCache[id] = book;
			gListPending.erase(id);
		}).detach();
	}

	bool TryGetListing(int id, ListingBook& out)
	{
		std::lock_guard<std::mutex> lock(gListMu);
		const auto it = gListCache.find(id);
		if (it == gListCache.end()) return false;
		out = it->second;
		return true;
	}

	bool ListingBusy(int id)
	{
		std::lock_guard<std::mutex> lock(gListMu);
		return gListPending.count(id) != 0;
	}
}
