#include "WalletPad.h"

#include "WalletShared.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Gw2Icons.h"
#include "InventoryData.h"
#include "Settings.h"

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

namespace WalletDetail
{
	std::wstring NamesPathW()
	{
		return AddonPaths::CacheDir() + L"\\stash-names.cache";
	}

	void LoadNames()
	{
		std::lock_guard<std::mutex> lock(gNameMu);
		if (gNamesLoaded) return;
		gNamesLoaded = true;
		HANDLE h = CreateFileW(NamesPathW().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(h);
			return;
		}
		std::string data(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD rd = 0;
		ReadFile(h, data.data(), static_cast<DWORD>(data.size()), &rd, nullptr);
		CloseHandle(h);
		size_t i = 0;
		while (i < data.size())
		{
			size_t eol = data.find('\n', i);
			if (eol == std::string::npos) eol = data.size();
			std::string line = data.substr(i, eol - i);
			i = eol + 1;
			if (!line.empty() && line.back() == '\r') line.pop_back();
			const size_t tab = line.find('\t');
			if (tab == std::string::npos || tab == 0) continue;
			const int id = std::atoi(line.c_str());
			if (id == 0 && line[0] != '-' && line[0] != '0') continue;
			gNames[id] = line.substr(tab + 1);
		}
	}

	void SaveNames()
	{
		std::unordered_map<int, std::string> copy;
		{
			std::lock_guard<std::mutex> lock(gNameMu);
			copy = gNames;
		}
		std::string out;
		out.reserve(copy.size() * 40);
		for (const auto& kv : copy)
		{
			out += std::to_string(kv.first);
			out += '\t';
			out += kv.second;
			out += '\n';
		}
		HANDLE h = CreateFileW(NamesPathW().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return;
		DWORD wr = 0;
		WriteFile(h, out.data(), static_cast<DWORD>(out.size()), &wr, nullptr);
		CloseHandle(h);
	}

	std::string LookupName(int mapKey, int id, bool currency)
	{
		std::lock_guard<std::mutex> lock(gNameMu);
		auto it = gNames.find(mapKey);
		if (it != gNames.end()) return it->second;
		char buf[48];
		std::snprintf(buf, sizeof(buf), currency ? "Currency #%d" : "Item #%d", id);
		return buf;
	}

	void RememberName(int mapKey, const std::string& name)
	{
		if (name.empty()) return;
		std::lock_guard<std::mutex> lock(gNameMu);
		gNames[mapKey] = name;
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

	std::string FormatCount(long long n)
	{
		char buf[48];
		if (n >= 1000000)
			std::snprintf(buf, sizeof(buf), "%.2fM", n / 1000000.0);
		else if (n >= 10000)
			std::snprintf(buf, sizeof(buf), "%lldk", n / 1000);
		else
			std::snprintf(buf, sizeof(buf), "%lld", n);
		return buf;
	}

	std::string UrlEncode(const std::string& s)
	{
		std::string o;
		static const char* hex = "0123456789ABCDEF";
		for (unsigned char c : s)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
				o.push_back(static_cast<char>(c));
			else if (c == ' ')
				o += "%20";
			else
			{
				o.push_back('%');
				o.push_back(hex[c >> 4]);
				o.push_back(hex[c & 15]);
			}
		}
		return o;
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
			if (c == '\\' && k < json.size())
			{
				char e = json[k++];
				if (e == 'n') out.push_back('\n');
				else if (e == 't') out.push_back('\t');
				else if (e == 'u' && k + 3 < json.size())
					k += 4;
				else
					out.push_back(e);
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

	void ParseStringArray(const std::string& body, std::vector<std::string>& out)
	{
		out.clear();
		size_t p = 0;
		while (p < body.size())
		{
			size_t q = body.find('"', p);
			if (q == std::string::npos) break;
			++q;
			std::string s;
			while (q < body.size())
			{
				char c = body[q++];
				if (c == '\\' && q < body.size()) { s.push_back(body[q++]); continue; }
				if (c == '"') break;
				s.push_back(c);
			}
			if (!s.empty()) out.push_back(s);
			p = q;
		}
	}

	void MergeLoc(std::unordered_map<int, Entry>& byId, int id, bool currency,
		LocKind kind, const std::string& where, int count)
	{
		if (id <= 0 || count < 0)
			return;
		const int key = currency ? -id : id;
		Entry& e = byId[key];
		e.id = id;
		e.isCurrency = currency;
		if (count == 0)
			return;
		e.total += count;
		for (LocQty& l : e.locs)
		{
			if (l.kind == kind && l.where == where)
			{
				l.count += count;
				return;
			}
		}
		LocQty l;
		l.kind = kind;
		l.where = where;
		l.count = count;
		e.locs.push_back(std::move(l));
	}

	void MergeMap(std::unordered_map<int, Entry>& dst, const std::unordered_map<int, Entry>& src)
	{
		for (const auto& kv : src)
		{
			const Entry& s = kv.second;
			for (const LocQty& l : s.locs)
				MergeLoc(dst, s.id, s.isCurrency, l.kind, l.where, l.count);
		}
	}

	Snapshot SnapshotFromMap(std::unordered_map<int, Entry>& byId, const char* status,
		int charCount, int charBagsOk, bool ok, bool charsPending)
	{
		Snapshot snap;
		snap.ok = ok;
		snap.charsPending = charsPending;
		snap.charCount = charCount;
		snap.charBagsOk = charBagsOk;
		snap.status = status ? status : "";
		snap.fetchedAt = GetTickCount();
		snap.entries.reserve(byId.size());
		for (auto& kv : byId)
		{
			Entry& e = kv.second;
			e.name = LookupName(kv.first, e.id, e.isCurrency);
			std::sort(e.locs.begin(), e.locs.end(),
				[](const LocQty& a, const LocQty& b) {
					if (a.kind != b.kind) return a.kind < b.kind;
					return a.where < b.where;
				});
			for (const LocQty& l : e.locs)
			{
				if (l.kind == Loc_Character)
				{
					++snap.characterLocItems;
					break;
				}
			}
			snap.entries.push_back(std::move(e));
		}
		byId.clear();
		std::sort(snap.entries.begin(), snap.entries.end(),
			[](const Entry& a, const Entry& b) {
				const size_t n = std::min(a.name.size(), b.name.size());
				for (size_t i = 0; i < n; ++i)
				{
					const int ca = std::tolower(static_cast<unsigned char>(a.name[i]));
					const int cb = std::tolower(static_cast<unsigned char>(b.name[i]));
					if (ca != cb) return ca < cb;
				}
				return a.name.size() < b.name.size();
			});
		return snap;
	}

	/* Non-destructive publish copy. */
	void Publish(const std::unordered_map<int, Entry>& byId, const char* status,
		int charCount, int charBagsOk, bool ok, bool charsPending,
		const std::vector<SlotSection>* sections)
	{
		std::unordered_map<int, Entry> copy = byId;
		Snapshot snap = SnapshotFromMap(copy, status, charCount, charBagsOk, ok, charsPending);
		if (sections)
			snap.sections = *sections;
		std::lock_guard<std::mutex> lock(gMu);
		gSnap = std::move(snap);
		gGen.fetch_add(1);
	}

	void ResolveMissingNames(const std::unordered_map<int, Entry>& byId, const char* apiKey)
	{
		std::vector<int> curIds;
		std::vector<int> itemIds;
		{
			std::lock_guard<std::mutex> lock(gNameMu);
			for (const auto& kv : byId)
			{
				if (kv.second.isCurrency)
				{
					/* Resolve when name or icon is missing (icons skipped if names cached). */
					if (!gNames.count(kv.first) || !Gw2Icons::HasCurrencyIcon(kv.second.id))
						curIds.push_back(kv.second.id);
					continue;
				}
				if (gNames.count(kv.first)) continue;
				itemIds.push_back(kv.second.id);
			}
		}
		bool saved = false;
		for (size_t i = 0; i < curIds.size() && !gCancel; i += static_cast<size_t>(kItemBatch))
		{
			std::string path = "/v2/currencies?ids=";
			const size_t end = std::min(i + static_cast<size_t>(kItemBatch), curIds.size());
			for (size_t j = i; j < end; ++j)
			{
				if (j > i) path += ',';
				path += std::to_string(curIds[j]);
			}
			auto r = Gw2Http::Api(path.c_str(), apiKey, kHttpTimeoutMs);
			if (!r.ok) continue;
			size_t p = 0;
			while (p < r.body.size())
			{
				size_t brace = r.body.find('{', p);
				if (brace == std::string::npos) break;
				size_t e = JsonObjectEnd(r.body, brace);
				if (e == std::string::npos) break;
				long long id = JsonIntAfterKey(r.body, "id", brace);
				std::string name = JsonStringAfterKey(r.body, "name", brace);
				if (id > 0)
				{
					if (!name.empty())
					{
						RememberName(static_cast<int>(-id), name);
						saved = true;
					}
					Gw2Icons::RememberCurrencyIconFromJson(static_cast<int>(id), r.body.c_str(), brace, e);
				}
				p = e + 1;
			}
		}
		for (size_t i = 0; i < itemIds.size() && !gCancel; i += static_cast<size_t>(kItemBatch))
		{
			std::string path = "/v2/items?ids=";
			const size_t end = std::min(i + static_cast<size_t>(kItemBatch), itemIds.size());
			for (size_t j = i; j < end; ++j)
			{
				if (j > i) path += ',';
				path += std::to_string(itemIds[j]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (!r.ok) continue;
			size_t p = 0;
			while (p < r.body.size())
			{
				size_t brace = r.body.find('{', p);
				if (brace == std::string::npos) break;
				size_t e = JsonObjectEnd(r.body, brace);
				if (e == std::string::npos) break;
				long long id = JsonIntAfterKey(r.body, "id", brace);
				std::string name = JsonStringAfterKey(r.body, "name", brace);
				if (id > 0 && !name.empty())
				{
					RememberName(static_cast<int>(id), name);
					Gw2Icons::RememberIconFromJson(static_cast<int>(id), r.body.c_str(), brace, e);
					saved = true;
				}
				p = e + 1;
			}
		}
		if (saved) SaveNames();
	}
} // namespace WalletDetail
