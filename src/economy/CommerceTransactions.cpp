#include "CommerceShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "JsonView.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Commerce
{
	namespace
	{
		std::mutex gMu;
		TxSnap gLast{};
		TxSnap gPending{};
		std::atomic<bool> gBusy{false};
		std::atomic<bool> gReady{false};

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

		void ParseTxArray(const std::string& body, std::vector<TxOrder>& out, size_t maxN)
		{
			out.clear();
			size_t p = 0;
			while (p < body.size() && out.size() < maxN)
			{
				const size_t brace = body.find('{', p);
				if (brace == std::string::npos) break;
				const size_t end = JsonObjectEnd(body, brace);
				if (end == std::string::npos) break;
				TxOrder o{};
				const long long tid = JsonView::IntAfterKey(body, "id", brace);
				const long long iid = JsonView::IntAfterKey(body, "item_id", brace);
				const long long qty = JsonView::IntAfterKey(body, "quantity", brace);
				const long long price = JsonView::IntAfterKey(body, "price", brace);
				if (tid > 0) o.txId = tid;
				if (iid > 0) o.itemId = static_cast<int>(iid);
				if (qty > 0) o.quantity = static_cast<int>(qty);
				if (price >= 0) o.price = price;
				if (o.itemId > 0)
					out.push_back(o);
				p = end + 1;
			}
		}

		void ResolveNames(std::vector<TxOrder>& orders)
		{
			std::vector<int> ids;
			for (const TxOrder& o : orders)
				if (o.itemId > 0) ids.push_back(o.itemId);
			if (ids.empty()) return;
			std::sort(ids.begin(), ids.end());
			ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
			std::string path = "/v2/items?ids=";
			for (size_t i = 0; i < ids.size() && i < 200; ++i)
			{
				if (i) path += ',';
				path += std::to_string(ids[i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, 8000);
			if (!r.ok) return;
			size_t p = 0;
			while (p < r.body.size())
			{
				const size_t brace = r.body.find('{', p);
				if (brace == std::string::npos) break;
				const size_t end = JsonObjectEnd(r.body, brace);
				if (end == std::string::npos) break;
				const long long id = JsonView::IntAfterKey(r.body, "id", brace);
				const std::string name = JsonView::StringAfterKey(r.body, "name", brace);
				if (id > 0 && !name.empty())
				{
					for (TxOrder& o : orders)
					{
						if (o.itemId == static_cast<int>(id))
							std::snprintf(o.name, sizeof(o.name), "%s", name.c_str());
					}
				}
				p = end + 1;
			}
		}

		bool FetchSide(const char* path, std::vector<TxOrder>& out, TxSnap& snap)
		{
			auto r = Gw2Http::Api(path, G::Gw2ApiKey, 8000);
			if (!r.ok)
			{
				if (r.status == 401 || r.status == 403)
				{
					snap.scopeFail = true;
					snap.status = "Need tradingpost scope for open orders.";
				}
				else if (snap.status.empty())
					snap.status = "Orders fetch failed.";
				return false;
			}
			ParseTxArray(r.body, out, 80);
			return true;
		}
	}

	void FetchTransactions(TxSnap& out)
	{
		out = {};
		if (!G::Gw2ApiKey[0])
		{
			out.noKey = true;
			out.status = "Add API key with tradingpost for open orders.";
			return;
		}
		FetchSide("/v2/commerce/transactions/current/buys", out.currentBuys, out);
		if (out.scopeFail) return;
		FetchSide("/v2/commerce/transactions/current/sells", out.currentSells, out);
		if (out.scopeFail) return;
		FetchSide("/v2/commerce/transactions/history/buys", out.histBuys, out);
		FetchSide("/v2/commerce/transactions/history/sells", out.histSells, out);
		ResolveNames(out.currentBuys);
		ResolveNames(out.currentSells);
		ResolveNames(out.histBuys);
		ResolveNames(out.histSells);
		out.ok = true;
		out.status = "Orders updated.";
	}

	void StartTransactionsFetch()
	{
		if (gBusy.exchange(true)) return;
		std::thread([]() {
			TxSnap snap;
			FetchTransactions(snap);
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPending = std::move(snap);
				gReady = true;
			}
			gBusy = false;
		}).detach();
	}

	bool TransactionsBusy() { return gBusy.load(); }

	bool PollTransactions(TxSnap& out)
	{
		if (!gReady.load()) return false;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gReady.load()) return false;
		gLast = gPending;
		out = gLast;
		gReady = false;
		return true;
	}

	TxSnap CopyLastTransactions()
	{
		std::lock_guard<std::mutex> lock(gMu);
		return gLast;
	}
}
