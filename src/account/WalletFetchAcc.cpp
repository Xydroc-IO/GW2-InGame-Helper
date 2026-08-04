#include "WalletPad.h"

#include "WalletShared.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace WalletDetail
{
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
		std::atomic<int> bagsOk{0};
		std::atomic<int> bagsFail{0};
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
			if (!inv.ok)
			{
				job->bagsFail.fetch_add(1);
				continue;
			}
			job->bagsOk.fetch_add(1);
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
			Publish(acc.map, "Account stash ready — loading characters…", 0, 0, true);
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
			char st[200];
			const int bagsOk = job.bagsOk.load();
			const int bagsFail = job.bagsFail.load();
			if (bagsFail > 0 && bagsOk == 0)
			{
				std::snprintf(st, sizeof(st),
					"%d unique · %d toons listed, 0 bags loaded (need characters + inventories?). %s",
					static_cast<int>(acc.map.size()), static_cast<int>(job.names.size()),
					acc.note.c_str());
			}
			else if (bagsFail > 0)
			{
				std::snprintf(st, sizeof(st),
					"%d unique · %d/%d toon bags (%d failed). %s",
					static_cast<int>(acc.map.size()), bagsOk,
					static_cast<int>(job.names.size()), bagsFail, acc.note.c_str());
			}
			else
			{
				std::snprintf(st, sizeof(st), "%d unique · %d toons. %s",
					static_cast<int>(acc.map.size()), static_cast<int>(job.names.size()),
					acc.note.c_str());
			}
			Publish(acc.map, st, static_cast<int>(job.names.size()), bagsOk, true);
		}
		else
		{
			std::lock_guard<std::mutex> lock(acc.mu);
			std::string st = "Account stash loaded.";
			if (chars.status == 401 || chars.status == 403)
				st += " Enable characters scope for per-toon bags.";
			Publish(acc.map, st.c_str(), 0, 0, true);
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

} // namespace WalletDetail
