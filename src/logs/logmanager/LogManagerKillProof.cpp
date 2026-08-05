#include "LogManagerShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace LogManagerDetail
{
	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	std::string UrlEncodeAccount(const std::string& account)
	{
		std::string out;
		out.reserve(account.size() * 3);
		for (unsigned char c : account)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == '~')
				out += static_cast<char>(c);
			else
			{
				char hex[8];
				std::snprintf(hex, sizeof(hex), "%%%02X", c);
				out += hex;
			}
		}
		return out;
	}

	/* Encounter name → token item id (from killproof.me tokens / killproofs). */
	bool BossTokenForEncounter(const std::string& encounter, int& outId, const char*& outLabel)
	{
		outId = 0;
		outLabel = nullptr;
		if (encounter.empty())
			return false;
		const std::string low = ToLowerCopy(encounter);
		struct Map
		{
			const char* needle;
			int id;
			const char* label;
		};
		static const Map kMap[] = {
			{"vale guardian", 77705, "VG"},
			{"gorseval", 77751, "Gors"},
			{"sabetha", 77728, "Sab"},
			{"slothasor", 77706, "Sloth"},
			{"matthias", 77679, "Matt"},
			{"keep construct", 78902, "KC"},
			{"xera", 78942, "Xera"},
			{"cairn", 80623, "Cairn"},
			{"mursaat", 80542, "MO"},
			{"samarog", 80269, "Sam"},
			{"deimos", 80269, "Deimos"}, /* floor fragment often used; prefer Impaled if present */
			{"soulless horror", 85993, "SH"},
			{"desmina", 85993, "SH"},
			{"river of souls", 85785, "River"},
			{"statues", 85800, "Statues"},
			{"dhuum", 85633, "Dhuum"},
			{"conjured amalgamate", 88543, "CA"},
			{"twin largos", 88860, "Largos"},
			{"qadim the peerless", 91175, "QTP"},
			{"qadim", 88645, "Qadim"},
			{"cardinal adina", 91246, "Adina"},
			{"cardinal sabir", 91270, "Sabir"},
			{"godsquall decima", 103754, "Decima"},
			{"decima", 103754, "Decima"},
			{"greer", 104047, "Greer"},
			{"ura", 103996, "Ura"},
			{"dagda", 100068, "Dagda"},
			{"cerus", 100858, "Cerus"},
			{"mai trin", 95638, "Mai"},
			{"ankka", 95982, "Ankka"},
			{"minister li", 97451, "Li"},
			{"void", 97132, "Void"},
			{"assault knight", 99165, "AK"},
			{"boneskinner", 93781, "Vial"},
		};
		for (const Map& m : kMap)
		{
			if (low.find(m.needle) != std::string::npos)
			{
				outId = m.id;
				outLabel = m.label;
				return true;
			}
		}
		return false;
	}


	bool FetchKillProofProfile(const std::string& account, KillProofCacheEntry& out)
	{
		out = KillProofCacheEntry{};
		out.fetchedAtMs = GetTickCount();
		if (account.empty())
		{
			out.missing = true;
			return false;
		}
		std::string url = "https://killproof.me/api/kp/";
		url += UrlEncodeAccount(account);
		url += "?lang=en";
		const Gw2Http::Result r = Gw2Http::Get(url.c_str(), nullptr, 10000);
		if (r.body.find("\"error\"") != std::string::npos &&
			r.body.find("Account not found") != std::string::npos)
		{
			out.missing = true;
			return false;
		}
		if (r.status == 404)
		{
			out.missing = true;
			return false;
		}
		if (!r.ok || r.body.empty())
		{
			out.error = true;
			return false;
		}
		JsonStringAfterKey(r.body.c_str(), "proof_url", out.proofUrl);
		out.li = AmountNearId(r.body.c_str(), kKpIdLi);
		out.ld = AmountNearId(r.body.c_str(), kKpIdLd);
		out.ufe = AmountNearId(r.body.c_str(), kKpIdUfe);
		/* Cache common token amounts from tokens[] + killproofs[]. */
		static const int kExtraIds[] = {
			77705, 77751, 77728, 77706, 77679, 78902, 78942, 80623, 80542, 80269,
			85993, 85785, 85800, 85633, 88543, 88860, 88645, 91175, 91246, 91270,
			103754, 104047, 103996, 100068, 100858, 95638, 95982, 97451, 97132,
			99165, 93781, 78873, 80087
		};
		for (int id : kExtraIds)
		{
			const int amt = AmountNearId(r.body.c_str(), id);
			if (amt >= 0)
				out.amounts[id] = amt;
		}
		if (out.li < 0 && out.ld < 0 && out.ufe < 0 && out.amounts.empty() && out.proofUrl.empty())
		{
			/* Private / empty profile — treat as missing rather than inventing zeros. */
			out.missing = true;
			return false;
		}
		if (out.li < 0) out.li = 0;
		if (out.ld < 0) out.ld = 0;
		if (out.ufe < 0) out.ufe = 0;
		return true;
	}

	void ApplyKillProofEntryToPlayer(PlayerInfo& p, const KillProofCacheEntry& e, const std::string& encounter)
	{
		if (e.missing)
		{
			p.kpState = 3;
			p.kpLi = p.kpLd = p.kpUfe = p.kpBoss = -1;
			p.kpBossLabel.clear();
			p.kpUrl.clear();
			return;
		}
		if (e.error)
		{
			p.kpState = 4;
			return;
		}
		p.kpLi = e.li;
		p.kpLd = e.ld;
		p.kpUfe = e.ufe;
		p.kpUrl = e.proofUrl;
		p.kpState = 2;
		int bossId = 0;
		const char* bossLabel = nullptr;
		if (BossTokenForEncounter(encounter, bossId, bossLabel))
		{
			p.kpBossLabel = bossLabel ? bossLabel : "";
			auto it = e.amounts.find(bossId);
			if (it != e.amounts.end())
				p.kpBoss = it->second;
			else
				p.kpBoss = 0;
		}
		else
		{
			p.kpBoss = -1;
			p.kpBossLabel.clear();
		}
	}

	void ApplyKillProofCacheToPlayersLocked(std::vector<PlayerInfo>& players, const std::string& encounter)
	{
		/* Caller must hold gKpCacheMu. */
		for (PlayerInfo& p : players)
		{
			if (p.account.empty())
				continue;
			const std::string key = ToLowerCopy(p.account);
			auto it = gKpCache.find(key);
			if (it == gKpCache.end())
				continue;
			ApplyKillProofEntryToPlayer(p, it->second, encounter);
		}
	}

	void ApplyKillProofCacheToAllLogsLocked()
	{
		/* Caller must hold gMu and gKpCacheMu. */
		for (LogEntry& e : gLogs)
			ApplyKillProofCacheToPlayersLocked(e.players, e.encounter);
	}

	void QueueKillProofAccountsLocked(const std::vector<PlayerInfo>& players, bool force)
	{
		/* Caller must hold gKpCacheMu. */
		const DWORD now = GetTickCount();
		constexpr DWORD kTtlMs = 60u * 60u * 1000u;
		for (const PlayerInfo& p : players)
		{
			if (p.account.empty())
				continue;
			const std::string key = ToLowerCopy(p.account);
			if (!force)
			{
				auto it = gKpCache.find(key);
				if (it != gKpCache.end() && !it->second.error &&
					(now - it->second.fetchedAtMs) < kTtlMs)
					continue;
			}
			bool queued = false;
			for (const std::string& q : gKpQueue)
			{
				if (ToLowerCopy(q) == key)
				{
					queued = true;
					break;
				}
			}
			if (!queued)
				gKpQueue.push_back(p.account);
		}
		if (force)
			gKpForce.store(true);
	}

	DWORD WINAPI KillProofWorker(LPVOID);

	DWORD WINAPI KillProofWorker(LPVOID);

	void BeginKillProofFetch(bool force)
	{
		if (force)
			gKpForce.store(true);
		if (gKillProofBusy.exchange(true))
			return;
		if (gKillProofThread)
		{
			CloseHandle(gKillProofThread);
			gKillProofThread = nullptr;
		}
		gKillProofThread = CreateThread(nullptr, 0, KillProofWorker, nullptr, 0, nullptr);
		if (!gKillProofThread)
			gKillProofBusy.store(false);
	}

	DWORD WINAPI KillProofWorker(LPVOID)
	{
		gKpForce.exchange(false);
		int done = 0;
		for (;;)
		{
			std::vector<std::string> jobs;
			{
				std::lock_guard<std::mutex> lock(gKpCacheMu);
				jobs.swap(gKpQueue);
			}
			if (jobs.empty())
				break;
			for (const std::string& account : jobs)
			{
				if (gCancel.load())
					break;
				std::snprintf(gStatus, sizeof(gStatus), "Loading KP %d… (%s)",
					done + 1, account.c_str());
				KillProofCacheEntry entry;
				FetchKillProofProfile(account, entry);
				{
					std::lock_guard<std::mutex> lockLogs(gMu);
					std::lock_guard<std::mutex> lockKp(gKpCacheMu);
					gKpCache[ToLowerCopy(account)] = entry;
					ApplyKillProofCacheToAllLogsLocked();
					gGen.fetch_add(1);
				}
				++done;
				Sleep(80);
			}
		}
		if (done > 0)
			std::snprintf(gStatus, sizeof(gStatus), "KP loaded for %d account(s).", done);
		gKillProofBusy.store(false);
		bool more = false;
		{
			std::lock_guard<std::mutex> lock(gKpCacheMu);
			more = !gKpQueue.empty();
		}
		if (more)
			BeginKillProofFetch(false);
		return 0;
	}

	/* Queue KP for this log. Caller holds gMu. Starts worker after releasing preferred. */
	bool EnsureKillProofForLog(LogEntry& e, bool force)
	{
		std::lock_guard<std::mutex> lockKp(gKpCacheMu);
		ApplyKillProofCacheToPlayersLocked(e.players, e.encounter);
		bool needFetch = force;
		if (!needFetch)
		{
			for (const PlayerInfo& p : e.players)
			{
				if (!p.account.empty() && p.kpState == 0)
				{
					needFetch = true;
					break;
				}
			}
		}
		if (!needFetch)
			return false;
		for (PlayerInfo& p : e.players)
		{
			if (p.account.empty())
				continue;
			if (force || p.kpState == 0)
				p.kpState = 1;
		}
		QueueKillProofAccountsLocked(e.players, force);
		return true;
	}

} // namespace LogManagerDetail
