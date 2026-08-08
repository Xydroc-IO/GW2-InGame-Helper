#include "EconomyShared.h"

#include "EconomyInternal.h"

#include "CommerceShared.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Gw2Icons.h"

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
	static std::atomic<bool> gWorkerRunning{false};
	static std::vector<FlipRow> gPending;
	static bool gHavePending = false;
	static char gPendingStatus[192] = {};

	/* Curated tradeable mats - id is the source of truth; names are fallbacks
	   until /v2/items fills them (API puts "name" before "id" in objects). */
	struct Seed { int id; const char* name; };
	/* Verified against /v2/items - fallback names only used if names fetch fails. */
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
		/* T6 rares + extras */
		{24295, "Vial of Powerful Blood"},
		{24358, "Ancient Bone"},
		{24357, "Vicious Fang"},
		{24351, "Vicious Claw"},
		{24289, "Armored Scale"},
		{24283, "Powerful Venom Sac"},
		{24299, "Intricate Totem"},
		{24300, "Elaborate Totem"},
		{24277, "Pile of Crystalline Dust"},
		{19721, "Glob of Ectoplasm"},
		{19976, "Mystic Coin"},
		{19925, "Obsidian Shard"},
		{49457, "Empyreal Fragment"},
		{46733, "Dragonite Ore"},
		{46731, "Blood Ruby"},
		{89103, "Difluorite Crystal"},
		{86069, "Inscribed Shard"},
		{79280, "Kralkatite Ore"},
		{87645, "Mist Crystal"},
		{21155, "Gift of Exploration"},
		{19663, "Bolt of Damask"},
		{19662, "Bolt of Silk"},
		{19746, "Cured Hardened Leather Square"},
		{19721, "Glob of Ectoplasm"},
		{43772, "Icy Runestone"},
		{68063, "Stabilizing Matrix"},
		{73248, "Cube of Stabilized Dark Energy"},
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

	const char* FallbackName(int id)
	{
		for (const auto& s : kSeeds)
			if (s.id == id)
				return s.name;
		return "Item";
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
			{
				byId[static_cast<int>(id)] = name;
				Gw2Icons::RememberIconFromJson(static_cast<int>(id), body.c_str(), brace, end + 1);
			}
			p = end + 1;
		}
		for (auto& r : rows)
		{
			const auto it = byId.find(r.id);
			if (it != byId.end())
				std::snprintf(r.name, sizeof(r.name), "%s", it->second.c_str());
		}
	}

	void EnsureSeed()
	{
		/* Default focus id only — Charts list is empty until the user pins. */
		if (gChartItemId == 0 && gChartIds.empty())
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
		std::snprintf(gStatus, sizeof(gStatus), "Scanning commerce prices...");
		std::thread([]() {
			std::vector<int> ids;
			{
				std::unordered_map<int, bool> seen;
				for (const auto& s : kSeeds)
				{
					if (seen.count(s.id)) continue;
					seen[s.id] = true;
					ids.push_back(s.id);
				}
			}
			std::vector<FlipRow> rows;
			std::vector<PriceSample> samples;
			char status[192] = {};

			std::unordered_map<int, Commerce::Quote> quotes;
			Commerce::FetchQuotes(ids, quotes, true);
			if (quotes.empty())
			{
				std::snprintf(status, sizeof(status), "Commerce scan returned no listings.");
			}
			else
			{
				for (const auto& kv : quotes)
				{
					const Commerce::Quote& q = kv.second;
					FlipRow row{};
					row.id = q.id;
					std::snprintf(row.name, sizeof(row.name), "%s", FallbackName(q.id));
					row.buy = q.buy;
					row.sell = q.sell;
					row.demand = q.demand;
					row.supply = q.supply;
					row.spread = ApproxFeeAdjustedSpread(q.buy, q.sell);
					rows.push_back(row);

					PriceSample s{};
					s.id = q.id;
					s.buy = q.buy;
					s.sell = q.sell;
					s.ts = static_cast<unsigned>(std::time(nullptr));
					samples.push_back(s);
				}

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
					char err[96];
					FormatHttpError(err, sizeof(err), "Item names", names);
					std::snprintf(status, sizeof(status),
						"Flip scan: %zu items. %s", rows.size(), err);
				}
			}

			AppendSamples(samples);
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPending = std::move(rows);
				std::snprintf(gPendingStatus, sizeof(gPendingStatus), "%s", status);
				gHavePending = true;
			}
			gWorkerRunning = false;
		}).detach();
	}

	void PollFlipWorker()
	{
		bool saveHist = false;
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
			saveHist = !gFlips.empty();
		}
		if (saveHist)
			SaveHistory();
	}
}
