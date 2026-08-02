#include "CraftingData.h"

#include "Globals.h"
#include "Gw2Http.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kHttpTimeoutMs = 2000;
	constexpr int kBulkTimeoutMs = 5000;
	/* Deep enough for legendary → gift → sub-gift → mats. */
	constexpr int kMaxDepth = 5;
	constexpr DWORD kDailyTtlMs = 10 * 60 * 1000;

	struct IngNode
	{
		int itemId = 0;
		int need = 0;
		int have = 0;
		int depth = 0;
		long long buyUnit = -1; /* instant-buy (sells) unit; -1 = not on TP */
		std::string name;
		bool crafted = false;
		std::vector<IngNode> kids;
	};

	struct RecipeIng
	{
		int itemId = 0;
		int count = 0;
		std::string name; /* wiki/API hint so sub-gifts expand without an extra lookup */
	};

	struct RecipeCacheEntry
	{
		bool apiTried = false;
		bool wikiTried = false;
		bool ok = false;
		int outCount = 1;
		int recipeId = 0;
		std::vector<RecipeIng> ings;
		std::string source; /* "Crafting station" / "Mystic Forge" / … */
	};

	struct DailyRow
	{
		int id = 0;
		std::string name;
	};

	struct Plan
	{
		bool ok = false;
		std::string status;
		std::string outputName;
		int outputId = 0;
		int outputCount = 1;
		IngNode root;
		long long buyTotal = 0;
		int noTpMissing = 0; /* missing stacks with no commerce listing */
		std::string recipeSource; /* station vs mystic forge (wiki) */
		std::vector<std::string> nameHints;
	};

	/* Forward decls — helpers defined below. */
	std::string ToLowerCopy(std::string s);
	std::string FetchWikiWikitext(const char* title);
	int ResolveWikiTitleToItemId(const char* title, std::string* nameOut);
	bool LoadWikiRecipeForName(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut);
	void CollectLeafIds(const IngNode& n, std::vector<int>& ids);
	void ApplyPrices(IngNode& n, const std::unordered_map<int, long long>& sells,
		long long& buyTotal, int& noTpMissing);
	bool IsTerminalMaterial(const std::string& name);

	std::mutex gMu;
	Plan gPlan;
	Plan gPendingPlan;
	std::vector<DailyRow> gDailies;
	std::vector<DailyRow> gPendingDailies;
	std::string gDailyStatus;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gDailyBusy{false};
	std::atomic<bool> gReady{false};
	std::atomic<bool> gDailyReady{false};
	HANDLE gThread = nullptr;
	HANDLE gDailyThread = nullptr;
	char gQuery[192] = {};
	char gThreadQuery[192] = {};
	DWORD gDailyFetchedAt = 0;
	std::atomic<bool> gFocusTab{false};
	std::atomic<unsigned> gPlanGen{0};

	std::mutex gWikiMu;
	std::unordered_map<std::string, std::string> gWikiTextCache;
	std::mutex gRecipeCacheMu;

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

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0)
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
			if (c == '\\' && k < json.size())
			{
				char e = json[k++];
				if (e == 'n') out.push_back('\n');
				else if (e == 't') out.push_back('\t');
				else if (e == 'u' && k + 3 < json.size()) k += 4;
				else out.push_back(e);
				continue;
			}
			if (c == '"') break;
			out.push_back(c);
		}
		return out;
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0)
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

	std::string UrlEncode(const char* s)
	{
		std::string o;
		static const char* hex = "0123456789ABCDEF";
		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
		{
			unsigned char c = *p;
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
				o.push_back(static_cast<char>(c));
			else if (c == ' ')
				o.push_back('+');
			else
			{
				o.push_back('%');
				o.push_back(hex[c >> 4]);
				o.push_back(hex[c & 15]);
			}
		}
		return o;
	}

	int ParseItemId(const char* text)
	{
		if (!text || !text[0]) return 0;
		const char* a = std::strstr(text, "[&");
		if (a)
		{
			a += 2;
			const char* b = std::strchr(a, ']');
			if (b && b > a)
			{
				static const char kB64[] =
					"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				int buf = 0, bits = 0;
				unsigned char out[16]{};
				size_t n = 0;
				for (const char* p = a; p < b && n < sizeof(out); ++p)
				{
					if (*p == '=' || *p == ' ') break;
					const char* q = std::strchr(kB64, *p);
					if (!q) continue;
					buf = (buf << 6) | static_cast<int>(q - kB64);
					bits += 6;
					if (bits >= 8)
					{
						bits -= 8;
						out[n++] = static_cast<unsigned char>((buf >> bits) & 0xFF);
					}
				}
				if (n >= 5 && out[0] == 0x02)
				{
					const int id = out[2] | (out[3] << 8) | (out[4] << 16);
					if (id > 0) return id;
				}
			}
		}
		int id = 0;
		bool onlyDigits = true;
		for (const char* p = text; *p; ++p)
		{
			if (*p == ' ' || *p == '\t') continue;
			if (*p >= '0' && *p <= '9')
				id = id * 10 + (*p - '0');
			else { onlyDigits = false; break; }
		}
		return (onlyDigits && id > 0) ? id : 0;
	}

	/* Case-insensitive find of needle in hay (ASCII). */
	size_t FindCi(const std::string& hay, const char* needle, size_t from = 0)
	{
		if (!needle || !needle[0]) return std::string::npos;
		const size_t nlen = std::strlen(needle);
		if (from >= hay.size() || nlen > hay.size()) return std::string::npos;
		for (size_t i = from; i + nlen <= hay.size(); ++i)
		{
			bool ok = true;
			for (size_t j = 0; j < nlen; ++j)
			{
				const unsigned char a = static_cast<unsigned char>(hay[i + j]);
				const unsigned char b = static_cast<unsigned char>(needle[j]);
				if (std::tolower(a) != std::tolower(b)) { ok = false; break; }
			}
			if (ok) return i;
		}
		return std::string::npos;
	}

	void AppendIdsAfterEquals(const std::string& wt, size_t eqPos, std::vector<int>& out)
	{
		size_t k = eqPos + 1;
		while (k < wt.size() && (wt[k] == ' ' || wt[k] == '\t')) ++k;
		/* Support "| id = 91737, 92443" — take each integer. */
		while (k < wt.size())
		{
			int id = 0;
			bool any = false;
			while (k < wt.size() && wt[k] >= '0' && wt[k] <= '9')
			{
				any = true;
				id = id * 10 + (wt[k] - '0');
				++k;
			}
			if (any && id > 0) out.push_back(id);
			while (k < wt.size() && (wt[k] == ' ' || wt[k] == '\t')) ++k;
			if (k < wt.size() && wt[k] == ',')
			{
				++k;
				while (k < wt.size() && (wt[k] == ' ' || wt[k] == '\t')) ++k;
				continue;
			}
			break;
		}
	}

	void CollectIdsInRange(const std::string& wt, size_t from, size_t to, std::vector<int>& out)
	{
		if (to > wt.size()) to = wt.size();
		size_t p = from;
		while (p < to)
		{
			size_t bar = wt.find('|', p);
			if (bar == std::string::npos || bar >= to) break;
			size_t k = bar + 1;
			while (k < to && (wt[k] == ' ' || wt[k] == '\t')) ++k;
			if (k + 2 < to &&
				(wt[k] == 'i' || wt[k] == 'I') &&
				(wt[k + 1] == 'd' || wt[k + 1] == 'D'))
			{
				const char after = (k + 2 < to) ? wt[k + 2] : 0;
				if (after == ' ' || after == '\t' || after == '=')
				{
					while (k < to && wt[k] != '=') ++k;
					if (k < to && wt[k] == '=')
						AppendIdsAfterEquals(wt, k, out);
				}
			}
			p = bar + 1;
		}
	}

	/* Wiki pages put effect ids / recipe ids in | id = before the real item id.
	   Prefer item/weapon/armor infobox blocks; skip {{recipe}} (recipe ids). */
	std::vector<int> CollectWikiItemIdCandidates(const std::string& wikitext)
	{
		std::vector<int> preferred;
		std::vector<int> fallback;
		const char* boxTags[] = {
			"{{item infobox", "{{weapon infobox", "{{armor infobox",
			"{{trinket infobox", "{{back item infobox", "{{accessory infobox",
		};
		size_t box = std::string::npos;
		for (const char* tag : boxTags)
		{
			const size_t at = FindCi(wikitext, tag);
			if (at != std::string::npos && (box == std::string::npos || at < box))
				box = at;
		}
		if (box != std::string::npos)
		{
			size_t recipe = FindCi(wikitext, "{{recipe", box + 1);
			const size_t end = (recipe != std::string::npos) ? recipe : wikitext.size();
			CollectIdsInRange(wikitext, box, end, preferred);
		}
		/* Whole page except recipe templates. */
		size_t p = 0;
		while (p < wikitext.size())
		{
			size_t recipe = FindCi(wikitext, "{{recipe", p);
			if (recipe == std::string::npos)
			{
				CollectIdsInRange(wikitext, p, wikitext.size(), fallback);
				break;
			}
			CollectIdsInRange(wikitext, p, recipe, fallback);
			/* Skip until matching }} after {{recipe — simple depth scan. */
			size_t i = recipe + 2;
			int depth = 1;
			while (i + 1 < wikitext.size() && depth > 0)
			{
				if (wikitext[i] == '{' && wikitext[i + 1] == '{') { depth++; i += 2; continue; }
				if (wikitext[i] == '}' && wikitext[i + 1] == '}') { depth--; i += 2; continue; }
				++i;
			}
			p = i;
		}
		/* Dedup preferred first, then fallback. */
		std::vector<int> out;
		auto addAll = [&](const std::vector<int>& src) {
			for (int id : src)
			{
				bool dup = false;
				for (int x : out) if (x == id) { dup = true; break; }
				if (!dup) out.push_back(id);
			}
		};
		addAll(preferred);
		addAll(fallback);
		return out;
	}

	std::string ItemName(int id)
	{
		char path[64];
		std::snprintf(path, sizeof(path), "/v2/items/%d", id);
		auto r = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (!r.ok || r.body.empty() || r.body[0] != '{') return {};
		return JsonStringAfterKey(r.body, "name", 0);
	}

	void FetchNames(std::unordered_map<int, std::string>& names, const std::vector<int>& ids)
	{
		std::vector<int> need;
		for (int id : ids)
			if (names.find(id) == names.end()) need.push_back(id);
		for (size_t off = 0; off < need.size(); off += 200)
		{
			const size_t n = (std::min)(need.size() - off, size_t{200});
			std::string path = "/v2/items?ids=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) path += ',';
				path += std::to_string(need[off + i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kBulkTimeoutMs);
			if (!r.ok) continue;
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
					names[static_cast<int>(id)] = name;
				p = end + 1;
			}
		}
	}

	/* First candidate that exists on the official items API (one bulk request). */
	int FirstValidItemId(const std::vector<int>& candidates, std::string* nameOut)
	{
		if (candidates.empty()) return 0;
		std::unordered_map<int, std::string> names;
		FetchNames(names, candidates);
		for (int id : candidates)
		{
			auto it = names.find(id);
			if (it != names.end() && !it->second.empty())
			{
				if (nameOut) *nameOut = it->second;
				return id;
			}
		}
		return 0;
	}

	/* Instant-buy unit price = lowest sell listing (not buy-order). */
	void FetchPrices(std::unordered_map<int, long long>& sells, const std::vector<int>& ids)
	{
		for (size_t off = 0; off < ids.size(); off += 200)
		{
			const size_t n = (std::min)(ids.size() - off, size_t{200});
			std::string path = "/v2/commerce/prices?ids=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) path += ',';
				path += std::to_string(ids[off + i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kBulkTimeoutMs);
			if (!r.ok) continue;
			size_t p = 0;
			while (p < r.body.size())
			{
				size_t brace = r.body.find('{', p);
				if (brace == std::string::npos) break;
				size_t end = JsonObjectEnd(r.body, brace);
				if (end == std::string::npos) break;
				long long id = JsonIntAfterKey(r.body, "id", brace);
				size_t sellsKey = r.body.find("\"sells\"", brace);
				long long unit = -1;
				if (sellsKey != std::string::npos && sellsKey < end)
					unit = JsonIntAfterKey(r.body, "unit_price", sellsKey);
				if (id > 0 && unit >= 0)
					sells[static_cast<int>(id)] = unit;
				p = end + 1;
			}
		}
	}

	void AddOwnedCounts(std::unordered_map<int, int>& owned, const std::string& body)
	{
		size_t p = 0;
		while (p < body.size())
		{
			size_t brace = body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(body, "id", brace);
			long long cnt = JsonIntAfterKey(body, "count", brace);
			if (id > 0 && cnt > 0)
				owned[static_cast<int>(id)] += static_cast<int>(cnt);
			p = end + 1;
		}
	}

	struct OwnedJob
	{
		const char* path = nullptr;
		const char* key = nullptr;
		Gw2Http::Result result;
	};

	DWORD WINAPI OwnedProc(void* param)
	{
		auto* j = static_cast<OwnedJob*>(param);
		j->result = Gw2Http::Api(j->path, j->key, kHttpTimeoutMs);
		return 0;
	}

	void LoadOwned(std::unordered_map<int, int>& owned)
	{
		if (!G::Gw2ApiKey[0]) return;
		const char* key = G::Gw2ApiKey;
		OwnedJob jobs[3] = {
			{ "/v2/account/materials", key, {} },
			{ "/v2/account/bank", key, {} },
			{ "/v2/account/inventory", key, {} },
		};
		HANDLE hs[3]{};
		for (int i = 0; i < 3; ++i)
			hs[i] = CreateThread(nullptr, 0, OwnedProc, &jobs[i], 0, nullptr);
		if (hs[0] && hs[1] && hs[2])
			WaitForMultipleObjects(3, hs, TRUE, 12000);
		for (int i = 0; i < 3; ++i)
		{
			if (hs[i])
			{
				WaitForSingleObject(hs[i], 0);
				CloseHandle(hs[i]);
			}
			if (jobs[i].result.ok)
				AddOwnedCounts(owned, jobs[i].result.body);
		}
	}

	bool LoadApiRecipeForOutput(int outputId, int& outCount, std::vector<RecipeIng>& ings, int& recipeId)
	{
		char path[80];
		std::snprintf(path, sizeof(path), "/v2/recipes/search?output=%d", outputId);
		auto sr = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (!sr.ok || sr.body.empty()) return false;
		recipeId = 0;
		size_t p = 0;
		while (p < sr.body.size())
		{
			while (p < sr.body.size() && (sr.body[p] < '0' || sr.body[p] > '9')) ++p;
			if (p >= sr.body.size()) break;
			int v = 0;
			while (p < sr.body.size() && sr.body[p] >= '0' && sr.body[p] <= '9')
			{
				v = v * 10 + (sr.body[p] - '0');
				++p;
			}
			if (v > 0) { recipeId = v; break; }
		}
		if (recipeId <= 0) return false;
		std::snprintf(path, sizeof(path), "/v2/recipes/%d", recipeId);
		auto rr = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (!rr.ok) return false;
		outCount = static_cast<int>(JsonIntAfterKey(rr.body, "output_item_count", 0));
		if (outCount <= 0) outCount = 1;
		ings.clear();
		size_t ingKey = rr.body.find("\"ingredients\"");
		if (ingKey == std::string::npos) return false;
		size_t arr = rr.body.find('[', ingKey);
		if (arr == std::string::npos) return false;
		p = arr;
		while (p < rr.body.size())
		{
			size_t brace = rr.body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(rr.body, brace);
			if (end == std::string::npos) break;
			long long iid = JsonIntAfterKey(rr.body, "item_id", brace);
			long long cnt = JsonIntAfterKey(rr.body, "count", brace);
			if (iid > 0 && cnt > 0)
			{
				RecipeIng ri;
				ri.itemId = static_cast<int>(iid);
				ri.count = static_cast<int>(cnt);
				ings.push_back(ri);
			}
			p = end + 1;
			const size_t nextBrace = rr.body.find('{', p);
			const size_t nextClose = rr.body.find(']', p);
			if (nextClose != std::string::npos &&
				(nextBrace == std::string::npos || nextClose < nextBrace))
				break;
		}
		return !ings.empty();
	}

	/* TP / gather mats — buy these; do not chase promotion ladders (ore/dust/T6 blood…). */
	bool IsTerminalMaterial(const std::string& name)
	{
		if (name.empty()) return false;
		const std::string n = ToLowerCopy(name);
		if (n.rfind("gift of ", 0) == 0) return false;
		if (n.rfind("recipe:", 0) == 0) return false;
		if (n.find("legendary") != std::string::npos) return false;
		auto has = [&](const char* s) {
			return n.find(s) != std::string::npos;
		};
		if (has(" ore") || has(" log") || has(" scrap")) return true;
		if (has("pile of ") || has("glob of ectoplasm") || has("obsidian shard")) return true;
		if (has(" dust")) return true;
		if (has("vial of ") || has("venom sac")) return true;
		if (has("blood") && !has("gift")) return true; /* T5/T6 blood, bloodstone */
		if (has("fang") || has("claw") || has("scale") || has("bone") ||
			has("totem"))
			return true;
		if (has("crystal") && !has("gift")) return true;
		if (has("lodestone") || has("mystic coin") || has("philosopher") ||
			has("icy runestone") || has("spirit shard") || has("karma") ||
			has("tale of dungeon") || has("badge of honor"))
			return true;
		if (has("quartz crystal") || has("thermocatalytic") || has("spool of ") ||
			has("lump of ") || has("glob of elder"))
			return true;
		return false;
	}

	/* Station API first; wiki forge/gifts when API has none. Never wiki-expand raw mats. */
	bool TryLoadRecipe(int outputId, const std::string& nameHint, int& outCount,
		std::vector<RecipeIng>& ings, int& recipeId,
		std::unordered_map<int, RecipeCacheEntry>& cache, std::string* sourceOut)
	{
		std::unique_lock<std::mutex> cacheLock(gRecipeCacheMu);
		RecipeCacheEntry& e = cache[outputId];
		if (e.ok)
		{
			outCount = e.outCount;
			ings = e.ings;
			recipeId = e.recipeId;
			if (sourceOut) *sourceOut = e.source;
			return true;
		}

		if (!e.apiTried)
		{
			e.apiTried = true;
			cacheLock.unlock();
			const bool apiOk = LoadApiRecipeForOutput(outputId, outCount, ings, recipeId);
			cacheLock.lock();
			RecipeCacheEntry& eApi = cache[outputId];
			if (apiOk)
			{
				eApi.ok = true;
				eApi.outCount = outCount;
				eApi.recipeId = recipeId;
				eApi.ings = ings;
				eApi.source = "Crafting station";
				if (sourceOut) *sourceOut = eApi.source;
				return true;
			}
		}

		RecipeCacheEntry& eWikiGate = cache[outputId];
		if (!eWikiGate.wikiTried && !nameHint.empty())
		{
			eWikiGate.wikiTried = true;
			if (IsTerminalMaterial(nameHint))
			{
				/* Ore/dust/T6 mats: wiki lists mystic-forge promotions — ignore. */
				return false;
			}
			std::string src;
			cacheLock.unlock();
			const bool wikiOk = LoadWikiRecipeForName(nameHint.c_str(), outCount, ings, &src);
			cacheLock.lock();
			RecipeCacheEntry& eWiki = cache[outputId];
			if (wikiOk)
			{
				eWiki.ok = true;
				eWiki.outCount = outCount;
				eWiki.recipeId = 0;
				eWiki.ings = ings;
				eWiki.source = src.empty() ? "Wiki recipe" : src;
				if (sourceOut) *sourceOut = eWiki.source;
				return true;
			}
		}

		RecipeCacheEntry& eEnd = cache[outputId];
		outCount = eEnd.outCount;
		ings = eEnd.ings;
		recipeId = eEnd.recipeId;
		return false;
	}

	void PublishLivePlan(const Plan& plan)
	{
		std::lock_guard<std::mutex> lock(gMu);
		gPlan = plan;
	}

	void FinishPrices(Plan& plan, std::unordered_map<int, std::string>& names)
	{
		std::vector<int> allIds;
		allIds.push_back(plan.outputId);
		CollectLeafIds(plan.root, allIds);
		for (const IngNode& k : plan.root.kids)
		{
			allIds.push_back(k.itemId);
			CollectLeafIds(k, allIds);
		}
		std::sort(allIds.begin(), allIds.end());
		allIds.erase(std::unique(allIds.begin(), allIds.end()), allIds.end());
		FetchNames(names, allIds);
		std::vector<IngNode*> stack;
		stack.push_back(&plan.root);
		while (!stack.empty())
		{
			IngNode* cur = stack.back();
			stack.pop_back();
			if (names.count(cur->itemId))
				cur->name = names[cur->itemId];
			for (IngNode& c : cur->kids) stack.push_back(&c);
		}

		std::vector<int> leafIds;
		CollectLeafIds(plan.root, leafIds);
		std::sort(leafIds.begin(), leafIds.end());
		leafIds.erase(std::unique(leafIds.begin(), leafIds.end()), leafIds.end());
		std::unordered_map<int, long long> sells;
		FetchPrices(sells, leafIds);
		plan.buyTotal = 0;
		plan.noTpMissing = 0;
		ApplyPrices(plan.root, sells, plan.buyTotal, plan.noTpMissing);

		char buf[192];
		const char* src = plan.recipeSource.empty() ? "recipe" : plan.recipeSource.c_str();
		if (plan.noTpMissing > 0)
		{
			std::snprintf(buf, sizeof(buf),
				"%s · TP buy ~ %s (+ bound/no-TP mats)",
				src, FormatCoins(plan.buyTotal).c_str());
		}
		else
		{
			std::snprintf(buf, sizeof(buf),
				"%s · TP buy ~ %s",
				src, FormatCoins(plan.buyTotal).c_str());
		}
		plan.status = buf;
	}

	/* Expand one frontier of leaves in parallel (keeps legendary gift trees snappy). */
	void ExpandFrontier(Plan& plan, std::unordered_map<int, int>& owned,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, RecipeCacheEntry>& recipeCache, int childDepth)
	{
		struct Front
		{
			IngNode* node = nullptr;
			bool ok = false;
			int outCount = 1;
			std::vector<RecipeIng> ings;
		};
		std::vector<Front> fronts;
		std::vector<IngNode*> stack;
		stack.push_back(&plan.root);
		while (!stack.empty())
		{
			IngNode* n = stack.back();
			stack.pop_back();
			if (!n->kids.empty())
			{
				for (IngNode& c : n->kids) stack.push_back(&c);
				continue;
			}
			if (n->depth != childDepth - 1) continue;
			if (n->have >= n->need) continue;
			if (IsTerminalMaterial(n->name)) continue;
			fronts.push_back({ n, false, 1, {} });
		}
		if (fronts.empty()) return;

		/* Cap parallelism — fewer threads, less crash surface under WinHTTP. */
		constexpr size_t kMaxParallel = 4;
		for (size_t off = 0; off < fronts.size(); off += kMaxParallel)
		{
			const size_t batch = (std::min)(fronts.size() - off, kMaxParallel);
			struct Job
			{
				Front* f = nullptr;
				std::unordered_map<int, RecipeCacheEntry>* cache = nullptr;
				static DWORD WINAPI Thunk(void* p)
				{
					auto* j = static_cast<Job*>(p);
					Front& f = *j->f;
					std::string hint = f.node->name;
					int recipeId = 0;
					f.ok = TryLoadRecipe(f.node->itemId, hint, f.outCount, f.ings, recipeId,
						*j->cache, nullptr);
					return 0;
				}
			};
			std::vector<Job> jobs(batch);
			std::vector<HANDLE> hs;
			hs.reserve(batch);
			for (size_t i = 0; i < batch; ++i)
			{
				jobs[i].f = &fronts[off + i];
				jobs[i].cache = &recipeCache;
				HANDLE h = CreateThread(nullptr, 0, Job::Thunk, &jobs[i], 0, nullptr);
				if (h) hs.push_back(h);
				else Job::Thunk(&jobs[i]);
			}
			if (!hs.empty())
				WaitForMultipleObjects(static_cast<DWORD>(hs.size()), hs.data(), TRUE, 60000);
			for (HANDLE h : hs)
			{
				WaitForSingleObject(h, 1000);
				CloseHandle(h);
			}
		}

		for (Front& f : fronts)
		{
			if (!f.ok || f.ings.empty()) continue;
			f.node->crafted = true;
			const int deficit = f.node->need - f.node->have;
			const int crafts = (deficit + f.outCount - 1) / f.outCount;
			for (const RecipeIng& ri : f.ings)
			{
				if (!ri.name.empty())
					names[ri.itemId] = ri.name;
				IngNode kid;
				kid.itemId = ri.itemId;
				kid.need = ri.count * crafts;
				kid.depth = childDepth;
				kid.name = ri.name;
				kid.have = owned.count(ri.itemId) ? owned[ri.itemId] : 0;
				if (kid.name.empty() && names.count(ri.itemId))
					kid.name = names[ri.itemId];
				f.node->kids.push_back(std::move(kid));
			}
		}
	}

	void BuildTree(IngNode& node, int depth, std::unordered_map<int, int>& owned,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, RecipeCacheEntry>& cache)
	{
		node.have = owned.count(node.itemId) ? owned[node.itemId] : 0;
		if (names.count(node.itemId))
			node.name = names[node.itemId];
		if (depth >= kMaxDepth) return;
		if (node.have >= node.need) return;
		if (IsTerminalMaterial(node.name)) return;

		std::string nameHint = node.name;
		if (nameHint.empty() && names.count(node.itemId))
			nameHint = names[node.itemId];
		if (nameHint.empty())
			nameHint = ItemName(node.itemId);
		if (!nameHint.empty())
			names[node.itemId] = nameHint;

		int outCount = 1;
		std::vector<RecipeIng> ings;
		int recipeId = 0;
		if (!TryLoadRecipe(node.itemId, nameHint, outCount, ings, recipeId, cache, nullptr))
			return;
		node.crafted = true;
		const int deficit = node.need - node.have;
		const int crafts = (deficit + outCount - 1) / outCount;
		for (const RecipeIng& ri : ings)
		{
			IngNode kid;
			kid.itemId = ri.itemId;
			kid.need = ri.count * crafts;
			kid.depth = depth + 1;
			if (!ri.name.empty())
			{
				kid.name = ri.name;
				names[ri.itemId] = ri.name;
			}
			BuildTree(kid, depth + 1, owned, names, cache);
			node.kids.push_back(std::move(kid));
		}
	}

	void CollectLeafIds(const IngNode& n, std::vector<int>& ids)
	{
		if (n.kids.empty())
		{
			ids.push_back(n.itemId);
			return;
		}
		for (const IngNode& k : n.kids)
			CollectLeafIds(k, ids);
	}

	void ApplyPrices(IngNode& n, const std::unordered_map<int, long long>& sells,
		long long& total, int& noTpMissing)
	{
		if (n.kids.empty())
		{
			const int miss = (std::max)(0, n.need - n.have);
			if (miss <= 0) return;
			auto it = sells.find(n.itemId);
			if (it != sells.end())
			{
				n.buyUnit = it->second;
				total += n.buyUnit * miss;
			}
			else
			{
				n.buyUnit = -1;
				noTpMissing += miss;
			}
			return;
		}
		for (IngNode& k : n.kids)
			ApplyPrices(k, sells, total, noTpMissing);
	}

	void DrawNode(const IngNode& n)
	{
		ImGui::PushID(n.itemId + n.depth * 1000003);
		const char* name = n.name.empty() ? "..." : n.name.c_str();
		const int miss = (std::max)(0, n.need - n.have);
		const bool ok = miss <= 0;
		char label[256];
		std::snprintf(label, sizeof(label), "%s  %d / %d", name, n.have, n.need);
		if (!n.kids.empty())
		{
			if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const IngNode& k : n.kids)
					DrawNode(k);
				ImGui::TreePop();
			}
		}
		else
		{
			if (ok)
				ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.f), "%s", label);
			else
				ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.40f, 1.f), "%s", label);
			if (miss > 0 && n.buyUnit >= 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
					"buy %s", FormatCoins(n.buyUnit * miss).c_str());
			}
			else if (miss > 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "no TP");
			}
		}
		ImGui::PopID();
	}

	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	std::string FetchWikiWikitext(const char* title)
	{
		if (!title || !title[0]) return {};
		const std::string key = ToLowerCopy(title);
		{
			std::lock_guard<std::mutex> lock(gWikiMu);
			auto it = gWikiTextCache.find(key);
			if (it != gWikiTextCache.end())
				return it->second;
		}
		std::string parseUrl =
			"https://wiki.guildwars2.com/api.php?action=parse&prop=wikitext&format=json"
			"&formatversion=2&page=";
		parseUrl += UrlEncode(title);
		auto pr = Gw2Http::Get(parseUrl.c_str(), nullptr, kHttpTimeoutMs);
		std::string wt;
		if (pr.ok)
		{
			wt = JsonStringAfterKey(pr.body, "wikitext", 0);
			if (wt.empty())
			{
				size_t wk = pr.body.find("\"wikitext\"");
				if (wk != std::string::npos)
					wt = JsonStringAfterKey(pr.body, "wikitext", wk);
			}
		}
		{
			std::lock_guard<std::mutex> lock(gWikiMu);
			gWikiTextCache[key] = wt;
		}
		return wt;
	}

	/* One MediaWiki query for many titles (legendary gift ingredients). */
	void FetchWikiWikitextBatch(const std::vector<std::string>& titles,
		std::unordered_map<std::string, std::string>& outByKey)
	{
		std::vector<std::string> miss;
		miss.reserve(titles.size());
		{
			std::lock_guard<std::mutex> lock(gWikiMu);
			for (const std::string& t : titles)
			{
				if (t.empty()) continue;
				const std::string key = ToLowerCopy(t);
				auto it = gWikiTextCache.find(key);
				if (it != gWikiTextCache.end())
					outByKey[key] = it->second;
				else
					miss.push_back(t);
			}
		}
		for (size_t off = 0; off < miss.size(); off += 8)
		{
			const size_t n = (std::min)(miss.size() - off, size_t{8});
			std::string url =
				"https://wiki.guildwars2.com/api.php?action=query&prop=revisions"
				"&rvprop=content&rvslots=main&format=json&formatversion=2&titles=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) url += '|';
				url += UrlEncode(miss[off + i].c_str());
			}
			auto pr = Gw2Http::Get(url.c_str(), nullptr, kBulkTimeoutMs);
			if (!pr.ok) continue;
			/* formatversion=2: pages:[{title,revisions:[{slots:{main:{content}}}]}] */
			size_t p = 0;
			while (p < pr.body.size())
			{
				size_t titleKey = pr.body.find("\"title\"", p);
				if (titleKey == std::string::npos) break;
				std::string title = JsonStringAfterKey(pr.body, "title", titleKey);
				size_t contentKey = pr.body.find("\"content\"", titleKey);
				size_t nextTitle = pr.body.find("\"title\"", titleKey + 7);
				std::string wt;
				if (contentKey != std::string::npos &&
					(nextTitle == std::string::npos || contentKey < nextTitle))
					wt = JsonStringAfterKey(pr.body, "content", contentKey);
				if (!title.empty())
				{
					const std::string key = ToLowerCopy(title);
					std::lock_guard<std::mutex> lock(gWikiMu);
					gWikiTextCache[key] = wt;
					outByKey[key] = wt;
				}
				p = titleKey + 7;
			}
			/* Alias requested spellings + fill empties for missing pages. */
			for (size_t i = 0; i < n; ++i)
			{
				const std::string reqKey = ToLowerCopy(miss[off + i]);
				if (outByKey.find(reqKey) != outByKey.end()) continue;
				outByKey[reqKey] = FetchWikiWikitext(miss[off + i].c_str());
			}
		}
	}

	void ApplyOwnedCounts(IngNode& n, const std::unordered_map<int, int>& owned)
	{
		auto it = owned.find(n.itemId);
		n.have = (it != owned.end()) ? it->second : 0;
		for (IngNode& k : n.kids)
			ApplyOwnedCounts(k, owned);
	}

	int ResolveWikiTitleToItemId(const char* title, std::string* nameOut)
	{
		const std::string wt = FetchWikiWikitext(title);
		if (wt.empty()) return 0;
		return FirstValidItemId(CollectWikiItemIdCandidates(wt), nameOut);
	}

	std::string CleanWikiLinkName(std::string s)
	{
		/* Trim + unwrap [[Name]] / [[Name|Label]] → Name */
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
		if (s.size() >= 4 && s[0] == '[' && s[1] == '[')
		{
			s = s.substr(2);
			const size_t end = s.find("]]");
			if (end != std::string::npos) s = s.substr(0, end);
			const size_t pipe = s.find('|');
			if (pipe != std::string::npos) s = s.substr(0, pipe);
		}
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
		return s;
	}

	/* Legendaries / mystic forge are not on /v2/recipes — read {{recipe}} from the wiki. */
	bool LoadWikiRecipeForName(const char* pageTitle, int& outCount,
		std::vector<RecipeIng>& ings, std::string* sourceOut)
	{
		if (!pageTitle || !pageTitle[0]) return false;
		const std::string wt = FetchWikiWikitext(pageTitle);
		if (wt.empty()) return false;

		size_t searchFrom = 0;
		std::string bestSource;
		std::vector<RecipeIng> bestIngs;
		int bestQty = 1;
		int bestScore = -1;

		while (searchFrom < wt.size())
		{
			size_t recipe = FindCi(wt, "{{recipe", searchFrom);
			if (recipe == std::string::npos) break;
			size_t i = recipe + 2;
			int depth = 1;
			while (i + 1 < wt.size() && depth > 0)
			{
				if (wt[i] == '{' && wt[i + 1] == '{') { depth++; i += 2; continue; }
				if (wt[i] == '}' && wt[i + 1] == '}') { depth--; i += 2; continue; }
				++i;
			}
			const size_t blockEnd = i;
			const std::string block = wt.substr(recipe, blockEnd - recipe);
			searchFrom = blockEnd;

			std::string source;
			std::string recipeType;
			int qty = 1;
			bool hasUpperQty = false;
			struct NamedIng { int count = 1; std::string name; };
			std::vector<NamedIng> named;

			size_t p = 0;
			while (p < block.size())
			{
				size_t bar = block.find('|', p);
				if (bar == std::string::npos) break;
				size_t k = bar + 1;
				while (k < block.size() && (block[k] == ' ' || block[k] == '\t')) ++k;
				size_t keyStart = k;
				while (k < block.size() && block[k] != '=' && block[k] != '\n' && block[k] != '|')
					++k;
				if (k >= block.size() || block[k] != '=') { p = bar + 1; continue; }
				std::string key = block.substr(keyStart, k - keyStart);
				while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
				for (char& c : key)
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				++k;
				while (k < block.size() && (block[k] == ' ' || block[k] == '\t')) ++k;
				size_t valStart = k;
				while (k < block.size() && block[k] != '|' && block[k] != '\n') ++k;
				std::string val = block.substr(valStart, k - valStart);
				while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r'))
					val.pop_back();
				p = (k < block.size() && block[k] == '\n') ? k + 1 : k;

				if (key == "source")
					source = val;
				else if (key == "type")
					recipeType = val;
				else if (key == "upper quantity" || key == "upper qty")
					hasUpperQty = true;
				else if (key == "quantity" || key == "qty")
				{
					int v = 0;
					for (char c : val)
						if (c >= '0' && c <= '9') v = v * 10 + (c - '0');
					if (v > 0) qty = v;
				}
				else if (key.size() >= 10 && key.compare(0, 10, "ingredient") == 0)
				{
					/* "1 Zap" or "250 Glob of Ectoplasm" or "[[Zap]]" */
					size_t vi = 0;
					while (vi < val.size() && (val[vi] == ' ' || val[vi] == '\t')) ++vi;
					int count = 0;
					bool anyDigit = false;
					while (vi < val.size() && val[vi] >= '0' && val[vi] <= '9')
					{
						anyDigit = true;
						count = count * 10 + (val[vi] - '0');
						++vi;
					}
					if (!anyDigit) count = 1;
					while (vi < val.size() && (val[vi] == ' ' || val[vi] == '\t')) ++vi;
					std::string iname = CleanWikiLinkName(val.substr(vi));
					if (!iname.empty() && count > 0)
						named.push_back({ count, iname });
				}
			}

			if (named.empty()) continue;

			const std::string typeLow = ToLowerCopy(recipeType);
			const std::string srcLow = ToLowerCopy(source);
			const std::string pageLow = ToLowerCopy(pageTitle);
			/* Mystic Forge promotions (mithril→orichalcum, dust ladders) inflate plans. */
			if (typeLow.find("promotion") != std::string::npos) continue;
			if (hasUpperQty) continue;
			if (typeLow.find("salvage") != std::string::npos) continue;
			bool feedsSelf = false;
			for (const NamedIng& ni : named)
			{
				if (ToLowerCopy(ni.name) == pageLow) { feedsSelf = true; break; }
			}
			if (feedsSelf) continue;

			const bool forge = srcLow.find("mystic") != std::string::npos ||
				srcLow.find("forge") != std::string::npos;
			const bool station = srcLow.find("recipe") != std::string::npos ||
				srcLow.find("weaponsmith") != std::string::npos ||
				srcLow.find("artificer") != std::string::npos ||
				srcLow.find("huntsman") != std::string::npos ||
				srcLow.find("tailor") != std::string::npos ||
				srcLow.find("armorsmith") != std::string::npos ||
				srcLow.find("jeweler") != std::string::npos ||
				srcLow.find("chef") != std::string::npos ||
				srcLow.find("scribe") != std::string::npos;
			int score = 0;
			if (forge) score += 40;
			if (station) score += 55;
			if (typeLow.find("legendary") != std::string::npos) score += 50;
			if (typeLow.find("component") != std::string::npos) score += 15;
			if (score < 30) continue;

			if (score < bestScore) continue;

			std::vector<std::string> ingTitles;
			ingTitles.reserve(named.size());
			for (const NamedIng& ni : named)
				ingTitles.push_back(ni.name);
			std::unordered_map<std::string, std::string> pages;
			FetchWikiWikitextBatch(ingTitles, pages);

			std::vector<std::vector<int>> cands(named.size());
			std::vector<int> allIds;
			for (size_t ii = 0; ii < named.size(); ++ii)
			{
				const std::string key = ToLowerCopy(named[ii].name);
				auto pit = pages.find(key);
				const std::string& pageWt = (pit != pages.end()) ? pit->second : std::string{};
				if (pageWt.empty()) continue;
				cands[ii] = CollectWikiItemIdCandidates(pageWt);
				for (int id : cands[ii]) allIds.push_back(id);
			}
			std::unordered_map<int, std::string> idNames;
			FetchNames(idNames, allIds);

			std::vector<RecipeIng> resolved;
			for (size_t ii = 0; ii < named.size(); ++ii)
			{
				int id = 0;
				std::string resolvedName;
				for (int cand : cands[ii])
				{
					auto it = idNames.find(cand);
					if (it == idNames.end() || it->second.empty()) continue;
					id = cand;
					resolvedName = it->second;
					break;
				}
				if (id <= 0) continue;
				RecipeIng ri;
				ri.itemId = id;
				ri.count = named[ii].count;
				ri.name = resolvedName.empty() ? named[ii].name : resolvedName;
				resolved.push_back(ri);
			}
			if (resolved.empty()) continue;

			bestScore = score;
			bestQty = qty > 0 ? qty : 1;
			bestSource = source.empty() ? "Wiki recipe" : source;
			bestIngs = std::move(resolved);
			if (score >= 90) break; /* strong station/legendary hit */
		}

		if (bestIngs.empty()) return false;
		outCount = bestQty;
		ings = std::move(bestIngs);
		if (sourceOut) *sourceOut = bestSource;
		return true;
	}

	/* Significant tokens — skip of/the/a/an/and/with. */
	void TokenizeQuery(const char* q, std::vector<std::string>& tokens)
	{
		tokens.clear();
		std::string cur;
		auto flush = [&]() {
			if (cur.size() < 2) { cur.clear(); return; }
			const std::string low = ToLowerCopy(cur);
			if (low == "of" || low == "the" || low == "a" || low == "an" ||
				low == "and" || low == "with" || low == "for")
			{
				cur.clear();
				return;
			}
			tokens.push_back(low);
			cur.clear();
		};
		for (const char* p = q; *p; ++p)
		{
			if (std::isalnum(static_cast<unsigned char>(*p)) || *p == '\'')
				cur.push_back(*p);
			else
				flush();
		}
		flush();
	}

	/* Higher = closer title match. Reject weak wiki noise (e.g. "bow" → Emblem…). */
	int ScoreTitleMatch(const std::string& title, const std::vector<std::string>& tokens,
		const std::string& qLow)
	{
		const std::string tLow = ToLowerCopy(title);
		if (tLow == qLow) return 1000;
		if (!qLow.empty() && tLow.find(qLow) != std::string::npos) return 800;
		if (tokens.empty()) return 0;
		int matched = 0;
		int score = 0;
		for (const std::string& tok : tokens)
		{
			if (tLow.find(tok) != std::string::npos)
			{
				++matched;
				score += 10 + static_cast<int>(tok.size()); /* longer tokens weigh more */
			}
		}
		/* Require most of the meaningful words, not just "of"/one lucky hit. */
		const int need = (tokens.size() <= 2) ? static_cast<int>(tokens.size())
			: (static_cast<int>(tokens.size()) * 2 + 2) / 3; /* ceil(2/3) */
		if (matched < need) return 0;
		if (matched == static_cast<int>(tokens.size())) score += 50;
		return score;
	}

	void WikiSearchTitles(const char* query, std::vector<std::string>& titles, size_t maxN)
	{
		std::string url =
			"https://wiki.guildwars2.com/api.php?action=query&list=search&srnamespace=0"
			"&srlimit=8&format=json&formatversion=2&srsearch=";
		url += UrlEncode(query);
		auto wr = Gw2Http::Get(url.c_str(), nullptr, kHttpTimeoutMs);
		if (!wr.ok) return;
		size_t p = 0;
		while (titles.size() < maxN && p < wr.body.size())
		{
			size_t t = wr.body.find("\"title\"", p);
			if (t == std::string::npos) break;
			std::string title = JsonStringAfterKey(wr.body, "title", t);
			p = t + 7;
			if (title.empty()) continue;
			bool dup = false;
			for (const auto& h : titles)
				if (h == title) { dup = true; break; }
			if (!dup) titles.push_back(title);
		}
	}

	/* Common food typo: "bow of …" → "bowl of …". */
	std::string TypoHintsQuery(const char* q)
	{
		std::string s = q ? q : "";
		std::string low = ToLowerCopy(s);
		/* whole-word bow → bowl (not already bowl) */
		std::string out;
		size_t i = 0;
		bool changed = false;
		while (i < low.size())
		{
			if ((i == 0 || !std::isalnum(static_cast<unsigned char>(low[i - 1]))) &&
				i + 3 <= low.size() && low.compare(i, 3, "bow") == 0 &&
				(i + 3 == low.size() || !std::isalnum(static_cast<unsigned char>(low[i + 3]))) &&
				!(i + 4 <= low.size() && low.compare(i, 4, "bowl") == 0))
			{
				out += "bowl";
				/* copy casing-agnostic replacement into original-length stream from s */
				i += 3;
				changed = true;
				continue;
			}
			out.push_back(s[i]);
			++i;
		}
		return changed ? out : std::string{};
	}

	int ResolveQueryToItemId(const char* q, Plan& plan, std::string* nameOut)
	{
		const int id = ParseItemId(q);
		if (id > 0)
		{
			std::string name = ItemName(id);
			if (!name.empty())
			{
				if (nameOut) *nameOut = name;
				return id;
			}
			return 0;
		}

		std::vector<std::string> tokens;
		TokenizeQuery(q, tokens);
		const std::string qLow = ToLowerCopy(q ? q : "");

		std::vector<std::string> titles;
		WikiSearchTitles(q, titles, 8);

		auto bestScoreAmong = [&](const std::vector<std::string>& list, const char* queryStr) {
			std::vector<std::string> toks;
			TokenizeQuery(queryStr, toks);
			const std::string ql = ToLowerCopy(queryStr);
			int best = 0;
			for (const std::string& title : list)
			{
				const int s = ScoreTitleMatch(title, toks, ql);
				if (s > best) best = s;
			}
			return best;
		};

		/* Skip second wiki round-trip when the first search already looks solid. */
		std::string typo;
		if (bestScoreAmong(titles, q) < 80)
		{
			typo = TypoHintsQuery(q);
			if (!typo.empty())
				WikiSearchTitles(typo.c_str(), titles, 12);
		}

		struct Ranked { std::string title; int score = 0; };
		std::vector<Ranked> ranked;
		ranked.reserve(titles.size());
		std::vector<std::string> typoTok;
		if (!typo.empty())
			TokenizeQuery(typo.c_str(), typoTok);
		const std::string typoLow = typo.empty() ? std::string{} : ToLowerCopy(typo);
		for (const std::string& title : titles)
		{
			Ranked r;
			r.title = title;
			r.score = ScoreTitleMatch(title, tokens, qLow);
			if (!typo.empty())
			{
				const int s2 = ScoreTitleMatch(title, typoTok, typoLow);
				if (s2 > r.score) r.score = s2;
			}
			ranked.push_back(std::move(r));
		}
		std::sort(ranked.begin(), ranked.end(),
			[](const Ranked& a, const Ranked& b) { return a.score > b.score; });

		plan.nameHints.clear();
		for (const Ranked& r : ranked)
		{
			if (plan.nameHints.size() >= 8) break;
			plan.nameHints.push_back(r.title);
		}

		/* Auto-resolve only a confident title match — never first-hit roulette. */
		constexpr int kMinAutoScore = 30;
		for (const Ranked& r : ranked)
		{
			if (r.score < kMinAutoScore) break;
			const int resolved = ResolveWikiTitleToItemId(r.title.c_str(), nameOut);
			if (resolved > 0)
			{
				/* Extra guard: resolved item name should also look like the query. */
				std::string itemName = nameOut && !nameOut->empty() ? *nameOut : ItemName(resolved);
				const int nameScore = ScoreTitleMatch(itemName, tokens, qLow);
				int nameScoreTypo = 0;
				if (!typo.empty())
				{
					std::vector<std::string> typoTok;
					TokenizeQuery(typo.c_str(), typoTok);
					nameScoreTypo = ScoreTitleMatch(itemName, typoTok, ToLowerCopy(typo));
				}
				if (nameScore >= kMinAutoScore || nameScoreTypo >= kMinAutoScore)
				{
					if (nameOut && nameOut->empty()) *nameOut = itemName;
					return resolved;
				}
			}
		}
		return 0;
	}

	DWORD WINAPI PlanProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		for (;;)
		{
			const unsigned gen = gPlanGen.load();
			char q[192];
			std::snprintf(q, sizeof(q), "%s", gThreadQuery);
			Plan plan;
			if (!q[0])
			{
				plan.status = "Paste a chat code, item ID, or name.";
			}
			else
			{
				plan.status = "Resolving item…";
				PublishLivePlan(plan);
				std::string resolvedName;
				const int itemId = ResolveQueryToItemId(q, plan, &resolvedName);
				if (gen != gPlanGen.load())
					goto restart_or_done;
				if (itemId <= 0)
				{
					plan.status = plan.nameHints.empty()
						? "Could not resolve that item."
						: "No close name match — click a wiki title below (check spelling), "
						  "or paste a chat code / ID.";
				}
				else
				{
					plan.outputId = itemId;
					plan.outputName = resolvedName.empty() ? ItemName(itemId) : resolvedName;
					plan.nameHints.clear();
					plan.status = "Fetching recipe…";
					PublishLivePlan(plan);
					std::unordered_map<int, RecipeCacheEntry> recipeCache;
					int outCount = 1;
					std::vector<RecipeIng> ings;
					int recipeId = 0;
					std::string recipeSource;
					if (!TryLoadRecipe(itemId, plan.outputName, outCount, ings, recipeId,
							recipeCache, &recipeSource))
					{
						char buf[192];
						std::snprintf(buf, sizeof(buf),
							"Found %s (#%d) — no station or wiki forge recipe.",
							plan.outputName.empty() ? "item" : plan.outputName.c_str(),
							itemId);
						plan.status = buf;
						plan.outputCount = outCount;
					}
					else if (gen == gPlanGen.load())
					{
						plan.outputCount = outCount;
						plan.recipeSource = recipeSource;
						std::unordered_map<int, std::string> names;
						names[itemId] = plan.outputName;

						/* Paint top recipe immediately — stash / gifts catch up after. */
						plan.root = {};
						plan.root.itemId = itemId;
						plan.root.need = 1;
						plan.root.have = 0;
						plan.root.name = plan.outputName;
						plan.root.crafted = true;
						plan.root.depth = 0;
						for (const RecipeIng& ri : ings)
						{
							if (!ri.name.empty())
								names[ri.itemId] = ri.name;
							IngNode kid;
							kid.itemId = ri.itemId;
							kid.need = ri.count;
							kid.depth = 1;
							kid.name = ri.name;
							kid.have = 0;
							plan.root.kids.push_back(std::move(kid));
						}
						plan.ok = true;
						plan.nameHints.clear();
						plan.status = std::string(plan.recipeSource.empty() ? "Recipe" : plan.recipeSource)
							+ " · loading stash…";
						PublishLivePlan(plan);

						std::unordered_map<int, int> owned;
						LoadOwned(owned);
						if (gen != gPlanGen.load())
							goto restart_or_done;
						ApplyOwnedCounts(plan.root, owned);
						plan.status = std::string(plan.recipeSource.empty() ? "Recipe" : plan.recipeSource)
							+ " · expanding gifts…";
						PublishLivePlan(plan);

						for (int depth = 2; depth <= kMaxDepth; ++depth)
						{
							if (gen != gPlanGen.load())
								break;
							char st[96];
							std::snprintf(st, sizeof(st), "Expanding gifts (depth %d/%d)…",
								depth, kMaxDepth);
							plan.status = st;
							PublishLivePlan(plan);
							ExpandFrontier(plan, owned, names, recipeCache, depth);
							ApplyOwnedCounts(plan.root, owned);
							plan.status = st;
							if (gen == gPlanGen.load())
								PublishLivePlan(plan);
						}

						if (gen == gPlanGen.load())
						{
							plan.status = "Pricing materials…";
							PublishLivePlan(plan);
							FinishPrices(plan, names);
							PublishLivePlan(plan);
						}
					}
				}
			}
			if (!plan.ok)
			{
				std::lock_guard<std::mutex> lock(gMu);
				if (gen == gPlanGen.load())
					gPlan = std::move(plan);
			}

		restart_or_done:
			if (gen != gPlanGen.load())
				continue; /* newer Plan() — reuse this worker */
			gReady = false;
			gBusy = false;
			/* Lost the race with StartPlan after clearing busy? */
			if (gen != gPlanGen.load() && !gBusy.exchange(true))
				continue;
			return 0;
		}
	}

	DWORD WINAPI DailyProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		std::vector<DailyRow> rows;
		std::string status;
		auto r = Gw2Http::Api("/v2/dailycrafting", nullptr, kHttpTimeoutMs);
		if (!r.ok)
		{
			status = "Could not load daily crafting list.";
		}
		else
		{
			/* ["item", ...] string ids or item names? Actually returns string slugs like "glob_of_ectoplasm" OR ids?
			   API: array of strings — daily crafting recipe ids as strings matching /v2/dailycrafting */
			std::vector<std::string> slugs;
			size_t i = 0;
			while (i < r.body.size() && slugs.size() < 32)
			{
				while (i < r.body.size() && r.body[i] != '"') ++i;
				if (i >= r.body.size()) break;
				++i;
				std::string val;
				while (i < r.body.size() && r.body[i] != '"')
				{
					if (r.body[i] == '\\' && i + 1 < r.body.size()) { val.push_back(r.body[i + 1]); i += 2; continue; }
					val.push_back(r.body[i++]);
				}
				if (i < r.body.size()) ++i;
				if (!val.empty()) slugs.push_back(val);
			}
			/* Resolve names via /v2/dailycrafting?ids=slug1,slug2 */
			if (!slugs.empty())
			{
				std::string path = "/v2/dailycrafting?ids=";
				for (size_t si = 0; si < slugs.size(); ++si)
				{
					if (si) path += ',';
					path += slugs[si];
				}
				auto det = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
				if (det.ok)
				{
					size_t p = 0;
					while (p < det.body.size())
					{
						size_t brace = det.body.find('{', p);
						if (brace == std::string::npos) break;
						size_t end = JsonObjectEnd(det.body, brace);
						if (end == std::string::npos) break;
						DailyRow row;
						row.name = JsonStringAfterKey(det.body, "id", brace);
						/* dailycrafting objects: { "id": "slug" } — need item via separate mapping.
						   Actually schema is just { "id": "charged_quartz_crystal" }.
						   Convert slug to display name. */
						if (!row.name.empty())
						{
							std::string pretty = row.name;
							for (char& c : pretty)
								if (c == '_') c = ' ';
							if (!pretty.empty())
								pretty[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(pretty[0])));
							row.name = pretty;
							rows.push_back(row);
						}
						p = end + 1;
					}
				}
				if (rows.empty())
				{
					for (const std::string& s : slugs)
					{
						DailyRow row;
						row.name = s;
						for (char& c : row.name)
							if (c == '_') c = ' ';
						rows.push_back(row);
					}
				}
			}
			status = rows.empty() ? "No daily crafts listed." : "Daily crafting (UTC reset).";
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPendingDailies = std::move(rows);
			gDailyStatus = status;
			gDailyReady = true;
			gDailyBusy = false;
			gDailyFetchedAt = GetTickCount();
		}
		return 0;
	}

	void StartPlan()
	{
		std::snprintf(gThreadQuery, sizeof(gThreadQuery), "%s", gQuery);
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPlan.status = "Planning…";
			gPlan.ok = false;
			gPlan.root = {};
			gPlan.nameHints.clear();
		}
		++gPlanGen; /* cancel in-flight expand; worker restarts if still alive */
		if (gBusy.exchange(true))
			return; /* existing PlanProc loop will pick up new gen/query */
		if (gThread)
		{
			/* Reap finished worker; if still winding down, wait briefly (rare). */
			if (WaitForSingleObject(gThread, 8000) == WAIT_OBJECT_0)
			{
				CloseHandle(gThread);
				gThread = nullptr;
			}
		}
		gThread = CreateThread(nullptr, 0, PlanProc, nullptr, 0, nullptr);
		if (!gThread) gBusy = false;
	}

	void StartDailies(bool force)
	{
		if (!force && gDailyFetchedAt != 0 &&
			(GetTickCount() - gDailyFetchedAt) < kDailyTtlMs)
			return;
		if (gDailyBusy.exchange(true)) return;
		if (gDailyThread)
		{
			WaitForSingleObject(gDailyThread, 0);
			CloseHandle(gDailyThread);
			gDailyThread = nullptr;
		}
		gDailyThread = CreateThread(nullptr, 0, DailyProc, nullptr, 0, nullptr);
		if (!gDailyThread) gDailyBusy = false;
	}

	void Tick()
	{
		if (gThread && !gBusy && WaitForSingleObject(gThread, 0) == WAIT_OBJECT_0)
		{
			CloseHandle(gThread);
			gThread = nullptr;
		}
		if (gReady)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gReady)
			{
				gPlan = std::move(gPendingPlan);
				gPendingPlan = {};
				gReady = false;
			}
		}
		if (gDailyReady)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gDailyReady)
			{
				gDailies = std::move(gPendingDailies);
				gPendingDailies.clear();
				gDailyReady = false;
			}
			if (gDailyThread)
			{
				WaitForSingleObject(gDailyThread, 0);
				CloseHandle(gDailyThread);
				gDailyThread = nullptr;
			}
		}
	}
}

