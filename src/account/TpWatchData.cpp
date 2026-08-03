#include "TpWatchPad.h"

#include "TpWatchShared.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

namespace TpWatchDetail
{
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

	/* Compact for the alert field: "5g", "50s", "12c", or "1g 50s". */
	void FormatAlertEdit(long long copper, char* out, size_t outLen)
	{
		if (!out || outLen == 0) return;
		out[0] = 0;
		if (copper <= 0) return;
		const long long g = copper / 10000;
		const long long s = (copper % 10000) / 100;
		const long long c = copper % 100;
		if (g > 0 && s == 0 && c == 0)
			std::snprintf(out, outLen, "%lldg", g);
		else if (g > 0 && c == 0)
			std::snprintf(out, outLen, "%lldg %llds", g, s);
		else if (g > 0)
			std::snprintf(out, outLen, "%lldg %llds %lldc", g, s, c);
		else if (s > 0 && c == 0)
			std::snprintf(out, outLen, "%llds", s);
		else if (s > 0)
			std::snprintf(out, outLen, "%llds %lldc", s, c);
		else
			std::snprintf(out, outLen, "%lldc", c);
	}

	/* "5g", "50s", "1g 20s", "12345" (copper). Empty / junk → 0. */
	long long ParseCoinsInput(const char* text)
	{
		if (!text) return 0;
		long long total = 0;
		bool anyUnit = false;
		const char* p = text;
		while (*p)
		{
			while (*p == ' ' || *p == '\t' || *p == ',' || *p == '+') ++p;
			if (!*p) break;
			long long v = 0;
			bool digits = false;
			while (*p >= '0' && *p <= '9')
			{
				digits = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (!digits) break;
			char u = *p;
			if (u >= 'A' && u <= 'Z') u = static_cast<char>(u - 'A' + 'a');
			if (u == 'g')
			{
				anyUnit = true;
				total += v * 10000;
				++p;
			}
			else if (u == 's')
			{
				anyUnit = true;
				total += v * 100;
				++p;
			}
			else if (u == 'c')
			{
				anyUnit = true;
				total += v;
				++p;
			}
			else
			{
				/* Bare number: copper if alone, otherwise stop. */
				if (!anyUnit && total == 0)
					return v;
				break;
			}
		}
		return total > 0 ? total : 0;
	}

	void ParseIds(const char* csv, std::vector<int>& out)
	{
		out.clear();
		if (!csv) return;
		const char* p = csv;
		while (*p && out.size() < static_cast<size_t>(kMaxItems))
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t') ++p;
			if (!*p) break;
			int v = 0;
			bool any = false;
			while (*p >= '0' && *p <= '9')
			{
				any = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (any && v > 0)
			{
				bool dup = false;
				for (int x : out) if (x == v) { dup = true; break; }
				if (!dup) out.push_back(v);
			}
			while (*p && *p != ',' && *p != ';' && !(*p >= '0' && *p <= '9')) ++p;
		}
	}

	void ParseAlerts(const char* csv, std::vector<std::pair<int, long long>>& out)
	{
		out.clear();
		if (!csv) return;
		const char* p = csv;
		while (*p && out.size() < static_cast<size_t>(kMaxItems))
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t') ++p;
			if (!*p) break;
			int id = 0;
			bool anyId = false;
			while (*p >= '0' && *p <= '9')
			{
				anyId = true;
				id = id * 10 + (*p - '0');
				++p;
			}
			long long thresh = 0;
			if (anyId && *p == ':')
			{
				++p;
				bool anyT = false;
				while (*p >= '0' && *p <= '9')
				{
					anyT = true;
					thresh = thresh * 10 + (*p - '0');
					++p;
				}
				if (!anyT) thresh = 0;
			}
			if (anyId && id > 0 && thresh > 0)
			{
				bool dup = false;
				for (auto& e : out)
				{
					if (e.first == id)
					{
						e.second = thresh;
						dup = true;
						break;
					}
				}
				if (!dup) out.emplace_back(id, thresh);
			}
			while (*p && *p != ',' && *p != ';') ++p;
		}
	}

