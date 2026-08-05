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
				if (a.profession.empty())
					a.profession = p.profession;
				a.logs += 1;
				if (e->result == 1)
					a.success += 1;
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
					label = p.guildId.size() > 8 ? p.guildId.substr(0, 8) + "…" : p.guildId;
				}
				if (key.empty())
					continue;
				if (!seenGuild[key])
				{
					seenGuild[key] = true;
					map[key].logs += 1;
					map[key].label = label;
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

} // namespace LogManagerDetail