void CraftingData::RefreshDailiesIfNeeded(bool force)
{
	StartDailies(force);
}

void CraftingData::QueuePlan(const char* itemNameOrCode)
{
	if (!itemNameOrCode || !itemNameOrCode[0]) return;
	std::snprintf(gQuery, sizeof(gQuery), "%s", itemNameOrCode);
	gFocusTab = true;
	StartPlan();
}

bool CraftingData::ConsumeFocusTab()
{
	return gFocusTab.exchange(false);
}

void CraftingData::RenderContents()
{
	Tick();
	StartDailies(false);

	Plan plan;
	std::vector<DailyRow> dailies;
	std::string dailyStatus;
	{
		std::lock_guard<std::mutex> lock(gMu);
		plan = gPlan;
		dailies = gDailies;
		dailyStatus = gDailyStatus;
	}

	ImGui::TextUnformatted("Crafting planner");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Station crafts use the official recipe API. Legendaries / gifts use wiki "
		"Mystic Forge trees (expandable to depth %d) — gifts, sub-gifts, then mats. "
		"Owned counts: materials, bank, shared (API key).",
		kMaxDepth);
	ImGui::PopTextWrapPos();

	const float btnW = ImGui::CalcTextSize("Plan").x + ImGui::GetStyle().FramePadding.x * 2.f + 16.f;
	float fieldW = ImGui::GetContentRegionAvail().x - btnW - ImGui::GetStyle().ItemSpacing.x;
	if (fieldW < 120.f) fieldW = 120.f;
	ImGui::SetNextItemWidth(fieldW);
	if (ImGui::InputTextWithHint("###gw2igh_craft_q", "[&…] / ID / name",
			gQuery, sizeof(gQuery), ImGuiInputTextFlags_EnterReturnsTrue))
		StartPlan();
	ImGui::SameLine();
	if (ImGui::Button("Plan###gw2igh_craft_go", ImVec2(btnW, 0.f)))
		StartPlan();

	if (gBusy && !plan.ok)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
			plan.status.empty() ? "Planning…" : plan.status.c_str());
	else if (gBusy && plan.ok)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s", plan.status.c_str());
	else if (!plan.status.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", plan.status.c_str());

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.f), "Daily crafting");
	if (gDailyBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Loading…");
	else if (dailies.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
			"%s", dailyStatus.empty() ? "—" : dailyStatus.c_str());
	else
	{
		for (const DailyRow& d : dailies)
			ImGui::BulletText("%s", d.name.c_str());
	}

	ImGui::Separator();

	const float listH = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("###gw2igh_craft_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);

	if (plan.ok)
	{
		ImGui::TextColored(ImVec4(0.85f, 0.80f, 0.95f, 1.f), "%s",
			plan.outputName.empty() ? "Output" : plan.outputName.c_str());
		if (!plan.recipeSource.empty())
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
				"#%d · %s · crafts %d", plan.outputId, plan.recipeSource.c_str(),
				plan.outputCount);
		else
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
				"#%d · crafts %d per recipe", plan.outputId, plan.outputCount);
		if (plan.noTpMissing > 0)
		{
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f),
				"TP buy (instant): %s", FormatCoins(plan.buyTotal).c_str());
			ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f),
				"Some missing mats are account-bound / not on the TP.");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f),
				"TP buy (instant): %s", FormatCoins(plan.buyTotal).c_str());
		}
		ImGui::Spacing();
		for (const IngNode& k : plan.root.kids)
			DrawNode(k);
	}
	else if (!plan.nameHints.empty())
	{
		ImGui::TextUnformatted("Wiki results");
		for (size_t i = 0; i < plan.nameHints.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Selectable(plan.nameHints[i].c_str()))
			{
				std::snprintf(gQuery, sizeof(gQuery), "%s", plan.nameHints[i].c_str());
				StartPlan();
			}
			ImGui::PopID();
		}
	}
	else if (!gBusy)
	{
		ImGui::TextWrapped(
			"Try an ascended food, gift, or crafted gear name — or Shift+click a chat code.");
	}

	ImGui::EndChild();
}
