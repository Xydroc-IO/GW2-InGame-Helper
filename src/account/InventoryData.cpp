#include "InventoryData.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "JsonView.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kHttpTimeoutMs = 8000;
	constexpr DWORD kTtlMs = 5u * 60u * 1000u;

	struct LocQty
	{
		InventoryData::LocKind kind = InventoryData::LocKind::Bank;
		std::string where;
		int count = 0;
	};

	struct Entry
	{
		int id = 0;
		int total = 0;
		std::vector<LocQty> locs;
	};

	struct Snapshot
	{
		bool ok = false;
		std::string status = "Not loaded.";
		std::unordered_map<int, Entry> byId;
		DWORD fetchedAt = 0;
	};

	std::mutex gMu;
	Snapshot gSnap;
	Snapshot gPending;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gPendingReady{false};
	HANDLE gThread = nullptr;

	size_t JsonObjectEnd(const std::string& json, size_t openBrace)
	{
		return JsonView::ObjectEnd(json, openBrace);
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
	{
		return JsonView::IntAfterKey(json, key, from);
	}

	void MergeLoc(std::unordered_map<int, Entry>& byId, int id,
		InventoryData::LocKind kind, const std::string& where, int count)
	{
		if (id <= 0 || count <= 0) return;
		Entry& e = byId[id];
		e.id = id;
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

	void CollectSlots(const std::string& body, std::unordered_map<int, Entry>& byId,
		InventoryData::LocKind kind, const std::string& where)
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
					/* bag wrapper - skip */
				}
				else if (hasCount && count > 0)
					MergeLoc(byId, static_cast<int>(id), kind, where, static_cast<int>(count));
				else if (!hasSize && !hasCount)
					MergeLoc(byId, static_cast<int>(id), kind, where, 1);
			}
			p = end + 1;
		}
	}

	void CollectEquipment(const std::string& body, std::unordered_map<int, Entry>& byId,
		const std::string& charName)
	{
		size_t p = 0;
		while (p < body.size())
		{
			size_t brace = body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(body, "id", brace);
			if (id > 0 && body.find("\"slot\"", brace) < end)
				MergeLoc(byId, static_cast<int>(id), InventoryData::LocKind::Equipment,
					charName + " (worn)", 1);
			p = end + 1;
		}
	}

	void ParseCharNames(const std::string& body, std::vector<std::string>& names)
	{
		names.clear();
		bool inStr = false, esc = false;
		std::string cur;
		for (size_t i = 0; i < body.size(); ++i)
		{
			char c = body[i];
			if (inStr)
			{
				if (esc) { cur.push_back(c); esc = false; }
				else if (c == '\\') esc = true;
				else if (c == '"')
				{
					inStr = false;
					if (!cur.empty())
						names.push_back(cur);
					cur.clear();
				}
				else cur.push_back(c);
			}
			else if (c == '"')
				inStr = true;
		}
	}

	std::string UrlEncodeName(const std::string& name)
	{
		std::string o;
		for (unsigned char c : name)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
				o.push_back(static_cast<char>(c));
			else if (c == ' ')
				o += "%20";
			else
			{
				char buf[8];
				std::snprintf(buf, sizeof(buf), "%%%02X", c);
				o += buf;
			}
		}
		return o;
	}

	DWORD WINAPI LoadProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		Snapshot snap;
		snap.status = "Loading inventory...";

		if (!G::Gw2ApiKey[0])
		{
			snap.status = "API key required.";
			snap.ok = false;
		}
		else
		{
			const char* key = G::Gw2ApiKey;
			auto mats = Gw2Http::Api("/v2/account/materials", key, kHttpTimeoutMs);
			auto bank = Gw2Http::Api("/v2/account/bank", key, kHttpTimeoutMs);
			auto shared = Gw2Http::Api("/v2/account/inventory", key, kHttpTimeoutMs);
			if (mats.ok)
				CollectSlots(mats.body, snap.byId, InventoryData::LocKind::Materials, "Materials");
			if (bank.ok)
				CollectSlots(bank.body, snap.byId, InventoryData::LocKind::Bank, "Bank");
			if (shared.ok)
				CollectSlots(shared.body, snap.byId, InventoryData::LocKind::Shared, "Shared");

			auto chars = Gw2Http::Api("/v2/characters", key, kHttpTimeoutMs);
			std::vector<std::string> names;
			if (chars.ok)
				ParseCharNames(chars.body, names);
			const size_t maxChars = (std::min)(names.size(), size_t{24});
			for (size_t i = 0; i < maxChars; ++i)
			{
				const std::string enc = UrlEncodeName(names[i]);
				std::string invPath = "/v2/characters/" + enc + "/inventory";
				std::string eqPath = "/v2/characters/" + enc + "/equipment";
				auto inv = Gw2Http::Api(invPath.c_str(), key, kHttpTimeoutMs);
				auto eq = Gw2Http::Api(eqPath.c_str(), key, kHttpTimeoutMs);
				if (inv.ok)
					CollectSlots(inv.body, snap.byId, InventoryData::LocKind::Character, names[i]);
				if (eq.ok)
					CollectEquipment(eq.body, snap.byId, names[i]);
			}

			snap.ok = mats.ok || bank.ok || shared.ok || !snap.byId.empty();
			snap.status = snap.ok
				? ("Indexed " + std::to_string(snap.byId.size()) + " unique items.")
				: "Inventory fetch failed.";
			snap.fetchedAt = GetTickCount();
		}

		{
			std::lock_guard<std::mutex> lock(gMu);
			gPending = std::move(snap);
			gPendingReady = true;
			gBusy = false;
		}
		return 0;
	}
}

