#include "EconomyShared.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>

namespace EconomyDetail
{
	bool gFocus = false;
	bool gPlaceOnce = false;
	int gTab = 0;
	int gForceTab = -1;
	char gStatus[192] = {};
	char gFlipFilter[64] = {};
	bool gFlipBusy = false;
	bool gFlipDone = false;
	std::vector<FlipRow> gFlips;
	std::vector<PriceSample> gHistory;
	std::vector<CartItem> gCart;
	int gChartItemId = 19721; /* Vicious Fang default */

	static std::mutex gMu;
	static std::atomic<bool> gWorkerRunning{false};
	static std::vector<FlipRow> gPending;
	static bool gHavePending = false;
	static char gPendingStatus[192] = {};

	/* Curated tradeable mats — id is the source of truth; names are fallbacks
	   until /v2/items fills them (API puts "name" before "id" in objects). */
	struct Seed { int id; const char* name; };
	/* Verified against /v2/items — fallback names only used if names fetch fails. */
	static const Seed kSeeds[] = {
		{24357, "Vicious Fang"},
		{24356, "Large Fang"},
		{24355, "Sharp Fang"},
		{24354, "Fang"},
		{24351, "Vicious Claw"},
		{24350, "Large Claw"},
		{24349, "Sharp Claw"},
		{24348, "Claw"},
		{24358, "Ancient Bone"},
		{24341, "Large Bone"},
		{24345, "Heavy Bone"},
		{24344, "Bone"},
		{24289, "Armored Scale"},
		{24288, "Large Scale"},
		{24287, "Smooth Scale"},
		{24286, "Scale"},
		{19721, "Glob of Ectoplasm"},
		{24277, "Pile of Crystalline Dust"},
		{24276, "Pile of Incandescent Dust"},
		{24275, "Pile of Luminous Dust"},
		{24274, "Pile of Radiant Dust"},
		{24299, "Intricate Totem"},
		{24300, "Elaborate Totem"},
		{24298, "Totem"},
		{24295, "Vial of Powerful Blood"},
		{24294, "Vial of Potent Blood"},
		{24293, "Vial of Thick Blood"},
		{24292, "Vial of Blood"},
		{24283, "Powerful Venom Sac"},
		{24282, "Potent Venom Sac"},
		{24281, "Full Venom Sac"},
		{24280, "Venom Sac"},
		{19976, "Mystic Coin"},
		{19925, "Obsidian Shard"},
		{46747, "Thermocatalytic Reagent"},
		{19711, "Hard Wood Plank"},
		{19714, "Seasoned Wood Plank"},
		{19713, "Soft Wood Plank"},
		{19709, "Elder Wood Plank"},
		{19712, "Ancient Wood Plank"},
		{19701, "Orichalcum Ore"},
		{19700, "Mithril Ore"},
		{19702, "Platinum Ore"},
		{19698, "Gold Ore"},
		{19685, "Orichalcum Ingot"},
		{19684, "Mithril Ingot"},
		{19686, "Platinum Ingot"},
		{19682, "Gold Ingot"},
		{77378, "Magnetite Shard"},
		{79061, "Unbound Magic"},
		{19732, "Hardened Leather Section"},
		{19729, "Thick Leather Section"},
		{19731, "Rugged Leather Section"},
		{19745, "Gossamer Scrap"},
		{19748, "Silk Scrap"},
		{19743, "Linen Scrap"},
	};

