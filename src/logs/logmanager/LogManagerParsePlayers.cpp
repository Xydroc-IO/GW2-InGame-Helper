#include "LogManagerParse.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace LogManagerParse
{
constexpr int kMaxPlayersPerLog = 64;

void ParsePlayersFromJson(const char* json, std::vector<PlayerInfo>& out)
{
	out.clear();
	const char* arr = FindPlayersArray(json);
	if (!arr)
		return;
	const char* p = arr + 1;
	while (*p && out.size() < static_cast<size_t>(kMaxPlayersPerLog))
	{
		p = SkipWs(p);
		if (*p == ']')
			break;
		if (*p != '{')
		{
			++p;
			continue;
		}
		const char* end = ObjectEnd(p);
		if (!end)
			break;
		std::string obj(p, end);
		PlayerInfo pi;
		JsonStringAfterKey(obj.c_str(), "name", pi.name);
		if (pi.name.empty())
			JsonStringAfterKey(obj.c_str(), "Name", pi.name);
		JsonStringAfterKey(obj.c_str(), "account", pi.account);
		if (pi.account.empty())
			JsonStringAfterKey(obj.c_str(), "Account", pi.account);
		/* Trim whitespace - EI sometimes pads account names. */
		while (!pi.account.empty() && (pi.account.front() == ' ' || pi.account.front() == '\t'))
			pi.account.erase(pi.account.begin());
		while (!pi.account.empty() && (pi.account.back() == ' ' || pi.account.back() == '\t'))
			pi.account.pop_back();
		JsonStringAfterKey(obj.c_str(), "profession", pi.profession);
		if (pi.profession.empty())
			JsonStringAfterKey(obj.c_str(), "Profession", pi.profession);
		JsonStringAfterKey(obj.c_str(), "guildID", pi.guildId);
		if (pi.guildId.empty())
			JsonStringAfterKey(obj.c_str(), "GuildID", pi.guildId);
		if (pi.guildId.empty())
			JsonStringAfterKey(obj.c_str(), "guildId", pi.guildId);
		/* EI uses nil UUID when the player has no guild - treat as empty. */
		if (pi.guildId == "00000000-0000-0000-0000-000000000000")
			pi.guildId.clear();
		if (pi.guildTag.empty())
			JsonStringAfterKey(obj.c_str(), "guildTag", pi.guildTag);
		long long grp = 0;
		if (JsonLongAfterKey(obj.c_str(), "group", grp) || JsonLongAfterKey(obj.c_str(), "Group", grp))
			pi.group = static_cast<int>(grp);
		if (pi.guildTag.empty())
			pi.guildTag = ExtractGuildTag(pi.name);
		if (pi.guildTag.empty())
			pi.guildTag = ExtractGuildTag(pi.account);
		/* Flat cached fields first, then EI nested dpsAll / buffUptimes. */
		long long flat = 0;
		if (JsonLongAfterKey(obj.c_str(), "dps", flat))
			pi.dps = static_cast<int>(flat);
		if (JsonLongAfterKey(obj.c_str(), "powerDps", flat))
			pi.powerDps = static_cast<int>(flat);
		if (JsonLongAfterKey(obj.c_str(), "condiDps", flat))
			pi.condiDps = static_cast<int>(flat);
		auto readF = [&](const char* key, float& dst) {
			double d = 0.0;
			if (JsonDoubleAfterKey(obj.c_str(), key, d))
				dst = static_cast<float>(d);
		};
		readF("might", pi.might);
		readF("fury", pi.fury);
		readF("quickness", pi.quickness);
		readF("alacrity", pi.alacrity);
		readF("protection", pi.protection);
		readF("regeneration", pi.regeneration);
		readF("swiftness", pi.swiftness);
		readF("vigor", pi.vigor);
		FillPlayerCombatStats(obj.c_str(), pi);
		if (pi.downCount < 0 && JsonLongAfterKey(obj.c_str(), "downCount", flat))
			pi.downCount = static_cast<int>(flat);
		if (pi.deadCount < 0 && JsonLongAfterKey(obj.c_str(), "deadCount", flat))
			pi.deadCount = static_cast<int>(flat);
		if (!pi.name.empty() || !pi.account.empty())
			out.push_back(std::move(pi));
		p = end;
		p = SkipWs(p);
		if (*p == ',')
			++p;
	}
	std::sort(out.begin(), out.end(), [](const PlayerInfo& a, const PlayerInfo& b) {
		if (a.dps != b.dps)
			return a.dps > b.dps;
		return a.name < b.name;
	});
}

int AmountNearId(const char* json, int itemId)
{
	if (!json || itemId <= 0)
		return -1;
	char pat[48];
	std::snprintf(pat, sizeof(pat), "\"id\":%d", itemId);
	const char* hit = std::strstr(json, pat);
	if (!hit)
	{
		std::snprintf(pat, sizeof(pat), "\"id\": %d", itemId);
		hit = std::strstr(json, pat);
	}
	if (!hit)
		return -1;
	const char* obj = hit;
	while (obj > json && *obj != '{')
		--obj;
	if (*obj != '{')
		return -1;
	const char* end = ObjectEnd(obj);
	if (!end)
		return -1;
	std::string slice(obj, end);
	long long amt = 0;
	if (!JsonLongAfterKey(slice.c_str(), "amount", amt))
		return -1;
	if (amt < 0)
		amt = 0;
	if (amt > 2000000000ll)
		amt = 2000000000ll;
	return static_cast<int>(amt);
}

void ParseDpsReportPlayers(const char* json, std::vector<PlayerInfo>& out)
{
	out.clear();
	if (!json)
		return;
	const char* p = std::strstr(json, "\"players\"");
	if (!p)
		return;
	p = std::strchr(p, ':');
	if (!p)
		return;
	++p;
	p = SkipWs(p);
	/* Array form - dps.report objects (not EI player shape). */
	if (*p == '[')
	{
		++p;
		while (*p && out.size() < static_cast<size_t>(kMaxPlayersPerLog))
		{
			p = SkipWs(p);
			if (*p == ']')
				break;
			if (*p != '{')
			{
				++p;
				continue;
			}
			const char* end = ObjectEnd(p);
			if (!end)
				break;
			std::string obj(p, end);
			PlayerInfo pi;
			FillPlayerFromDpsReportObj(obj.c_str(), {}, pi);
			if (!pi.name.empty() || !pi.account.empty())
				out.push_back(std::move(pi));
			p = end;
			p = SkipWs(p);
			if (*p == ',')
				++p;
		}
		return;
	}
	/* Object form: "Char Name": { "display_name": "...", ... } */
	if (*p != '{')
		return;
	++p;
	while (*p && out.size() < static_cast<size_t>(kMaxPlayersPerLog))
	{
		p = SkipWs(p);
		if (*p == '}')
			break;
		if (*p != '"')
		{
			++p;
			continue;
		}
		++p;
		std::string charName;
		while (*p && *p != '"')
		{
			if (*p == '\\' && p[1])
			{
				++p;
				charName.push_back(*p);
			}
			else
				charName.push_back(*p);
			++p;
		}
		if (*p == '"')
			++p;
		p = SkipWs(p);
		if (*p != ':')
			continue;
		++p;
		p = SkipWs(p);
		if (*p != '{')
			continue;
		const char* end = ObjectEnd(p);
		if (!end)
			break;
		std::string obj(p, end);
		PlayerInfo pi;
		FillPlayerFromDpsReportObj(obj.c_str(), charName, pi);
		if (!pi.name.empty() || !pi.account.empty())
			out.push_back(std::move(pi));
		p = end;
		p = SkipWs(p);
		if (*p == ',')
			++p;
	}
}

} // namespace LogManagerParse
