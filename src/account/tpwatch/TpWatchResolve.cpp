#include "TpWatchPad.h"

#include "TpWatchShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

namespace TpWatchDetail
{
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

	/* Common shorthand -> wiki title (ecto etc.). */
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
		if (statusOut) *statusOut = "Added. Fetching price...";
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
			status = "Wiki search failed - try a chat code or ID.";
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
				status = "No name match - try a chat code or item ID.";
			}
			else
			{
				const std::string qLow = ToLowerCopy(query);
				/* Exact title first, then the rest - never auto-track. */
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
					? "Wiki hits - none resolved to an item ID."
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
		gStatus = "Searching name...";
		gAddThread = CreateThread(nullptr, 0, AddNameProc, nullptr, 0, nullptr);
		if (!gAddThread)
		{
			gAddBusy = false;
			gStatus = "Could not start name search.";
		}
	}
} // namespace TpWatchDetail
