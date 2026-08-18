#include "LogManagerShared.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace LogManagerDetail
{
	void BuildPlayerAggs(const std::vector<const LogEntry*>& filtered, std::vector<PlayerAgg>& out)
	{
		std::unordered_map<std::string, PlayerAgg> map;
		for (const LogEntry* e : filtered)
		{
			std::unordered_map<std::string, bool> seen;
			const time_t t = e->encounterTime > 0 ? e->encounterTime : FileTimeToUnix(e->mtime);
			for (const PlayerInfo& p : e->players)
			{
				const std::string key = !p.account.empty() ? p.account : p.name;
				if (key.empty() || seen[key])
					continue;
				seen[key] = true;
				PlayerAgg& a = map[key];
				a.account = key;
				if (a.displayName.empty())
					a.displayName = p.name.empty() ? key : p.name;
				if (!p.profession.empty())
					a.profession = p.profession;
				a.logs += 1;
				if (e->result == 1)
					a.success += 1;
				if (p.dps > 0)
				{
					a.dpsSum += p.dps;
					a.dpsN += 1;
				}
				if (t > a.lastTime)
					a.lastTime = t;
			}
		}
		out.clear();
		out.reserve(map.size());
		for (auto& kv : map)
			out.push_back(std::move(kv.second));
		std::sort(out.begin(), out.end(), [](const PlayerAgg& a, const PlayerAgg& b) {
			if (a.logs != b.logs)
				return a.logs > b.logs;
			return a.account < b.account;
		});
	}

	void BuildGuildAggs(const std::vector<const LogEntry*>& filtered, std::vector<GuildAgg>& out)
	{
		struct Acc
		{
			std::string label;
			int logs = 0;
			int success = 0;
			std::unordered_map<std::string, bool> players;
		};
		std::unordered_map<std::string, Acc> map;
		for (const LogEntry* e : filtered)
		{
			std::unordered_map<std::string, bool> seenGuild;
			for (const PlayerInfo& p : e->players)
			{
				std::string key = p.guildTag;
				std::string label = p.guildTag;
				if (key.empty() && !p.guildId.empty())
				{
					key = p.guildId;
					label = p.guildId.size() > 8 ? p.guildId.substr(0, 8) + "..." : p.guildId;
				}
				if (key.empty())
					continue;
				if (!seenGuild[key])
				{
					seenGuild[key] = true;
					map[key].logs += 1;
					map[key].label = label;
					if (e->result == 1)
						map[key].success += 1;
				}
				const std::string pk = !p.account.empty() ? p.account : p.name;
				if (!pk.empty())
					map[key].players[pk] = true;
			}
		}
		out.clear();
		for (auto& kv : map)
		{
			GuildAgg g;
			g.key = kv.first;
			g.label = kv.second.label;
			g.logs = kv.second.logs;
			g.success = kv.second.success;
			g.players = static_cast<int>(kv.second.players.size());
			out.push_back(std::move(g));
		}
		std::sort(out.begin(), out.end(), [](const GuildAgg& a, const GuildAgg& b) {
			if (a.logs != b.logs)
				return a.logs > b.logs;
			return a.label < b.label;
		});
	}

	void BuildFastest(const std::vector<const LogEntry*>& filtered, std::vector<FastestKill>& out)
	{
		std::unordered_map<std::string, FastestKill> best;
		for (const LogEntry* e : filtered)
		{
			if (e->result != 1 || e->durationMs <= 0 || e->encounter.empty())
				continue;
			auto it = best.find(e->encounter);
			if (it == best.end() || e->durationMs < it->second.durationMs)
			{
				FastestKill fk;
				fk.encounter = e->encounter;
				fk.durationMs = e->durationMs;
				fk.fileName = e->fileName;
				fk.pathUtf8 = e->pathUtf8;
				fk.mode = e->mode;
				fk.compDps = e->compDps;
				fk.encounterTime = e->encounterTime;
				best[e->encounter] = std::move(fk);
			}
		}
		out.clear();
		out.reserve(best.size());
		for (auto& kv : best)
			out.push_back(std::move(kv.second));
		std::sort(out.begin(), out.end(), [](const FastestKill& a, const FastestKill& b) {
			return a.encounter < b.encounter;
		});
	}

	void BuildEncStats(const std::vector<const LogEntry*>& filtered, std::vector<EncStat>& out)
	{
		std::unordered_map<std::string, EncStat> map;
		for (const LogEntry* e : filtered)
		{
			if (!e)
				continue;
			std::string key = e->encounter;
			if (key.empty())
				key = "Unknown";
			EncStat& s = map[key];
			if (s.encounter.empty())
				s.encounter = key;
			s.attempts += 1;
			if (e->result == 1)
			{
				s.kills += 1;
				if (e->durationMs > 0)
				{
					s.killDurSum += e->durationMs;
					s.killDurN += 1;
					if (s.bestKillMs <= 0 || e->durationMs < s.bestKillMs)
					{
						s.bestKillMs = e->durationMs;
						s.bestPath = e->pathUtf8;
					}
				}
			}
			else if (e->result == 0)
				s.fails += 1;
			if (e->compDps > s.bestSquadDps)
				s.bestSquadDps = e->compDps;
			const time_t t = e->encounterTime > 0 ? e->encounterTime : FileTimeToUnix(e->mtime);
			if (t >= s.lastTime)
			{
				s.lastTime = t;
				s.latestPath = e->pathUtf8;
			}
		}
		out.clear();
		out.reserve(map.size());
		for (auto& kv : map)
			out.push_back(std::move(kv.second));
		std::sort(out.begin(), out.end(), [](const EncStat& a, const EncStat& b) {
			if (a.attempts != b.attempts)
				return a.attempts > b.attempts;
			return a.encounter < b.encounter;
		});
	}

	const EncStat* FindEncStat(const std::vector<EncStat>& stats, const std::string& encounter)
	{
		if (encounter.empty())
			return nullptr;
		for (const EncStat& s : stats)
		{
			if (s.encounter == encounter)
				return &s;
		}
		return nullptr;
	}

} // namespace LogManagerDetail
