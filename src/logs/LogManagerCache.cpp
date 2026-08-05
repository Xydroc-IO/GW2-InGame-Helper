#include "LogManagerShared.h"

#include "AddonPaths.h"
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
	std::string JsonEscape(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() + 8);
		for (unsigned char c : s)
		{
			switch (c)
			{
			case '"': o += "\\\""; break;
			case '\\': o += "\\\\"; break;
			case '\n': o += "\\n"; break;
			case '\r': o += "\\r"; break;
			case '\t': o += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					o += buf;
				}
				else
					o += static_cast<char>(c);
				break;
			}
		}
		return o;
	}

	std::wstring CachePathW()
	{
		return AddonPaths::ConfigDir() + L"\\log-index.json";
	}

	bool FileExistsW(const std::wstring& path)
	{
		const DWORD a = GetFileAttributesW(path.c_str());
		return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
	}

	bool DirExistsW(const std::wstring& path)
	{
		const DWORD a = GetFileAttributesW(path.c_str());
		return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
	}

	/* Resolve EI guildUUID → official API tag (worker thread only). */
	void ResolveGuildTagsForPlayers(std::vector<PlayerInfo>& players)
	{
		static std::mutex sCacheMu;
		static std::unordered_map<std::string, std::string> sTagById;

		for (PlayerInfo& p : players)
		{
			if (p.guildId.empty() || !p.guildTag.empty())
				continue;
			{
				std::lock_guard<std::mutex> lock(sCacheMu);
				auto it = sTagById.find(p.guildId);
				if (it != sTagById.end())
				{
					p.guildTag = it->second;
					continue;
				}
			}
			std::string path = "/v2/guild/";
			path += p.guildId;
			const Gw2Http::Result r = Gw2Http::Api(path.c_str(), nullptr, 6000);
			if (!r.ok || r.body.empty())
				continue;
			std::string tag;
			if (!JsonStringAfterKey(r.body.c_str(), "tag", tag) || tag.empty())
				continue;
			p.guildTag = tag;
			std::lock_guard<std::mutex> lock(sCacheMu);
			sTagById[p.guildId] = tag;
		}
	}

	void ApplyEiJsonToEntry(LogEntry& e, const std::string& json)
	{
		std::string name;
		if (!JsonStringAfterKey(json.c_str(), "name", name))
			JsonStringAfterKey(json.c_str(), "Name", name);
		if (name.empty() && !JsonStringAfterKey(json.c_str(), "fightName", name))
			JsonStringAfterKey(json.c_str(), "FightName", name);
		if (!name.empty())
			e.encounter = name;

		bool success = false;
		if (JsonBoolAfterKey(json.c_str(), "success", success) || JsonBoolAfterKey(json.c_str(), "Success", success))
			e.result = success ? 1 : 0;

		bool isLcm = false;
		bool isCm = false;
		if (JsonBoolAfterKey(json.c_str(), "isLegendaryCM", isLcm) || JsonBoolAfterKey(json.c_str(), "IsLegendaryCM", isLcm))
		{
			/* ok */
		}
		if (JsonBoolAfterKey(json.c_str(), "isCM", isCm) || JsonBoolAfterKey(json.c_str(), "IsCM", isCm))
		{
			/* ok */
		}
		if (isLcm)
			e.mode = "LCM";
		else if (isCm)
			e.mode = "CM";
		else
			e.mode.clear();

		long long dur = 0;
		if (JsonLongAfterKey(json.c_str(), "durationMS", dur) || JsonLongAfterKey(json.c_str(), "DurationMS", dur))
			e.durationMs = dur;

		std::string tStart;
		if (!JsonStringAfterKey(json.c_str(), "timeStartStd", tStart))
			JsonStringAfterKey(json.c_str(), "TimeStartStd", tStart);
		if (tStart.empty() && !JsonStringAfterKey(json.c_str(), "timeStart", tStart))
			JsonStringAfterKey(json.c_str(), "TimeStart", tStart);
		if (!tStart.empty())
		{
			/* "2024-01-15 12:34:56 +00" or similar — parse YYYY-MM-DD HH:MM:SS */
			struct tm tm{};
			int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
			if (std::sscanf(tStart.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) >= 5)
			{
				tm.tm_year = y - 1900;
				tm.tm_mon = mo - 1;
				tm.tm_mday = d;
				tm.tm_hour = h;
				tm.tm_min = mi;
				tm.tm_sec = s;
				tm.tm_isdst = -1;
				const time_t tt = std::mktime(&tm);
				if (tt > 0)
					e.encounterTime = tt;
			}
		}

		ParsePlayersFromJson(json.c_str(), e.players);
		ResolveGuildTagsForPlayers(e.players);
		{
			std::lock_guard<std::mutex> lockKp(gKpCacheMu);
			ApplyKillProofCacheToPlayersLocked(e.players, e.encounter);
		}
		long long squad = 0;
		for (const auto& p : e.players)
			squad += p.dps;
		if (squad > 0)
			e.compDps = static_cast<int>(squad);
		e.state = ParseState::Parsed;
		e.parseError.clear();
	}

	/* ---------- cache ---------- */

	std::string ReadFileUtf8(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 80 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string body(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD got = 0;
		const BOOL ok = ReadFile(h, body.data(), static_cast<DWORD>(body.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok)
			return {};
		body.resize(got);
		return body;
	}

	bool WriteFileUtf8(const std::wstring& path, const std::string& body)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD wrote = 0;
		const BOOL ok = WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &wrote, nullptr);
		CloseHandle(h);
		return ok != 0;
	}

	void SaveCacheLocked()
	{
		/* caller holds gMu */
		std::string out;
		out.reserve(gLogs.size() * 256 + 32);
		out += "[\n";
		for (size_t i = 0; i < gLogs.size(); ++i)
		{
			const LogEntry& e = gLogs[i];
			if (i)
				out += ",\n";
			out += "  {\"path\":\"";
			out += JsonEscape(e.pathUtf8);
			out += "\",\"size\":";
			char num[64];
			std::snprintf(num, sizeof(num), "%llu", static_cast<unsigned long long>(e.fileSize.QuadPart));
			out += num;
			out += ",\"mtime\":";
			std::snprintf(num, sizeof(num), "%llu",
				(static_cast<unsigned long long>(e.mtime.dwHighDateTime) << 32) |
					e.mtime.dwLowDateTime);
			out += num;
			out += ",\"state\":";
			std::snprintf(num, sizeof(num), "%d", static_cast<int>(e.state));
			out += num;
			out += ",\"encounter\":\"";
			out += JsonEscape(e.encounter);
			out += "\",\"mode\":\"";
			out += JsonEscape(e.mode);
			out += "\",\"result\":";
			std::snprintf(num, sizeof(num), "%d", e.result);
			out += num;
			out += ",\"durationMs\":";
			std::snprintf(num, sizeof(num), "%lld", static_cast<long long>(e.durationMs));
			out += num;
			out += ",\"encounterTime\":";
			std::snprintf(num, sizeof(num), "%lld", static_cast<long long>(e.encounterTime));
			out += num;
			out += ",\"compDps\":";
			std::snprintf(num, sizeof(num), "%d", e.compDps);
			out += num;
			out += ",\"dpsReportUrl\":\"";
			out += JsonEscape(e.dpsReportUrl);
			out += "\",\"jsonPath\":\"";
			out += JsonEscape(e.jsonPathUtf8);
			out += "\",\"players\":[";
			for (size_t pi = 0; pi < e.players.size(); ++pi)
			{
				const PlayerInfo& p = e.players[pi];
				if (pi)
					out += ',';
				out += "{\"name\":\"";
				out += JsonEscape(p.name);
				out += "\",\"account\":\"";
				out += JsonEscape(p.account);
				out += "\",\"profession\":\"";
				out += JsonEscape(p.profession);
				out += "\",\"guildTag\":\"";
				out += JsonEscape(p.guildTag);
				out += "\",\"guildId\":\"";
				out += JsonEscape(p.guildId);
				out += "\",\"group\":";
				std::snprintf(num, sizeof(num), "%d", p.group);
				out += num;
				out += ",\"dps\":";
				std::snprintf(num, sizeof(num), "%d", p.dps);
				out += num;
				out += ",\"powerDps\":";
				std::snprintf(num, sizeof(num), "%d", p.powerDps);
				out += num;
				out += ",\"condiDps\":";
				std::snprintf(num, sizeof(num), "%d", p.condiDps);
				out += num;
				auto writeF = [&](const char* key, float v) {
					out += ",\"";
					out += key;
					out += "\":";
					if (v < 0.f)
						out += "-1";
					else
					{
						std::snprintf(num, sizeof(num), "%.1f", static_cast<double>(v));
						out += num;
					}
				};
				writeF("might", p.might);
				writeF("fury", p.fury);
				writeF("quickness", p.quickness);
				writeF("alacrity", p.alacrity);
				writeF("protection", p.protection);
				writeF("regeneration", p.regeneration);
				writeF("swiftness", p.swiftness);
				writeF("vigor", p.vigor);
				out += '}';
			}
			out += "]}";
		}
		out += "\n]\n";
		WriteFileUtf8(CachePathW(), out);
	}

	void LoadCacheInto(std::unordered_map<std::string, LogEntry>& byPath)
	{
		byPath.clear();
		const std::string body = ReadFileUtf8(CachePathW());
		if (body.empty())
			return;
		/* Walk top-level objects — simple scan for "path" keys in objects. */
		const char* p = body.c_str();
		while ((p = std::strstr(p, "{\"path\":\"")) != nullptr)
		{
			const char* end = ObjectEnd(p);
			if (!end)
				break;
			std::string obj(p, end);
			LogEntry e;
			JsonStringAfterKey(obj.c_str(), "path", e.pathUtf8);
			if (e.pathUtf8.empty())
			{
				p = end;
				continue;
			}
			e.pathW = Utf8ToWide(e.pathUtf8.c_str());
			const auto slash = e.pathUtf8.find_last_of("\\/");
			e.fileName = slash == std::string::npos ? e.pathUtf8 : e.pathUtf8.substr(slash + 1);
			long long size = 0, mtime = 0, state = 0, result = -1, dur = 0, et = 0, comp = 0;
			JsonLongAfterKey(obj.c_str(), "size", size);
			JsonLongAfterKey(obj.c_str(), "mtime", mtime);
			JsonLongAfterKey(obj.c_str(), "state", state);
			JsonLongAfterKey(obj.c_str(), "result", result);
			JsonLongAfterKey(obj.c_str(), "durationMs", dur);
			JsonLongAfterKey(obj.c_str(), "encounterTime", et);
			JsonLongAfterKey(obj.c_str(), "compDps", comp);
			e.fileSize.QuadPart = static_cast<ULONGLONG>(size);
			e.mtime.dwLowDateTime = static_cast<DWORD>(mtime & 0xffffffffu);
			e.mtime.dwHighDateTime = static_cast<DWORD>((mtime >> 32) & 0xffffffffu);
			e.state = static_cast<ParseState>(static_cast<int>(state));
			if (e.state == ParseState::Uploading)
				e.state = ParseState::Parsed;
			e.result = static_cast<int>(result);
			e.durationMs = dur;
			e.encounterTime = static_cast<time_t>(et);
			e.compDps = static_cast<int>(comp);
			JsonStringAfterKey(obj.c_str(), "encounter", e.encounter);
			JsonStringAfterKey(obj.c_str(), "mode", e.mode);
			JsonStringAfterKey(obj.c_str(), "dpsReportUrl", e.dpsReportUrl);
			JsonStringAfterKey(obj.c_str(), "jsonPath", e.jsonPathUtf8);
			ParsePlayersFromJson(obj.c_str(), e.players);
			byPath[e.pathUtf8] = std::move(e);
			p = end;
		}
	}

} // namespace LogManagerDetail