void InventoryData::RefreshIfNeeded(bool force)
{
	if (!force)
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (gBusy)
			return;
		if (gSnap.ok && gSnap.fetchedAt != 0 &&
			(GetTickCount() - gSnap.fetchedAt) < kTtlMs)
			return;
	}
	if (gBusy.exchange(true))
		return;
	if (gThread)
	{
		WaitForSingleObject(gThread, 0);
		CloseHandle(gThread);
		gThread = nullptr;
	}
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!gSnap.ok)
			gSnap.status = "Loading...";
		else
			gSnap.status = "Refreshing...";
	}
	gThread = CreateThread(nullptr, 0, LoadProc, nullptr, 0, nullptr);
	if (!gThread)
	{
		gBusy = false;
		std::lock_guard<std::mutex> lock(gMu);
		gSnap.status = "Could not start inventory loader.";
	}
}

void InventoryData::Tick()
{
	if (!gPendingReady)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	if (!gPendingReady)
		return;
	gSnap = std::move(gPending);
	gPending = {};
	gPendingReady = false;
	if (gThread)
	{
		WaitForSingleObject(gThread, 0);
		CloseHandle(gThread);
		gThread = nullptr;
	}
}

bool InventoryData::Busy()
{
	return gBusy.load();
}

bool InventoryData::Ready()
{
	std::lock_guard<std::mutex> lock(gMu);
	return gSnap.ok;
}

const char* InventoryData::Status()
{
	static char buf[256];
	std::lock_guard<std::mutex> lock(gMu);
	std::snprintf(buf, sizeof(buf), "%s", gSnap.status.c_str());
	return buf;
}

int InventoryData::OwnedCount(int itemId)
{
	if (itemId <= 0)
		return 0;
	std::lock_guard<std::mutex> lock(gMu);
	auto it = gSnap.byId.find(itemId);
	return it == gSnap.byId.end() ? 0 : it->second.total;
}

void InventoryData::Locations(int itemId, std::vector<Location>& out)
{
	out.clear();
	if (itemId <= 0)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	auto it = gSnap.byId.find(itemId);
	if (it == gSnap.byId.end())
		return;
	for (const LocQty& l : it->second.locs)
	{
		Location loc;
		loc.kind = l.kind;
		loc.where = l.where;
		loc.count = l.count;
		out.push_back(std::move(loc));
	}
}

void InventoryData::FillOwnedMap(std::unordered_map<int, int>& owned)
{
	owned.clear();
	std::lock_guard<std::mutex> lock(gMu);
	for (const auto& kv : gSnap.byId)
		owned[kv.first] = kv.second.total;
}

size_t InventoryData::UniqueItemCount()
{
	std::lock_guard<std::mutex> lock(gMu);
	return gSnap.byId.size();
}

size_t InventoryData::TotalStackCount()
{
	std::lock_guard<std::mutex> lock(gMu);
	size_t n = 0;
	for (const auto& kv : gSnap.byId)
		n += static_cast<size_t>(kv.second.total);
	return n;
}

unsigned InventoryData::FetchedAtMs()
{
	std::lock_guard<std::mutex> lock(gMu);
	return gSnap.fetchedAt;
}