	void SaveAlerts(const std::vector<std::pair<int, long long>>& alerts)
	{
		std::string s;
		for (size_t i = 0; i < alerts.size(); ++i)
		{
			if (alerts[i].second <= 0) continue;
			if (!s.empty()) s += ',';
			s += std::to_string(alerts[i].first);
			s += ':';
			s += std::to_string(alerts[i].second);
		}
		if (s.size() >= sizeof(G::TpWatchAlerts))
			s.resize(sizeof(G::TpWatchAlerts) - 1);
		std::snprintf(G::TpWatchAlerts, sizeof(G::TpWatchAlerts), "%s", s.c_str());
		Settings::SetDirty();
	}

	void SetAlertForId(int id, long long thresh)
	{
		if (id <= 0) return;
		std::vector<std::pair<int, long long>> alerts;
		ParseAlerts(G::TpWatchAlerts, alerts);
		bool found = false;
		for (size_t i = 0; i < alerts.size(); ++i)
		{
			if (alerts[i].first != id) continue;
			found = true;
			if (thresh <= 0)
				alerts.erase(alerts.begin() + static_cast<std::ptrdiff_t>(i));
			else
				alerts[i].second = thresh;
			break;
		}
		if (!found && thresh > 0)
			alerts.emplace_back(id, thresh);
		SaveAlerts(alerts);
	}

	void PruneAlertsToIds(const std::vector<int>& ids)
	{
		std::vector<std::pair<int, long long>> alerts;
		ParseAlerts(G::TpWatchAlerts, alerts);
		std::vector<std::pair<int, long long>> next;
		for (const auto& e : alerts)
		{
			bool keep = false;
			for (int id : ids) if (id == e.first) { keep = true; break; }
			if (keep) next.push_back(e);
		}
		if (next.size() != alerts.size())
			SaveAlerts(next);
	}

