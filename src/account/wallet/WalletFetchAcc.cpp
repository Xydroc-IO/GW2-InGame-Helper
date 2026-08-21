#include "WalletPad.h"

#include "WalletShared.h"

#include "BgFetch.h"
#include "Globals.h"
#include "Gw2Catalog.h"
#include "Gw2Http.h"
#include "Settings.h"

#include <algorithm>
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
		std::vector<SlotSection> sections;
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
		std::atomic<int> done{0}; /* inv+eq attempts finished (ok or fail) */
		std::unordered_map<int, Entry> map;
		std::vector<SlotSection> sections;
		std::mutex mu;
		const char* key = nullptr;
	};

	void SortSections(std::vector<SlotSection>& secs)
	{
		std::stable_sort(secs.begin(), secs.end(),
			[](const SlotSection& a, const SlotSection& b) {
				return a.kind < b.kind;
			});
	}

	void CopySections(AccPack& acc, CharJob* job, std::vector<SlotSection>& out)
	{
		out.clear();
		{
			std::lock_guard<std::mutex> lock(acc.mu);
			out = acc.sections;
		}
		if (job)
		{
			std::lock_guard<std::mutex> lock(job->mu);
			out.insert(out.end(), job->sections.begin(), job->sections.end());
		}
		SortSections(out);
	}

	DWORD WINAPI CharWorker(void* p)
	{
		CharJob* job = static_cast<CharJob*>(p);
		for (;;)
		{
			if (gCancel) break;
			while (!BgFetch::AllowWork(BgFetch::Channel::Wallet) && !gCancel)
				Sleep(40);
			if (gCancel) break;
			const size_t i = job->next.fetch_add(1);
			if (i >= job->names.size()) break;
			const std::string& name = job->names[i];

			/* Sequential inv + equipment — never hand a stack CharHttpPair to a
			   sibling thread. Gw2Http::Api can exceed kCharTimeoutMs (budget wait
			   + retries), so a timed Wait on a dual-fetch thread UAF'd the pair
			   (ArcDPS: CharFetchInv after Api return). */
			std::string invPath = "/v2/characters/";
			invPath += UrlEncode(name);
			invPath += "/inventory";
			std::string eqPath = "/v2/characters/";
			eqPath += UrlEncode(name);
			eqPath += "/equipment";

			auto inv = Gw2Http::Api(invPath.c_str(), job->key, kCharTimeoutMs);
			auto eq = Gw2Http::Api(eqPath.c_str(), job->key, kCharTimeoutMs);

			if (!inv.ok)
			{
				job->bagsFail.fetch_add(1);
				job->done.fetch_add(1);
				continue;
			}
			job->bagsOk.fetch_add(1);
			QtyMap qty;
			CollectSlots(inv.body, qty);
			std::vector<SlotSection> bags;
			CollectCharBagSections(inv.body, name, bags);

			if (eq.ok)
			{
				size_t ppos = 0;
				while (ppos < eq.body.size())
				{
					size_t brace = eq.body.find('{', ppos);
					if (brace == std::string::npos) break;
					size_t end = JsonObjectEnd(eq.body, brace);
					if (end == std::string::npos) break;
					/* Equipment entries have "slot"; skip nested upgrade objects. */
					if (eq.body.find("\"slot\"", brace) < end)
					{
						long long id = JsonIntAfterKey(eq.body, "id", brace);
						if (id > 0)
							qty[static_cast<int>(id)] += 1;
					}
					ppos = brace + 1;
				}
			}

			{
				std::lock_guard<std::mutex> lock(job->mu);
				if (!qty.empty())
				{
					std::unordered_map<int, Entry> local;
					for (const auto& kv : qty)
						MergeLoc(local, kv.first, false, Loc_Character, name, kv.second);
					MergeMap(job->map, local);
				}
				if (!bags.empty())
					job->sections.insert(job->sections.end(), bags.begin(), bags.end());
			}
			job->done.fetch_add(1);
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
		/* Full materials storage is a large JSON; 8s often dies on Wine. */
		auto r = Gw2Http::Api("/v2/account/materials", G::Gw2ApiKey, 20000);
		if (!r.ok)
		{
			std::lock_guard<std::mutex> lock(a->mu);
			if (r.status == 401 || r.status == 403)
				a->note += "Need inventories. ";
			else
				a->note += "Materials failed. ";
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
			long long id = IntKeyInObject(r.body, brace, end, "id");
			long long count = IntKeyInObject(r.body, brace, end, "count");
			if (id > 0 && count > 0)
				MergeLoc(local, static_cast<int>(id), false, Loc_Materials, "Materials",
					static_cast<int>(count > 2147483647LL ? 2147483647LL : count));
			pos = brace + 1;
		}
		std::string catBody;
		if (!Gw2Catalog::HasMaterialCategories())
		{
			auto list = Gw2Http::Api("/v2/materials", nullptr, kHttpTimeoutMs);
			std::vector<int> ids;
			if (list.ok)
			{
				size_t i = 0;
				while (i < list.body.size())
				{
					if (list.body[i] < '0' || list.body[i] > '9')
					{
						++i;
						continue;
					}
					int id = 0;
					while (i < list.body.size() && list.body[i] >= '0' && list.body[i] <= '9')
					{
						id = id * 10 + (list.body[i] - '0');
						++i;
					}
					if (id > 0)
						ids.push_back(id);
				}
			}
			if (!ids.empty())
			{
				std::string path = "/v2/materials?ids=";
				for (size_t j = 0; j < ids.size(); ++j)
				{
					if (j)
						path += ',';
					path += std::to_string(ids[j]);
				}
				auto cats = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
				if (cats.ok)
					catBody = std::move(cats.body);
			}
		}
		std::vector<SlotSection> secs;
		CollectMaterialSections(r.body, catBody, secs);
		std::lock_guard<std::mutex> lock(a->mu);
		MergeMap(a->map, local);
		a->sections.insert(a->sections.end(), secs.begin(), secs.end());
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
		std::vector<SlotSection> secs;
		CollectBankTabs(r.body, secs);
		std::lock_guard<std::mutex> lock(a->mu);
		MergeMap(a->map, local);
		a->sections.insert(a->sections.end(), secs.begin(), secs.end());
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
		std::vector<SlotSection> secs;
		CollectSharedSlots(r.body, secs);
		std::lock_guard<std::mutex> lock(a->mu);
		MergeMap(a->map, local);
		a->sections.insert(a->sections.end(), secs.begin(), secs.end());
		return 0;
	}

	DWORD WINAPI MasterProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		LoadNames();
		struct UnpinIfClosed
		{
			~UnpinIfClosed()
			{
				if (!G::ShowWallet)
					BgFetch::SetWanted(BgFetch::Channel::Wallet, false);
			}
		} unpin;

		if (!G::Gw2ApiKey[0])
		{
			Snapshot s;
			s.noKey = true;
			s.status = "Add an API key in Settings (helper side rail).";
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
				/* AccPack is stack-owned — must join for real. Timed waits UAF if
				   Api budget/retries outlast the wait (same class as CharFetchInv). */
				WaitForSingleObject(h, INFINITE);
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
			std::unordered_map<int, Entry> view;
			std::vector<SlotSection> secs;
			{
				std::lock_guard<std::mutex> lock(acc.mu);
				view = acc.map;
				secs = acc.sections;
			}
			SortSections(secs);
			ResolveMissingNames(view, G::Gw2ApiKey, &secs);
			Publish(view, "Account stash ready - loading characters...", 0, 0, true, true,
				&secs);
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

			const int nChars = static_cast<int>(job.names.size());
			int lastDone = -1;
			for (;;)
			{
				const DWORD wait = WaitForMultipleObjects(
					static_cast<DWORD>(nWorkers), cw, TRUE, 300);
				const int done = job.done.load();
				const bool finished = (wait == WAIT_OBJECT_0) || (wait == WAIT_FAILED);
				if (gCancel)
					break;
				/* Progressive UI: publish as each toon lands (throttle by done count). */
				if (done != lastDone || finished)
				{
					lastDone = done;
					std::unordered_map<int, Entry> view;
					{
						std::lock_guard<std::mutex> lock(acc.mu);
						view = acc.map;
					}
					{
						std::lock_guard<std::mutex> lock(job.mu);
						MergeMap(view, job.map);
					}
					const int bagsOk = job.bagsOk.load();
					std::vector<SlotSection> secs;
					CopySections(acc, &job, secs);
					/* Skip name HTTP on mid-progress ticks unless finishing — keeps feed snappy. */
					if (finished || (done > 0 && (done % 4) == 0))
						ResolveMissingNames(view, G::Gw2ApiKey, &secs);
					char st[200];
					if (!finished)
					{
						std::snprintf(st, sizeof(st),
							"Loading characters %d/%d...", done, nChars);
						Publish(view, st, nChars, bagsOk, true, true, &secs);
					}
				}
				if (finished)
					break;
			}

			for (int i = 0; i < nWorkers; ++i)
			{
				if (cw[i])
				{
					/* Always join — cancel only stops taking new toons; in-flight
					   HTTP still writes job on this stack. */
					WaitForSingleObject(cw[i], INFINITE);
					CloseHandle(cw[i]);
				}
			}

			std::unordered_map<int, Entry> view;
			std::vector<SlotSection> secs;
			std::string note;
			int nNames = 0;
			int bagItems = 0;
			{
				std::lock_guard<std::mutex> lock(acc.mu);
				std::lock_guard<std::mutex> lock2(job.mu);
				MergeMap(acc.map, job.map);
				acc.sections.insert(acc.sections.end(), job.sections.begin(), job.sections.end());
				view = acc.map;
				secs = acc.sections;
				note = acc.note;
				nNames = static_cast<int>(job.names.size());
				bagItems = static_cast<int>(job.map.size());
			}
			SortSections(secs);
			ResolveMissingNames(view, G::Gw2ApiKey, &secs);
			char st[240];
			const int bagsOk = job.bagsOk.load();
			const int bagsFail = job.bagsFail.load();
			if (bagsFail > 0 && bagsOk == 0)
			{
				std::snprintf(st, sizeof(st),
					"%d unique | %d toons listed, 0 bags loaded (need characters + inventories?). %s",
					static_cast<int>(view.size()), nNames, note.c_str());
			}
			else if (bagsOk > 0 && bagItems == 0)
			{
				std::snprintf(st, sizeof(st),
					"%d unique | %d/%d bag HTTP ok, 0 items parsed. %s",
					static_cast<int>(view.size()), bagsOk, nNames, note.c_str());
			}
			else if (bagsFail > 0)
			{
				std::snprintf(st, sizeof(st),
					"%d unique | %d/%d toon bags (%d failed, %d char items). %s",
					static_cast<int>(view.size()), bagsOk, nNames, bagsFail, bagItems,
					note.c_str());
			}
			else
			{
				std::snprintf(st, sizeof(st), "%d unique | %d toons | %d on characters. %s",
					static_cast<int>(view.size()), nNames, bagItems, note.c_str());
			}
			Publish(view, st, nNames, bagsOk, true, false, &secs);
		}
		else
		{
			std::unordered_map<int, Entry> view;
			std::vector<SlotSection> secs;
			std::string st = "Account stash loaded.";
			{
				std::lock_guard<std::mutex> lock(acc.mu);
				view = acc.map;
				secs = acc.sections;
			}
			if (chars.status == 401 || chars.status == 403)
				st += " Enable characters scope for per-toon bags.";
			SortSections(secs);
			Publish(view, st.c_str(), 0, 0, true, false, &secs);
		}

		gBusy = false;
		return 0;
	}

	void TickDeferredFetch()
	{
		if (!gDeferredFetch.load())
			return;
		if (!BgFetch::AllowWork(BgFetch::Channel::Wallet))
			return;
		const bool force = gDeferredForce.load();
		gDeferredFetch = false;
		StartFetch(force);
	}

	void StartFetch(bool force)
	{
		LoadNames();
		BgFetch::SetWanted(BgFetch::Channel::Wallet, true);
		if (!BgFetch::AllowWork(BgFetch::Channel::Wallet))
		{
			gDeferredFetch = true;
			gDeferredForce = force;
			return;
		}
		if (!force)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gSnap.ok && !gSnap.charsPending && gSnap.fetchedAt != 0)
			{
				const DWORD now = GetTickCount();
				if (now - gSnap.fetchedAt < kCacheTtlMs)
					return;
			}
		}
		gDeferredFetch = false;
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
				/* Still running - leave it; skip starting another. */
				gBusy = false;
				return;
			}
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!gSnap.ok)
				gSnap.status = "Loading...";
			else
				gSnap.status = "Refreshing in background...";
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
