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

constexpr long long kBuffMight = 740;
constexpr long long kBuffFury = 725;
constexpr long long kBuffQuickness = 1187;
constexpr long long kBuffAlacrity = 30328;
constexpr long long kBuffProtection = 717;
constexpr long long kBuffRegen = 718;
constexpr long long kBuffSwiftness = 719;
constexpr long long kBuffVigor = 726;

std::string ExtractGuildTag(const std::string& name)
{
	const auto open = name.rfind('[');
	const auto close = name.rfind(']');
	if (open == std::string::npos || close == std::string::npos || close <= open + 1)
		return {};
	if (close != name.size() - 1 && name.find(']', open) != close)
		return {};
	std::string tag = name.substr(open + 1, close - open - 1);
	if (tag.empty() || tag.size() > 8)
		return {};
	for (char c : tag)
	{
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
			return {};
	}
	return tag;
}

/* ---------- lightweight JSON extractors ---------- */

const char* SkipWs(const char* p)
{
	while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
		++p;
	return p;
}

bool JsonStringAfterKey(const char* json, const char* key, std::string& out)
{
	out.clear();
	if (!json || !key)
		return false;
	char pat[96];
	std::snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = std::strstr(json, pat);
	if (!p)
		return false;
	p += std::strlen(pat);
	p = SkipWs(p);
	if (*p != ':')
		return false;
	++p;
	p = SkipWs(p);
	if (*p != '"')
		return false;
	++p;
	std::string s;
	while (*p && *p != '"')
	{
		if (*p == '\\' && p[1])
		{
			++p;
			switch (*p)
			{
			case 'n': s += '\n'; break;
			case 'r': s += '\r'; break;
			case 't': s += '\t'; break;
			case '"': s += '"'; break;
			case '\\': s += '\\'; break;
			default: s += *p; break;
			}
		}
		else
			s += *p;
		++p;
	}
	out = std::move(s);
	return true;
}

bool JsonBoolAfterKey(const char* json, const char* key, bool& out)
{
	if (!json || !key)
		return false;
	char pat[96];
	std::snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = std::strstr(json, pat);
	if (!p)
		return false;
	p += std::strlen(pat);
	p = SkipWs(p);
	if (*p != ':')
		return false;
	++p;
	p = SkipWs(p);
	if (std::strncmp(p, "true", 4) == 0)
	{
		out = true;
		return true;
	}
	if (std::strncmp(p, "false", 5) == 0)
	{
		out = false;
		return true;
	}
	return false;
}

bool JsonLongAfterKey(const char* json, const char* key, long long& out)
{
	if (!json || !key)
		return false;
	char pat[96];
	std::snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = std::strstr(json, pat);
	if (!p)
		return false;
	p += std::strlen(pat);
	p = SkipWs(p);
	if (*p != ':')
		return false;
	++p;
	p = SkipWs(p);
	char* end = nullptr;
	const long long v = std::strtoll(p, &end, 10);
	if (end == p)
		return false;
	out = v;
	return true;
}

const char* ObjectEnd(const char* start); /* defined below */

bool JsonDoubleAfterKey(const char* json, const char* key, double& out)
{
	if (!json || !key)
		return false;
	char pat[96];
	std::snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = std::strstr(json, pat);
	if (!p)
		return false;
	p += std::strlen(pat);
	p = SkipWs(p);
	if (*p != ':')
		return false;
	++p;
	p = SkipWs(p);
	char* end = nullptr;
	const double v = std::strtod(p, &end);
	if (end == p)
		return false;
	out = v;
	return true;
}

bool ExtractFirstObjectInArrayAfterKey(const char* json, const char* key, std::string& objOut)
{
	objOut.clear();
	if (!json || !key)
		return false;
	char pat[96];
	std::snprintf(pat, sizeof(pat), "\"%s\"", key);
	const char* p = std::strstr(json, pat);
	if (!p)
		return false;
	p = std::strchr(p, '[');
	if (!p)
		return false;
	++p;
	p = SkipWs(p);
	if (*p != '{')
		return false;
	const char* end = ObjectEnd(p);
	if (!end)
		return false;
	objOut.assign(p, end);
	return true;
}

