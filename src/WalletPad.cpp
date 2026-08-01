#include "WalletPad.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

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
	constexpr int kHttpTimeoutMs = 2500;
	constexpr int kCharTimeoutMs = 2000;
	constexpr DWORD kCacheTtlMs = 5 * 60 * 1000; /* soft TTL — still show instantly */
	constexpr int kItemBatch = 200;
	constexpr int kMaxChars = 64;
	constexpr int kCharWorkers = 6;

	enum LocKind : int
	{
		Loc_Wallet = 0,
		Loc_Materials,
		Loc_Bank,
		Loc_Shared,
		Loc_Character,
		Loc_Count
	};

	const char* kLocLabels[] = {
		"All locations", "Wallet", "Materials", "Bank", "Shared", "Characters"
	};

	struct LocQty
	{
		LocKind kind = Loc_Bank;
		std::string where;
		int count = 0;
	};

	struct Entry
	{
		int id = 0;
		bool isCurrency = false;
		std::string name;
		int total = 0;
		std::vector<LocQty> locs;
	};

	struct Snapshot
	{
		bool ok = false;
		bool noKey = false;
		bool scopeFail = false;
		std::string status;
		std::vector<Entry> entries;
		int charCount = 0;
		DWORD fetchedAt = 0;
	};

	std::mutex gMu;
	Snapshot gSnap;
	std::atomic<unsigned> gGen{0};
	unsigned gDrawnGen = 0;
	Snapshot gDraw; /* UI copy — refreshed only when gGen changes */

	std::atomic<bool> gBusy{false};
	std::atomic<bool> gCancel{false};
	HANDLE gMasterThread = nullptr;
	bool gFocus = false;
	bool gPlaceOnce = false;
	char gFilter[96] = {};
	int gLocFilter = 0; /* 0=All … 5=Characters — never touch from worker */

	/* Persistent id → name (currency keys stored negative). */
	std::mutex gNameMu;
	std::unordered_map<int, std::string> gNames;
	bool gNamesLoaded = false;

	std::wstring NamesPathW()
	{
		return AddonPaths::DataDir() + L"\\stash-names.cache";
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

	using QtyMap = std::unordered_map<int, int>;

	void CollectSlots(const std::string& body, QtyMap& m)
	{
		size_t p = 0;
		while (p < body.size())
		{
			size_t brace = body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(body, "id", brace);
			if (id > 0)
			{
				const bool hasCount = body.find("\"count\"", brace) < end;
				const bool hasSize = body.find("\"size\"", brace) < end;
				const bool hasInventory = body.find("\"inventory\"", brace) < end;
				long long count = JsonIntAfterKey(body, "count", brace);
				if (hasInventory && hasSize)
				{
					/* bag wrapper */
				}
				else if (hasCount && count > 0)
					m[static_cast<int>(id)] += static_cast<int>(count);
				else if (!hasSize && !hasCount)
					m[static_cast<int>(id)] += 1; /* equipment-style */
			}
			p = end + 1;
		}
	}

	void MergeLoc(std::unordered_map<int, Entry>& byId, int id, bool currency,
		LocKind kind, const std::string& where, int count)
	{
		if (id <= 0 || count <= 0) return;
		const int key = currency ? -id : id;
		Entry& e = byId[key];
		e.id = id;
		e.isCurrency = currency;
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
		int charCount, bool ok)
	{
		Snapshot snap;
		snap.ok = ok;
		snap.charCount = charCount;
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
			snap.entries.push_back(std::move(e));
		}
		byId.clear();
		/* Rebuild byId not needed — caller keeps separate map. We moved entries out.
		   Fix: SnapshotFromMap shouldn't clear caller's map via move of entries from
		   references into byId values - we moved e out of byId. Caller must not reuse. */
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
		int charCount, bool ok)
	{
		std::unordered_map<int, Entry> copy = byId;
		Snapshot snap = SnapshotFromMap(copy, status, charCount, ok);
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
				if (gNames.count(kv.first)) continue;
				if (kv.second.isCurrency) curIds.push_back(kv.second.id);
				else itemIds.push_back(kv.second.id);
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
				if (id > 0 && !name.empty())
				{
					RememberName(static_cast<int>(-id), name);
					saved = true;
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
					saved = true;
				}
				p = e + 1;
			}
		}
		if (saved) SaveNames();
	}

	struct AccPack
	{
		std::unordered_map<int, Entry> map;
		std::mutex mu;
		std::string note;
		bool walletOk = false;
		bool scopeFail = false;
		bool noKey = false;
	};

	struct CharJob
	{
		std::vector<std::string> names;
		std::atomic<size_t> next{0};
		std::unordered_map<int, Entry> map;
		std::mutex mu;
		const char* key = nullptr;
	};

	DWORD WINAPI CharWorker(void* p)
	{
		CharJob* job = static_cast<CharJob*>(p);
		for (;;)
		{
			if (gCancel) break;
			const size_t i = job->next.fetch_add(1);
			if (i >= job->names.size()) break;
			const std::string& name = job->names[i];
			std::string path = "/v2/characters/";
			path += UrlEncode(name);
			path += "/inventory";
			auto inv = Gw2Http::Api(path.c_str(), job->key, kCharTimeoutMs);
			if (!inv.ok) continue;
			QtyMap qty;
			CollectSlots(inv.body, qty);
			std::unordered_map<int, Entry> local;
			for (const auto& kv : qty)
				MergeLoc(local, kv.first, false, Loc_Character, name, kv.second);
			std::lock_guard<std::mutex> lock(job->mu);
			MergeMap(job->map, local);
		}
		return 0;
	}

	DWORD WINAPI AccWallet(void* p)
	{
		AccPack* a = static_cast<AccPack*>(p);
		auto w = Gw2Http::Api("/v2/account/wallet", G::Gw2ApiKey, kHttpTimeoutMs);
		if (!w.ok && (w.status == 401 || w.status == 403))
		{
			a->scopeFail = true;
			return 0;
		}
		if (!w.ok) return 0;
		a->walletOk = true;
		std::unordered_map<int, Entry> local;
		size_t pos = 0;
		while (pos < w.body.size())
		{
			size_t brace = w.body.find('{', pos);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(w.body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(w.body, "id", brace);
			long long val = JsonIntAfterKey(w.body, "value", brace);
			if (id > 0 && val > 0)
				MergeLoc(local, static_cast<int>(id), true, Loc_Wallet, "Wallet",
					static_cast<int>(val > 2147483647 ? 2147483647 : val));
			pos = end + 1;
		}
		std::lock_guard<std::mutex> lock(a->mu);
		MergeMap(a->map, local);
		return 0;
	}

	DWORD WINAPI AccMats(void* p)
	{
		AccPack* a = static_cast<AccPack*>(p);
		auto r = Gw2Http::Api("/v2/account/materials", G::Gw2ApiKey, kHttpTimeoutMs);
		if (!r.ok)
		{
			if (r.status == 401 || r.status == 403)
			{
				std::lock_guard<std::mutex> lock(a->mu);
				a->note += "Need inventories. ";
			}
			return 0;
		}
		std::unordered_map<int, Entry> local;
		size_t pos = 0;
		while (pos < r.body.size())
		{
			size_t brace = r.body.find('{', pos);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(r.body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(r.body, "id", brace);
			long long count = JsonIntAfterKey(r.body, "count", brace);
			if (id > 0 && count > 0)
				MergeLoc(local, static_cast<int>(id), false, Loc_Materials, "Materials",
					static_cast<int>(count));
			pos = end + 1;
		}
		std::lock_guard<std::mutex> lock(a->mu);
		MergeMap(a->map, local);
		return 0;
	}

	DWORD WINAPI AccBank(void* p)
	{
		AccPack* a = static_cast<AccPack*>(p);
		auto r = Gw2Http::Api("/v2/account/bank", G::Gw2ApiKey, kHttpTimeoutMs);
		if (!r.ok) return 0;
		QtyMap qty;
		CollectSlots(r.body, qty);
		std::unordered_map<int, Entry> local;
		for (const auto& kv : qty)
			MergeLoc(local, kv.first, false, Loc_Bank, "Bank", kv.second);
		std::lock_guard<std::mutex> lock(a->mu);
		MergeMap(a->map, local);
		return 0;
	}

	DWORD WINAPI AccShared(void* p)
	{
		AccPack* a = static_cast<AccPack*>(p);
		auto r = Gw2Http::Api("/v2/account/inventory", G::Gw2ApiKey, kHttpTimeoutMs);
		if (!r.ok) return 0;
		QtyMap qty;
		CollectSlots(r.body, qty);
		std::unordered_map<int, Entry> local;
		for (const auto& kv : qty)
			MergeLoc(local, kv.first, false, Loc_Shared, "Shared", kv.second);
		std::lock_guard<std::mutex> lock(a->mu);
		MergeMap(a->map, local);
		return 0;
	}

	DWORD WINAPI MasterProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		LoadNames();

		if (!G::Gw2ApiKey[0])
		{
			Snapshot s;
			s.noKey = true;
			s.status = "Add an API key in Nexus Options.";
			std::lock_guard<std::mutex> lock(gMu);
			gSnap = std::move(s);
			gGen.fetch_add(1);
			gBusy = false;
			return 0;
		}

		AccPack acc;
		HANDLE th[4]{};
		th[0] = CreateThread(nullptr, 0, AccWallet, &acc, 0, nullptr);
		th[1] = CreateThread(nullptr, 0, AccMats, &acc, 0, nullptr);
		th[2] = CreateThread(nullptr, 0, AccBank, &acc, 0, nullptr);
		th[3] = CreateThread(nullptr, 0, AccShared, &acc, 0, nullptr);
		for (HANDLE h : th)
		{
			if (h)
			{
				WaitForSingleObject(h, 15000);
				CloseHandle(h);
			}
		}

		if (acc.scopeFail || (!acc.walletOk && acc.map.empty()))
		{
			Snapshot s;
			s.scopeFail = acc.scopeFail;
			s.status = acc.scopeFail
				? "Key needs account + wallet (+ inventories, characters)."
				: "Wallet request failed.";
			std::lock_guard<std::mutex> lock(gMu);
			gSnap = std::move(s);
			gGen.fetch_add(1);
			gBusy = false;
			return 0;
		}

		/* Fast first paint: account stash only. */
		{
			std::lock_guard<std::mutex> lock(acc.mu);
			ResolveMissingNames(acc.map, G::Gw2ApiKey);
			Publish(acc.map, "Account stash ready — loading characters…", 0, true);
		}

		if (gCancel)
		{
			gBusy = false;
			return 0;
		}

		CharJob job;
		job.key = G::Gw2ApiKey;
		auto chars = Gw2Http::Api("/v2/characters", G::Gw2ApiKey, kHttpTimeoutMs);
		if (chars.ok)
		{
			ParseStringArray(chars.body, job.names);
			if (job.names.size() > static_cast<size_t>(kMaxChars))
				job.names.resize(static_cast<size_t>(kMaxChars));

			HANDLE cw[kCharWorkers]{};
			const int nWorkers = std::min(kCharWorkers, std::max(1, static_cast<int>(job.names.size())));
			for (int i = 0; i < nWorkers; ++i)
				cw[i] = CreateThread(nullptr, 0, CharWorker, &job, 0, nullptr);
			for (int i = 0; i < nWorkers; ++i)
			{
				if (cw[i])
				{
					WaitForSingleObject(cw[i], 60000);
					CloseHandle(cw[i]);
				}
			}

			std::lock_guard<std::mutex> lock(acc.mu);
			std::lock_guard<std::mutex> lock2(job.mu);
			MergeMap(acc.map, job.map);
			ResolveMissingNames(acc.map, G::Gw2ApiKey);
			char st[160];
			std::snprintf(st, sizeof(st), "%d unique · %d toons. %s",
				static_cast<int>(acc.map.size()), static_cast<int>(job.names.size()),
				acc.note.c_str());
			Publish(acc.map, st, static_cast<int>(job.names.size()), true);
		}
		else
		{
			std::lock_guard<std::mutex> lock(acc.mu);
			std::string st = "Account stash loaded.";
			if (chars.status == 401 || chars.status == 403)
				st += " Enable characters scope for per-toon bags.";
			Publish(acc.map, st.c_str(), 0, true);
		}

		gBusy = false;
		return 0;
	}

	void StartFetch(bool force)
	{
		LoadNames();
		if (!force)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gSnap.ok && gSnap.fetchedAt != 0)
			{
				const DWORD now = GetTickCount();
				if (now - gSnap.fetchedAt < kCacheTtlMs)
					return;
			}
		}
		if (gBusy.exchange(true))
			return;
		gCancel = false;
		if (gMasterThread)
		{
			/* Previous run should be done; don't Wait forever on UI path. */
			if (WaitForSingleObject(gMasterThread, 0) == WAIT_OBJECT_0)
			{
				CloseHandle(gMasterThread);
				gMasterThread = nullptr;
			}
			else
			{
				/* Still running — leave it; skip starting another. */
				gBusy = false;
				return;
			}
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!gSnap.ok)
				gSnap.status = "Loading…";
			else
				gSnap.status = "Refreshing in background…";
			gGen.fetch_add(1);
		}
		gMasterThread = CreateThread(nullptr, 0, MasterProc, nullptr, 0, nullptr);
		if (!gMasterThread)
		{
			gBusy = false;
			std::lock_guard<std::mutex> lock(gMu);
			gSnap.status = "Could not start fetch.";
			gGen.fetch_add(1);
		}
	}

	void SyncDrawCopy()
	{
		const unsigned gen = gGen.load();
		if (gen == gDrawnGen) return;
		std::lock_guard<std::mutex> lock(gMu);
		gDraw = gSnap;
		gDrawnGen = gGen.load();
	}

	bool MatchesFilter(const Entry& e, const char* filter, int locFilter)
	{
		if (locFilter > 0)
		{
			const LocKind want = static_cast<LocKind>(locFilter - 1);
			bool any = false;
			for (const LocQty& l : e.locs)
			{
				if (l.kind == want) { any = true; break; }
			}
			if (!any) return false;
		}
		if (!filter || !filter[0]) return true;
		auto has = [](const char* hay, const char* needle) -> bool {
			if (!hay || !needle || !needle[0]) return true;
			const size_t nlen = std::strlen(needle);
			for (const char* p = hay; *p; ++p)
			{
				size_t i = 0;
				while (i < nlen)
				{
					const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(p[i])));
					const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
					if (!p[i] || a != b) break;
					++i;
				}
				if (i == nlen) return true;
			}
			return false;
		};
		if (has(e.name.c_str(), filter)) return true;
		char idBuf[24];
		std::snprintf(idBuf, sizeof(idBuf), "%d", e.id);
		if (has(idBuf, filter)) return true;
		for (const LocQty& l : e.locs)
			if (has(l.where.c_str(), filter)) return true;
		return false;
	}
}