	void SaveIds(const std::vector<int>& ids)
	{
		std::string s;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) s += ',';
			s += std::to_string(ids[i]);
		}
		if (s.size() >= sizeof(G::TpWatchIds))
			s.resize(sizeof(G::TpWatchIds) - 1);
		std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", s.c_str());
		PruneAlertsToIds(ids);
		Settings::SetDirty();
	}

	/* Attach thresholds + hit flags; returns number of hits. */
	int ApplyAlerts(std::vector<Row>& rows)
	{
		std::vector<std::pair<int, long long>> alerts;
		ParseAlerts(G::TpWatchAlerts, alerts);
		int hits = 0;
		for (Row& r : rows)
		{
			r.alertSell = 0;
			for (const auto& e : alerts)
				if (e.first == r.id) { r.alertSell = e.second; break; }
			r.alertHit = (r.alertSell > 0 && r.sell > 0 && r.sell <= r.alertSell);
			if (r.alertHit) ++hits;
		}
		return hits;
	}

	int ParseItemInput(const char* text)
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
		/* Pure numeric ID only — names must go through wiki resolve. */
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

	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	/* Common shorthand → wiki title (ecto etc.). */
	std::string ExpandNameAlias(const char* q)
	{
		const std::string low = ToLowerCopy(q ? q : "");
		std::string t;
		for (char c : low)
			if (c != ' ' && c != '\t') t.push_back(c);
		if (t == "ecto" || t == "ectos" || t == "ectoplasm" ||
			t == "globofecto" || t == "globofectoplasm")
			return "Glob of Ectoplasm";
		return q ? std::string(q) : std::string{};
	}

	int ExtractWikiItemId(const std::string& wikitext)
	{
		size_t p = 0;
		while (p < wikitext.size())
		{
			size_t bar = wikitext.find('|', p);
			if (bar == std::string::npos) break;
			size_t k = bar + 1;
			while (k < wikitext.size() && (wikitext[k] == ' ' || wikitext[k] == '\t')) ++k;
			if (k + 2 < wikitext.size() &&
				(wikitext[k] == 'i' || wikitext[k] == 'I') &&
				(wikitext[k + 1] == 'd' || wikitext[k + 1] == 'D') &&
				(wikitext[k + 2] == ' ' || wikitext[k + 2] == '=' || wikitext[k + 2] == '\t'))
			{
				while (k < wikitext.size() && wikitext[k] != '=') ++k;
				if (k < wikitext.size() && wikitext[k] == '=')
				{
					++k;
					while (k < wikitext.size() && (wikitext[k] == ' ' || wikitext[k] == '\t')) ++k;
					int id = 0;
					while (k < wikitext.size() && wikitext[k] >= '0' && wikitext[k] <= '9')
					{
						id = id * 10 + (wikitext[k] - '0');
						++k;
					}
					if (id > 0) return id;
				}
			}
			p = bar + 1;
		}
		return 0;
	}

	bool ItemExists(int id)
	{
		if (id <= 0) return false;
		char path[64];
		std::snprintf(path, sizeof(path), "/v2/items/%d", id);
		auto r = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		return r.ok && !r.body.empty() && r.body[0] == '{';
	}

	int ResolveWikiTitleToItemId(const char* title)
	{
		if (!title || !title[0]) return 0;
		std::string parseUrl =
			"https://wiki.guildwars2.com/api.php?action=parse&prop=wikitext&format=json"
			"&formatversion=2&page=";
		parseUrl += UrlEncode(title);
		auto pr = Gw2Http::Get(parseUrl.c_str(), nullptr, kHttpTimeoutMs);
		if (!pr.ok) return 0;
		std::string wt = JsonStringAfterKey(pr.body, "wikitext", 0);
		if (wt.empty())
		{
			size_t wk = pr.body.find("\"wikitext\"");
			if (wk != std::string::npos)
				wt = JsonStringAfterKey(pr.body, "wikitext", wk);
		}
		const int id = ExtractWikiItemId(wt);
		return ItemExists(id) ? id : 0;
	}
	bool CommitWatchId(int id, std::string* statusOut)
	{
		if (id <= 0)
		{
			if (statusOut) *statusOut = "Could not resolve that item.";
			return false;
		}
		std::vector<int> ids;
		ParseIds(G::TpWatchIds, ids);
		for (int x : ids)
		{
			if (x == id)
			{
				if (statusOut) *statusOut = "Already on your watchlist.";
				return false;
			}
		}
		if (static_cast<int>(ids.size()) >= kMaxItems)
		{
			if (statusOut) *statusOut = "Watchlist full (120).";
			return false;
		}
		ids.push_back(id);
		SaveIds(ids);
		SyncRowsFromSettings();
		StartFetch();
		if (statusOut) *statusOut = "Added. Fetching price…";
		return true;
	}

	DWORD WINAPI AddNameProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		const std::string query = ExpandNameAlias(gAddThreadQuery);
		std::string status;
		std::vector<NameHit> hits;
		std::vector<std::string> titles;

		std::string url =
			"https://wiki.guildwars2.com/api.php?action=query&list=search&srnamespace=0"
			"&srlimit=8&format=json&formatversion=2&srsearch=";
		url += UrlEncode(query.c_str());
		auto wr = Gw2Http::Get(url.c_str(), nullptr, kHttpTimeoutMs);
		if (!wr.ok)
		{
			status = "Wiki search failed — try a chat code or ID.";
		}
		else
		{
			size_t p = 0;
			while (titles.size() < 8 && p < wr.body.size())
			{
				size_t t = wr.body.find("\"title\"", p);
				if (t == std::string::npos) break;
				std::string title = JsonStringAfterKey(wr.body, "title", t);
				p = t + 7;
				if (title.empty()) continue;
				bool dup = false;
				for (const auto& h : titles)
					if (h == title) { dup = true; break; }
				if (!dup) titles.push_back(std::move(title));
			}
			if (titles.empty())
			{
				status = "No name match — try a chat code or item ID.";
			}
			else
			{
				const std::string qLow = ToLowerCopy(query);
				/* Exact title first, then the rest — never auto-track. */
				std::vector<std::string> ordered;
				for (const std::string& title : titles)
					if (ToLowerCopy(title) == qLow)
						ordered.push_back(title);
				for (const std::string& title : titles)
				{
					bool already = false;
					for (const auto& o : ordered)
						if (o == title) { already = true; break; }
					if (!already) ordered.push_back(title);
				}
				for (const std::string& title : ordered)
				{
					if (hits.size() >= 6) break;
					const int id = ResolveWikiTitleToItemId(title.c_str());
					if (id <= 0) continue;
					bool idDup = false;
					for (const NameHit& h : hits)
						if (h.id == id) { idDup = true; break; }
					if (idDup) continue;
					NameHit hit;
					hit.id = id;
					hit.name = title;
					hits.push_back(std::move(hit));
				}
				if (!hits.empty())
					FillNameHitPrices(hits);
				status = hits.empty()
					? "Wiki hits — none resolved to an item ID."
					: "Choose Track on an item below.";
			}
		}

		{
			std::lock_guard<std::mutex> lock(gMu);
			gPendingAddStatus = status;
			gPendingNameHits = std::move(hits);
			gAddReady = true;
			gAddBusy = false;
		}
		return 0;
	}

	void StartNameResolve()
	{
		if (!gAddBuf[0] || gAddBusy.exchange(true))
			return;
		if (gAddThread)
		{
			WaitForSingleObject(gAddThread, 0);
			CloseHandle(gAddThread);
			gAddThread = nullptr;
		}
		std::snprintf(gAddThreadQuery, sizeof(gAddThreadQuery), "%s", gAddBuf);
		gNameHits.clear();
		gStatus = "Searching name…";
		gAddThread = CreateThread(nullptr, 0, AddNameProc, nullptr, 0, nullptr);
		if (!gAddThread)
		{
			gAddBusy = false;
			gStatus = "Could not start name search.";
		}
	}

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
				outNames.emplace_back(static_cast<int>(id), std::move(name));
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

		std::string path = "/v2/commerce/prices?ids=";
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
			size_t buys = r.body.find("\"buys\"", brace);
			size_t sells = r.body.find("\"sells\"", brace);
			long long buy = -1, sell = -1;
			if (buys != std::string::npos && buys < end)
				buy = JsonIntAfterKey(r.body, "unit_price", buys);
			if (sells != std::string::npos && sells < end)
				sell = JsonIntAfterKey(r.body, "unit_price", sells);
			if (id > 0)
			{
				for (NameHit& h : hits)
				{
					if (h.id != static_cast<int>(id)) continue;
					if (buy >= 0) h.buy = buy;
					if (sell >= 0) h.sell = sell;
					h.hasPrices = (buy >= 0 || sell >= 0);
					break;
				}
			}
			p = end + 1;
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
			std::string path = "/v2/commerce/prices?ids=";
			path += IdsQuery(ids);
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (r.ok)
			{
				size_t p = 0;
				while (p < r.body.size())
				{
					size_t brace = r.body.find('{', p);
					if (brace == std::string::npos) break;
					size_t end = JsonObjectEnd(r.body, brace);
					if (end == std::string::npos) break;
					long long id = JsonIntAfterKey(r.body, "id", brace);
					size_t buys = r.body.find("\"buys\"", brace);
					size_t sells = r.body.find("\"sells\"", brace);
					long long buy = -1, sell = -1;
					if (buys != std::string::npos && buys < end)
						buy = JsonIntAfterKey(r.body, "unit_price", buys);
					if (sells != std::string::npos && sells < end)
						sell = JsonIntAfterKey(r.body, "unit_price", sells);
					if (id > 0)
					{
						for (Row& row : rows)
						{
							if (row.id == static_cast<int>(id))
							{
								if (buy >= 0) row.buy = buy;
								if (sell >= 0) row.sell = sell;
								break;
							}
						}
					}
					p = end + 1;
				}
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
		}

		if (d.coins == 0 && d.items.empty())
			d.status = "Nothing waiting to claim.";
		else if (d.items.empty())
			d.status = "Coins only — claim in-game at the Trading Post.";
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
		gStatus = "Refreshing…";
		gThread = CreateThread(nullptr, 0, FetchProc, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			gStatus = "Could not start refresh.";
		}
	}

	void SyncRowsFromSettings()
	{
		std::vector<int> ids;
		ParseIds(G::TpWatchIds, ids);
		std::lock_guard<std::mutex> lock(gMu);
		std::vector<Row> next;
		next.reserve(ids.size());
		for (int id : ids)
		{
			Row r;
			r.id = id;
			for (const Row& old : gRows)
			{
				if (old.id == id)
				{
					r = old;
					break;
				}
			}
			next.push_back(std::move(r));
		}
		ApplyAlerts(next);
		gRows = std::move(next);
	}

} // namespace TpWatchDetail