float BuffUptimePercent(const char* playerObj, long long buffId)
{
	if (!playerObj)
		return -1.f;
	const char* bu = std::strstr(playerObj, "\"buffUptimes\"");
	if (!bu)
		bu = std::strstr(playerObj, "\"BuffUptimes\"");
	if (!bu)
		return -1.f;
	const char* arr = std::strchr(bu, '[');
	if (!arr)
		return -1.f;
	char idPat[48];
	std::snprintf(idPat, sizeof(idPat), "\"id\":%lld", buffId);
	char idPatSp[48];
	std::snprintf(idPatSp, sizeof(idPatSp), "\"id\": %lld", buffId);
	const char* hit = std::strstr(arr, idPat);
	if (!hit)
		hit = std::strstr(arr, idPatSp);
	if (!hit)
	{
		std::snprintf(idPat, sizeof(idPat), "\"Id\":%lld", buffId);
		std::snprintf(idPatSp, sizeof(idPatSp), "\"Id\": %lld", buffId);
		hit = std::strstr(arr, idPat);
		if (!hit)
			hit = std::strstr(arr, idPatSp);
	}
	if (!hit)
		return -1.f;
	/* Walk back to owning object '{'. */
	const char* obj = hit;
	while (obj > arr && *obj != '{')
		--obj;
	if (*obj != '{')
		return -1.f;
	const char* objEnd = ObjectEnd(obj);
	if (!objEnd)
		return -1.f;
	std::string slice(obj, objEnd);
	std::string data0;
	if (!ExtractFirstObjectInArrayAfterKey(slice.c_str(), "buffData", data0) &&
		!ExtractFirstObjectInArrayAfterKey(slice.c_str(), "BuffData", data0))
		return -1.f;
	double up = 0.0;
	if (!JsonDoubleAfterKey(data0.c_str(), "uptime", up) &&
		!JsonDoubleAfterKey(data0.c_str(), "Uptime", up))
		return -1.f;
	return static_cast<float>(up);
}

void FillPlayerCombatStats(const char* playerObj, PlayerInfo& pi)
{
	if (!playerObj)
		return;
	std::string dps0;
	if (ExtractFirstObjectInArrayAfterKey(playerObj, "dpsAll", dps0) ||
		ExtractFirstObjectInArrayAfterKey(playerObj, "DpsAll", dps0))
	{
		long long v = 0;
		if (JsonLongAfterKey(dps0.c_str(), "dps", v) || JsonLongAfterKey(dps0.c_str(), "Dps", v))
			pi.dps = static_cast<int>(v);
		if (JsonLongAfterKey(dps0.c_str(), "powerDps", v) || JsonLongAfterKey(dps0.c_str(), "PowerDps", v))
			pi.powerDps = static_cast<int>(v);
		if (JsonLongAfterKey(dps0.c_str(), "condiDps", v) || JsonLongAfterKey(dps0.c_str(), "CondiDps", v))
			pi.condiDps = static_cast<int>(v);
	}
	/* Only overwrite when nested buffUptimes exist - flat cache values must survive. */
	auto setBuff = [](float& dst, float v) {
		if (v >= 0.f)
			dst = v;
	};
	setBuff(pi.might, BuffUptimePercent(playerObj, kBuffMight));
	setBuff(pi.fury, BuffUptimePercent(playerObj, kBuffFury));
	setBuff(pi.quickness, BuffUptimePercent(playerObj, kBuffQuickness));
	setBuff(pi.alacrity, BuffUptimePercent(playerObj, kBuffAlacrity));
	setBuff(pi.protection, BuffUptimePercent(playerObj, kBuffProtection));
	setBuff(pi.regeneration, BuffUptimePercent(playerObj, kBuffRegen));
	setBuff(pi.swiftness, BuffUptimePercent(playerObj, kBuffSwiftness));
	setBuff(pi.vigor, BuffUptimePercent(playerObj, kBuffVigor));

	std::string def0;
	if (ExtractFirstObjectInArrayAfterKey(playerObj, "defenses", def0) ||
		ExtractFirstObjectInArrayAfterKey(playerObj, "Defenses", def0))
	{
		long long v = 0;
		if (JsonLongAfterKey(def0.c_str(), "downCount", v) || JsonLongAfterKey(def0.c_str(), "DownCount", v))
			pi.downCount = static_cast<int>(v);
		if (JsonLongAfterKey(def0.c_str(), "deadCount", v) || JsonLongAfterKey(def0.c_str(), "DeadCount", v))
			pi.deadCount = static_cast<int>(v);
	}
}

bool PlayersHaveDps(const std::vector<PlayerInfo>& players)
{
	for (const auto& p : players)
	{
		if (p.dps > 0)
			return true;
	}
	return false;
}