void WalletPad::OpenAndRefresh()
{
	G::ShowWallet = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	LoadNames();
	/* Show whatever we already have immediately; refresh only if stale/empty. */
	bool need = true;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (gSnap.ok && gSnap.fetchedAt != 0)
		{
			const DWORD now = GetTickCount();
			if (now - gSnap.fetchedAt < kCacheTtlMs)
				need = false;
		}
	}
	StartFetch(need); /* force only when nothing fresh to show */
	if (!need)
	{
		/* Optional silent background refresh when half-expired — skip for snappy UX. */
	}
}

bool WalletPad::Render()
{
	SyncDrawCopy();
	if (!G::ShowWallet)
		return false;

	const Snapshot& snap = gDraw;

	constexpr float kPadW = 420.f;
	constexpr float kPadH = 560.f;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = (io.DisplaySize.y > 100.f)
		? std::min(io.DisplaySize.y * 0.90f, 900.f)
		: 720.f;
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 280.f), ImVec2(560.f, maxH));
	/* Same ballpark as Notes — fits laptop / 1080p without eating the screen. */
	if (gPlaceOnce)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	else
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gPlaceOnce)
	{
		const float x = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x * 0.52f : 160.f;
		const float y = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y * 0.14f : 80.f;
		ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
		ImGui::SetNextWindowFocus();
		gPlaceOnce = false;
	}
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowWallet;
	if (!ImGui::Begin("Wallet & Stash##GW2InGameHelperWallet", &open))
	{
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowWallet = false;
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowWallet = false;
		Settings::SetDirty();
	}

	ImGui::TextUnformatted("Wallet & stash search");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Scopes: account, wallet, inventories, characters. Reopen uses cache when fresh.");
	ImGui::PopTextWrapPos();

	if (ImGui::Button("Refresh###gw2igh_wallet_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (gBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Updating…");
	else if (!snap.status.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", snap.status.c_str());

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_wallet_filter", "Filter: ecto, Alice, bank…",
		gFilter, sizeof(gFilter));

	/* BeginCombo — ImGui::Combo was resetting selection in this layout. */
	ImGui::SetNextItemWidth(-1.f);
	const char* preview = (gLocFilter >= 0 && gLocFilter < 6)
		? kLocLabels[gLocFilter]
		: kLocLabels[0];
	if (ImGui::BeginCombo("###gw2igh_wallet_loc", preview))
	{
		for (int i = 0; i < 6; ++i)
		{
			const bool selected = (gLocFilter == i);
			if (ImGui::Selectable(kLocLabels[i], selected))
				gLocFilter = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();

	if (snap.noKey || snap.scopeFail)
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextWrapped("%s", snap.status.c_str());
		ImGui::PopTextWrapPos();
	}
	else
	{
		int shown = 0;
		const float listH = ImGui::GetContentRegionAvail().y;
		ImGui::BeginChild("###gw2igh_wallet_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);
		const int locFilter = gLocFilter; /* stable for this frame */
		for (const Entry& e : snap.entries)
		{
			if (!MatchesFilter(e, gFilter, locFilter))
				continue;
			++shown;
			ImGui::PushID(e.isCurrency ? -e.id : e.id);

			const std::string qty = e.isCurrency && e.id == 1
				? FormatCoins(e.total)
				: FormatCount(e.total);

			const bool openNode = ImGui::TreeNodeEx("##row",
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap,
				"%s", e.name.c_str());
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", qty.c_str());

			if (openNode)
			{
				ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
					"%s #%d", e.isCurrency ? "Currency" : "Item", e.id);
				for (const LocQty& l : e.locs)
				{
					if (locFilter > 0 && l.kind != static_cast<LocKind>(locFilter - 1))
						continue;
					const std::string lq = e.isCurrency && e.id == 1
						? FormatCoins(l.count)
						: FormatCount(l.count);
					ImGui::BulletText("%s — %s", l.where.c_str(), lq.c_str());
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if (shown == 0 && !gBusy)
		{
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextWrapped(snap.ok
				? "No matches. Clear the filter or pick All locations."
				: "No data yet — click Refresh.");
			ImGui::PopTextWrapPos();
		}
		ImGui::EndChild();
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