	static size_t JsonObjectEnd(const std::string& json, size_t openBrace)
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
				if (depth == 0)
					return i;
			}
		}
		return std::string::npos;
	}

	static std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from, size_t until)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos || k >= until)
			return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos || k >= until)
			return {};
		++k;
		while (k < until && (json[k] == ' ' || json[k] == '\t'))
			++k;
		if (k >= until || json[k] != '"')
			return {};
		++k;
		std::string out;
		while (k < until)
		{
			char c = json[k++];
			if (c == '\\' && k < until)
			{
				out.push_back(json[k++]);
				continue;
			}
			if (c == '"')
				break;
			out.push_back(c);
		}
		return out;
	}

	static long long JsonIntAfterKey(const std::string& json, const char* key, size_t from, size_t until)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos || k >= until)
			return -1;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos || k >= until)
			return -1;
		++k;
		while (k < until && (json[k] == ' ' || json[k] == '\t'))
			++k;
		bool neg = false;
		if (k < until && json[k] == '-')
		{
			neg = true;
			++k;
		}
		long long v = 0;
		bool any = false;
		while (k < until && json[k] >= '0' && json[k] <= '9')
		{
			any = true;
			v = v * 10 + (json[k] - '0');
			++k;
		}
		if (!any)
			return -1;
		return neg ? -v : v;
	}

	static const char* FallbackName(int id)
	{
		for (const auto& s : kSeeds)
			if (s.id == id)
				return s.name;
		return "Item";
	}

	static std::string BuildIdsQuery()
	{
		std::unordered_map<int, bool> seen;
		std::string q;
		for (const auto& s : kSeeds)
		{
			if (seen.count(s.id))
				continue;
			seen[s.id] = true;
			if (!q.empty())
				q += ',';
			q += std::to_string(s.id);
		}
		return q;
	}

	static void ApplyItemNames(std::vector<FlipRow>& rows, const std::string& body)
	{
		std::unordered_map<int, std::string> byId;
		size_t p = 0;
		while (p < body.size())
		{
			const size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			const size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos)
				break;
			const long long id = JsonIntAfterKey(body, "id", brace, end + 1);
			const std::string name = JsonStringAfterKey(body, "name", brace, end + 1);
			if (id > 0 && !name.empty())
				byId[static_cast<int>(id)] = name;
			p = end + 1;
		}
		for (auto& r : rows)
		{
			const auto it = byId.find(r.id);
			if (it != byId.end())
				std::snprintf(r.name, sizeof(r.name), "%s", it->second.c_str());
		}
	}

	std::string FormatCoins(long long copper)
	{
		if (copper < 0) copper = 0;
		const long long g = copper / 10000;
		const long long s = (copper % 10000) / 100;
		const long long c = copper % 100;
		char buf[64];
		if (g > 0)
			std::snprintf(buf, sizeof(buf), "%lldg %02llds %02lldc", g, s, c);
		else if (s > 0)
			std::snprintf(buf, sizeof(buf), "%llds %02lldc", s, c);
		else
			std::snprintf(buf, sizeof(buf), "%lldc", c);
		return buf;
	}

	void EnsureSeed()
	{
		if (gChartItemId == 0)
			gChartItemId = 19721;
	}

	static long long ApproxFeeAdjustedSpread(long long buy, long long sell)
	{
		/* Listing 5% + exchange 10% on sell ≈ 15% of sell. */
		const long long net = sell - (sell * 15) / 100;
		return net - buy;
	}

	static void FormatHttpError(char* out, size_t outLen, const char* what, const Gw2Http::Result& res)
	{
		if (!res.error.empty())
			std::snprintf(out, outLen, "%s failed: %s (HTTP %u).", what, res.error.c_str(), res.status);
		else if (res.status != 0)
			std::snprintf(out, outLen, "%s failed (HTTP %u). Try again.", what, res.status);
		else
			std::snprintf(out, outLen, "%s failed. Check network / try again.", what);
	}

	void RequestFlipScan()
	{
		if (gWorkerRunning.exchange(true))
			return;
		gFlipBusy = true;
		std::snprintf(gStatus, sizeof(gStatus), "Scanning commerce prices…");
		std::thread([]() {
			const std::string ids = BuildIdsQuery();
			std::vector<FlipRow> rows;
			std::vector<PriceSample> samples;
			char status[192] = {};

			auto prices = Gw2Http::Api(("/v2/commerce/prices?ids=" + ids).c_str(), nullptr, 12000);
			if (!prices.ok)
			{
				FormatHttpError(status, sizeof(status), "Commerce scan", prices);
			}
			else if (prices.body.empty() || prices.body == "[]")
			{
				std::snprintf(status, sizeof(status), "Commerce scan returned no listings.");
			}
			else
			{
				size_t p = 0;
				while (p < prices.body.size())
				{
					const size_t brace = prices.body.find('{', p);
					if (brace == std::string::npos)
						break;
					const size_t end = JsonObjectEnd(prices.body, brace);
					if (end == std::string::npos)
						break;

					const long long idLL = JsonIntAfterKey(prices.body, "id", brace, end + 1);
					if (idLL <= 0)
					{
						p = end + 1;
						continue;
					}
					const int id = static_cast<int>(idLL);

					long long buy = 0, sell = 0;
					int demand = 0, supply = 0;
					const size_t buys = prices.body.find("\"buys\"", brace);
					const size_t sells = prices.body.find("\"sells\"", brace);
					if (buys != std::string::npos && buys < end)
					{
						const long long up = JsonIntAfterKey(prices.body, "unit_price", buys, end + 1);
						const long long qty = JsonIntAfterKey(prices.body, "quantity", buys,
							(sells != std::string::npos && sells < end) ? sells : end + 1);
						if (up >= 0) buy = up;
						if (qty >= 0) demand = static_cast<int>(qty);
					}
					if (sells != std::string::npos && sells < end)
					{
						const long long up = JsonIntAfterKey(prices.body, "unit_price", sells, end + 1);
						const long long qty = JsonIntAfterKey(prices.body, "quantity", sells, end + 1);
						if (up >= 0) sell = up;
						if (qty >= 0) supply = static_cast<int>(qty);
					}

					FlipRow row{};
					row.id = id;
					std::snprintf(row.name, sizeof(row.name), "%s", FallbackName(id));
					row.buy = buy;
					row.sell = sell;
					row.demand = demand;
					row.supply = supply;
					row.spread = ApproxFeeAdjustedSpread(buy, sell);
					rows.push_back(row);

					PriceSample s{};
					s.id = id;
					s.buy = buy;
					s.sell = sell;
					s.ts = static_cast<unsigned>(std::time(nullptr));
					samples.push_back(s);
					p = end + 1;
				}

				if (rows.empty())
				{
					std::snprintf(status, sizeof(status),
						"Commerce scan parsed 0 items — API format changed?");
				}
				else
				{
					/* Names from returned price ids only (smaller + always valid). */
					std::string nameIds;
					for (size_t i = 0; i < rows.size(); ++i)
					{
						if (i) nameIds += ',';
						nameIds += std::to_string(rows[i].id);
					}
					auto names = Gw2Http::Api(("/v2/items?ids=" + nameIds).c_str(), nullptr, 12000);
					if (names.ok && !names.body.empty())
					{
						ApplyItemNames(rows, names.body);
						std::snprintf(status, sizeof(status),
							"Flip scan: %zu items (names from API).", rows.size());
					}
					else
					{
						/* Prices still useful — keep fallback names, say why. */
						char err[96];
						FormatHttpError(err, sizeof(err), "Item names", names);
						std::snprintf(status, sizeof(status),
							"Flip scan: %zu items. %s", rows.size(), err);
					}
				}
			}

			{
				std::lock_guard<std::mutex> lock(gMu);
				gPending = std::move(rows);
				for (const auto& s : samples)
					gHistory.push_back(s);
				if (gHistory.size() > 800)
					gHistory.erase(gHistory.begin(), gHistory.begin() + 100);
				std::snprintf(gPendingStatus, sizeof(gPendingStatus), "%s", status);
				gHavePending = true;
			}
			gWorkerRunning = false;
		}).detach();
	}

	void PollFlipWorker()
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!gHavePending)
			return;
		gFlips = std::move(gPending);
		gHavePending = false;
		gFlipBusy = false;
		gFlipDone = true;
		std::sort(gFlips.begin(), gFlips.end(),
			[](const FlipRow& a, const FlipRow& b) { return a.spread > b.spread; });
		std::snprintf(gStatus, sizeof(gStatus), "%s",
			gPendingStatus[0] ? gPendingStatus : "Flip scan done.");
		if (!gFlips.empty())
			SaveHistory();
	}

	void AddToCart(int id, const char* name, int qty)
	{
		for (auto& c : gCart)
		{
			if (c.id == id)
			{
				c.qty += qty;
				SaveCart();
				return;
			}
		}
		CartItem c{};
		c.id = id;
		c.qty = qty < 1 ? 1 : qty;
		std::snprintf(c.name, sizeof(c.name), "%s", name && name[0] ? name : FallbackName(id));
		gCart.push_back(c);
		SaveCart();
	}

	void RemoveCart(size_t idx)
	{
		if (idx < gCart.size())
		{
			gCart.erase(gCart.begin() + static_cast<std::ptrdiff_t>(idx));
			SaveCart();
		}
	}

	void ClearCart()
	{
		gCart.clear();
		SaveCart();
	}

	static std::wstring CartPath()
	{
		std::wstring dir = AddonPaths::ConfigDir();
		if (dir.empty()) dir = AddonPaths::DataDir();
		if (dir.empty()) return {};
		if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
		dir += L"economy-cart.txt";
		return dir;
	}

	static std::wstring HistPath()
	{
		std::wstring dir = AddonPaths::ConfigDir();
		if (dir.empty()) dir = AddonPaths::DataDir();
		if (dir.empty()) return {};
		if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
		dir += L"economy-prices.txt";
		return dir;
	}

	static bool WriteUtf8File(const std::wstring& path, const std::string& body)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, nullptr);
		CloseHandle(h);
		return ok != 0;
	}

	static bool ReadUtf8File(const std::wstring& path, std::string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 4 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		out.resize(static_cast<size_t>(sz.QuadPart));
		DWORD got = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok) { out.clear(); return false; }
		out.resize(got);
		return true;
	}

	void SaveCart()
	{
		const std::wstring path = CartPath();
		if (path.empty()) return;
		std::string body;
		for (const auto& c : gCart)
			body += std::to_string(c.id) + "\t" + std::to_string(c.qty) + "\t" + c.name + "\n";
		WriteUtf8File(path, body);
	}

	void LoadCart()
	{
		gCart.clear();
		const std::wstring path = CartPath();
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			CartItem c{};
			int qty = 1;
			char name[96]{};
			if (std::sscanf(line.c_str(), "%d\t%d\t%95[^\n]", &c.id, &qty, name) >= 2)
			{
				c.qty = qty;
				std::snprintf(c.name, sizeof(c.name), "%s", name[0] ? name : FallbackName(c.id));
				gCart.push_back(c);
			}
		}
	}

	void RecordSample(int id, long long buy, long long sell)
	{
		std::lock_guard<std::mutex> lock(gMu);
		PriceSample s{};
		s.id = id;
		s.buy = buy;
		s.sell = sell;
		s.ts = static_cast<unsigned>(std::time(nullptr));
		gHistory.push_back(s);
		if (gHistory.size() > 400)
			gHistory.erase(gHistory.begin(), gHistory.begin() + 50);
	}

	void SaveHistory()
	{
		const std::wstring path = HistPath();
		if (path.empty()) return;
		std::string body;
		const size_t start = gHistory.size() > 200 ? gHistory.size() - 200 : 0;
		for (size_t i = start; i < gHistory.size(); ++i)
		{
			const auto& s = gHistory[i];
			body += std::to_string(s.id) + "\t" + std::to_string(s.buy) + "\t" +
				std::to_string(s.sell) + "\t" + std::to_string(s.ts) + "\n";
		}
		WriteUtf8File(path, body);
	}

	void LoadHistory()
	{
		gHistory.clear();
		const std::wstring path = HistPath();
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			PriceSample s{};
			if (std::sscanf(line.c_str(), "%d\t%lld\t%lld\t%u", &s.id, &s.buy, &s.sell, &s.ts) >= 3)
				gHistory.push_back(s);
		}
	}
}