bool PlayersHaveBoons(const std::vector<PlayerInfo>& players)
{
	for (const auto& p : players)
	{
		if (p.quickness >= 0.f || p.alacrity >= 0.f || p.might >= 0.f ||
			p.fury >= 0.f || p.protection >= 0.f || p.regeneration >= 0.f ||
			p.swiftness >= 0.f || p.vigor >= 0.f)
			return true;
	}
	return false;
}

bool PlayersNeedCombatStats(const std::vector<PlayerInfo>& players)
{
	return !PlayersHaveDps(players) || !PlayersHaveBoons(players);
}

const char* ProfessionNameFromId(long long id)
{
	switch (id)
	{
	case 1: return "Guardian";
	case 2: return "Warrior";
	case 3: return "Engineer";
	case 4: return "Ranger";
	case 5: return "Thief";
	case 6: return "Elementalist";
	case 7: return "Mesmer";
	case 8: return "Necromancer";
	case 9: return "Revenant";
	default: return nullptr;
	}
}

const char* EliteSpecNameFromId(long long id)
{
	switch (id)
	{
	case 5: return "Druid";
	case 7: return "Daredevil";
	case 18: return "Berserker";
	case 27: return "Dragonhunter";
	case 34: return "Reaper";
	case 40: return "Chronomancer";
	case 43: return "Scrapper";
	case 48: return "Tempest";
	case 52: return "Herald";
	case 55: return "Soulbeast";
	case 56: return "Weaver";
	case 57: return "Holosmith";
	case 58: return "Deadeye";
	case 59: return "Mirage";
	case 60: return "Scourge";
	case 61: return "Spellbreaker";
	case 62: return "Firebrand";
	case 63: return "Renegade";
	case 64: return "Harbinger";
	case 65: return "Willbender";
	case 66: return "Virtuoso";
	case 67: return "Catalyst";
	case 68: return "Bladesworn";
	case 69: return "Vindicator";
	case 70: return "Mechanist";
	case 71: return "Specter";
	case 72: return "Untamed";
	case 73: return "Troubadour";
	case 74: return "Paragon";
	case 75: return "Amalgam";
	case 76: return "Ritualist";
	case 77: return "Antiquary";
	case 78: return "Galeshot";
	case 79: return "Conduit";
	case 80: return "Evoker";
	case 81: return "Luminary";
	default: return nullptr;
	}
}

std::string FormatProfessionElite(long long prof, long long elite)
{
	const char* eliteName = elite > 0 ? EliteSpecNameFromId(elite) : nullptr;
	if (eliteName)
		return eliteName;
	const char* profName = ProfessionNameFromId(prof);
	if (profName)
		return profName;
	if (prof <= 0 && elite <= 0)
		return {};
	char buf[48];
	if (elite > 0)
		std::snprintf(buf, sizeof(buf), "%lld / elite %lld", prof, elite);
	else
		std::snprintf(buf, sizeof(buf), "%lld", prof);
	return buf;
}

void FillPlayerFromDpsReportObj(const char* obj, const std::string& fallbackName, PlayerInfo& pi)
{
	pi = PlayerInfo{};
	pi.name = fallbackName;
	if (obj)
	{
		JsonStringAfterKey(obj, "character_name", pi.name);
		JsonStringAfterKey(obj, "display_name", pi.account);
		long long prof = 0, elite = 0;
		JsonLongAfterKey(obj, "profession", prof);
		JsonLongAfterKey(obj, "elite_spec", elite);
		pi.profession = FormatProfessionElite(prof, elite);
	}
	pi.guildTag = ExtractGuildTag(pi.name);
	if (pi.guildTag.empty())
		pi.guildTag = ExtractGuildTag(pi.account);
}

const char* FindPlayersArray(const char* json)
{
	if (!json)
		return nullptr;
	const char* p = std::strstr(json, "\"players\"");
	if (!p)
		p = std::strstr(json, "\"Players\"");
	if (!p)
		return nullptr;
	p = std::strchr(p, '[');
	return p;
}

/* Extract object slice starting at '{' - returns end past matching '}'. */
const char* ObjectEnd(const char* start)
{
	if (!start || *start != '{')
		return nullptr;
	int depth = 0;
	bool inStr = false;
	bool esc = false;
	for (const char* p = start; *p; ++p)
	{
		if (inStr)
		{
			if (esc)
				esc = false;
			else if (*p == '\\')
				esc = true;
			else if (*p == '"')
				inStr = false;
			continue;
		}
		if (*p == '"')
		{
			inStr = true;
			continue;
		}
		if (*p == '{')
			++depth;
		else if (*p == '}')
		{
			--depth;
			if (depth == 0)
				return p + 1;
		}
	}
	return nullptr;
}

} // namespace LogManagerParse
