#include "TpWatchPad.h"

#include "TpWatchShared.h"

#include "CommerceShared.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Gw2Icons.h"
#include "Settings.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <windows.h>

namespace TpWatchDetail
{
	size_t JsonObjectEnd(const std::string& json, size_t openBrace)
	{
		if (openBrace >= json.size() || json[openBrace] != '{')
			return std::string::npos;
		int depth = 0;
		bool inStr = false, esc = false;
		for (size_t i = openBrace; i < json.size(); ++i)
		{
			char c = json[i];
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

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return {};
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		if (k >= json.size() || json[k] != '"') return {};
		++k;
		std::string out;
		while (k < json.size())
		{
			char c = json[k++];
			if (c == '\\' && k < json.size()) { out.push_back(json[k++]); continue; }
			if (c == '"') break;
			out.push_back(c);
		}
		return out;
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return -1;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return -1;
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		bool neg = false;
		if (k < json.size() && json[k] == '-') { neg = true; ++k; }
		long long v = 0;
		bool any = false;
		while (k < json.size() && json[k] >= '0' && json[k] <= '9')
		{
			any = true;
			v = v * 10 + (json[k] - '0');
			++k;
		}
		if (!any) return -1;
		return neg ? -v : v;
	}

	std::string IdsQuery(const std::vector<int>& ids)
	{
		std::string q;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) q += ',';
			q += std::to_string(ids[i]);
		}
		return q;
	}

	void ResolveItemNames(const std::vector<int>& ids,
		std::vector<std::pair<int, std::string>>& outNames)
	{
		outNames.clear();
		if (ids.empty()) return;
		std::string path = "/v2/items?ids=";
		path += IdsQuery(ids);
		auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
		if (!r.ok) return;
		size_t p = 0;
		while (p < r.body.size())
		{
			size_t brace = r.body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(r.body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(r.body, "id", brace);
			std::string name = JsonStringAfterKey(r.body, "name", brace);
			if (id > 0 && !name.empty())
			{
				outNames.emplace_back(static_cast<int>(id), std::move(name));
				Gw2Icons::RememberIconFromJson(static_cast<int>(id), r.body.c_str(), brace, end);
			}
			p = end + 1;
		}
	}

	void FillNameHitPrices(std::vector<NameHit>& hits)
	{
		if (hits.empty()) return;
		std::vector<int> ids;
		ids.reserve(hits.size());
		for (const NameHit& h : hits) ids.push_back(h.id);

		std::vector<std::pair<int, std::string>> names;
		ResolveItemNames(ids, names);
		for (const auto& nv : names)
		{
			for (NameHit& h : hits)
				if (h.id == nv.first) { h.name = nv.second; break; }
		}

		std::unordered_map<int, Commerce::Quote> quotes;
		Commerce::FetchQuotes(ids, quotes, false);
		for (NameHit& h : hits)
		{
			const auto it = quotes.find(h.id);
			if (it == quotes.end()) continue;
			h.buy = it->second.buy;
			h.sell = it->second.sell;
			h.hasPrices = it->second.ok;
		}
	}

	void FetchInto(std::vector<Row>& rows)
	{
		if (rows.empty()) return;
		std::vector<int> ids;
		ids.reserve(rows.size());
		for (const Row& r : rows) ids.push_back(r.id);

		{
			std::vector<std::pair<int, std::string>> names;
			ResolveItemNames(ids, names);
			for (const auto& nv : names)
			{
				for (Row& row : rows)
					if (row.id == nv.first) { row.name = nv.second; break; }
			}
		}
		{
			std::unordered_map<int, Commerce::Quote> quotes;
			Commerce::FetchQuotes(ids, quotes, false);
			for (Row& row : rows)
			{
				const auto it = quotes.find(row.id);
				if (it == quotes.end()) continue;
				row.buy = it->second.buy;
				row.sell = it->second.sell;
			}
		}
	}

	void FetchDelivery(DeliverySnap& d)
	{
		d = DeliverySnap{};
		if (!G::Gw2ApiKey[0])
		{
			d.noKey = true;
			d.status = "Add API key with tradingpost for delivery.";
			return;
		}

		auto r = Gw2Http::Api("/v2/commerce/delivery", G::Gw2ApiKey, kHttpTimeoutMs);
		if (!r.ok)
		{
			if (r.status == 401 || r.status == 403)
			{
				d.scopeFail = true;
				d.status = "Need tradingpost scope on API key.";
			}
			else
				d.status = "Delivery unavailable.";
			return;
		}

		d.ok = true;
		const long long coins = JsonIntAfterKey(r.body, "coins", 0);
		d.coins = coins > 0 ? coins : 0;

		size_t itemsKey = r.body.find("\"items\"");
		if (itemsKey != std::string::npos)
		{
			size_t arr = r.body.find('[', itemsKey);
			size_t arrEnd = (arr != std::string::npos) ? r.body.find(']', arr) : std::string::npos;
			if (arr != std::string::npos && arrEnd != std::string::npos)
			{
				size_t p = arr;
				while (p < arrEnd && d.items.size() < static_cast<size_t>(kMaxItems))
				{
					size_t brace = r.body.find('{', p);
					if (brace == std::string::npos || brace >= arrEnd) break;
					size_t end = JsonObjectEnd(r.body, brace);
					if (end == std::string::npos || end > arrEnd) break;
					long long id = JsonIntAfterKey(r.body, "id", brace);
					long long count = JsonIntAfterKey(r.body, "count", brace);
					if (id > 0 && count > 0)
					{
						DeliveryItem it;
						it.id = static_cast<int>(id);
						it.count = static_cast<int>(count);
						d.items.push_back(std::move(it));
					}
					p = end + 1;
				}
			}
		}

		if (!d.items.empty())
		{
			std::vector<int> ids;
			ids.reserve(d.items.size());
			for (const DeliveryItem& it : d.items) ids.push_back(it.id);
			std::vector<std::pair<int, std::string>> names;
			ResolveItemNames(ids, names);
			for (const auto& nv : names)
			{
				for (DeliveryItem& it : d.items)
					if (it.id == nv.first) { it.name = nv.second; break; }
			}
			std::unordered_map<int, Commerce::Quote> quotes;
			Commerce::FetchQuotes(ids, quotes, false);
			d.itemsSellValue = 0;
			for (DeliveryItem& it : d.items)
			{
				auto qit = quotes.find(it.id);
				if (qit != quotes.end() && qit->second.sell > 0)
				{
					it.sellUnit = qit->second.sell;
					d.itemsSellValue += qit->second.sell * it.count;
				}
			}
		}

		if (d.coins == 0 && d.items.empty())
			d.status = "Nothing waiting to claim.";
		else if (d.items.empty())
			d.status = "Coins only - claim in-game at the Trading Post.";
		else
			d.status = "Claim in-game at the Trading Post.";
	}

	DWORD WINAPI FetchProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		std::vector<int> ids;
		ParseIds(G::TpWatchIds, ids);
		std::vector<Row> rows;
		rows.reserve(ids.size());
		for (int id : ids)
		{
			Row r;
			r.id = id;
			rows.push_back(std::move(r));
		}
		FetchInto(rows);
		DeliverySnap delivery;
		FetchDelivery(delivery);
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPending = std::move(rows);
			gPendingDelivery = std::move(delivery);
			gResultReady = true;
			gBusy = false;
		}
		return 0;
	}

	void StartFetch()
	{
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		gStatus = "Refreshing...";
		gThread = CreateThread(nullptr, 0, FetchProc, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			gStatus = "Could not start refresh.";
		}
	}
} // namespace TpWatchDetail
