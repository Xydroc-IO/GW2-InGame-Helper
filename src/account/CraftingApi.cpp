#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "InventoryData.h"

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

namespace CraftingDetail
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
		InventoryData::Tick();
		if (!InventoryData::Ready())
		{
			InventoryData::RefreshIfNeeded(false);
			/* Fall back to a quick materials/bank/shared pull if inventory module
			   is still cold — same endpoints, keeps planner usable. */
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
			return;
		}
		InventoryData::FillOwnedMap(owned);
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
	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

} // namespace CraftingDetail
