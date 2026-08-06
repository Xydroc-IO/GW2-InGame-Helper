#include "ConfirmedWaypoints.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "MumbleIdentity.h"
#include "WaypointsData.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kVersion = 1;
	/* Continent units - close enough to stand on / unlock a WP. */
	constexpr float kConfirmRadius = 75.f;
	constexpr float kConfirmRadius2 = kConfirmRadius * kConfirmRadius;
	constexpr DWORD kTickMs = 250u;
	constexpr DWORD kSaveDebounceMs = 2000u;

	std::mutex gMu;
	std::unordered_map<std::string, std::unordered_set<int>> gByChar;
	std::string gActive;
	bool gDirty = false;
	bool gPreferConfirmed = true;
	DWORD gLastTick = 0;
	DWORD gLastSave = 0;

	std::wstring PathW()
	{
		return AddonPaths::ConfigDir() + L"\\confirmed_waypoints.json";
	}

	bool WriteUtf8File(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		CloseHandle(h);
		return ok && written == data.size();
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 4 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok || read != out.size())
			return {};
		return out;
	}

	std::string EscapeJson(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() + 8);
		for (char c : s)
		{
			if (c == '\\' || c == '"')
			{
				o.push_back('\\');
				o.push_back(c);
			}
			else if (static_cast<unsigned char>(c) < 0x20)
				continue;
			else
				o.push_back(c);
		}
		return o;
	}

	std::string SerializeLocked()
	{
		std::string out = "{\n  \"version\": ";
		out += std::to_string(kVersion);
		out += ",\n  \"prefer_confirmed\": ";
		out += gPreferConfirmed ? "true" : "false";
		out += ",\n  \"characters\": {\n";
		bool firstChar = true;
		for (const auto& kv : gByChar)
		{
			if (kv.second.empty())
				continue;
			if (!firstChar)
				out += ",\n";
			firstChar = false;
			out += "    \"";
			out += EscapeJson(kv.first);
			out += "\": [";
			bool firstId = true;
			for (int id : kv.second)
			{
				if (!firstId)
					out += ", ";
				firstId = false;
				out += std::to_string(id);
			}
			out += "]";
		}
		out += "\n  }\n}\n";
		return out;
	}

	void Parse(const std::string& raw)
	{
		gByChar.clear();
		gPreferConfirmed = true;
		if (raw.find("\"prefer_confirmed\"") != std::string::npos)
		{
			const size_t k = raw.find("\"prefer_confirmed\"");
			const size_t c = raw.find(':', k);
			if (c != std::string::npos)
			{
				size_t p = c + 1;
				while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t'))
					++p;
				gPreferConfirmed = raw.compare(p, 4, "true") == 0;
			}
		}
		size_t charsKey = raw.find("\"characters\"");
		if (charsKey == std::string::npos)
			return;
		size_t obj = raw.find('{', charsKey);
		if (obj == std::string::npos)
			return;
		size_t i = obj + 1;
		while (i < raw.size())
		{
			while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\n' || raw[i] == '\r' ||
				raw[i] == '\t' || raw[i] == ','))
				++i;
			if (i >= raw.size() || raw[i] == '}')
				break;
			if (raw[i] != '"')
				break;
			++i;
			std::string name;
			while (i < raw.size())
			{
				const char c = raw[i++];
				if (c == '\\' && i < raw.size())
				{
					name.push_back(raw[i++]);
					continue;
				}
				if (c == '"')
					break;
				name.push_back(c);
			}
			size_t colon = raw.find(':', i);
			if (colon == std::string::npos)
				break;
			size_t lb = raw.find('[', colon);
			size_t rb = (lb != std::string::npos) ? raw.find(']', lb) : std::string::npos;
			if (lb == std::string::npos || rb == std::string::npos || name.empty())
				break;
			std::unordered_set<int> ids;
			size_t p = lb + 1;
			while (p < rb)
			{
				while (p < rb && (raw[p] < '0' || raw[p] > '9') && raw[p] != '-')
					++p;
				if (p >= rb)
					break;
				const int id = std::atoi(raw.c_str() + p);
				if (id > 0)
					ids.insert(id);
				while (p < rb && ((raw[p] >= '0' && raw[p] <= '9') || raw[p] == '-'))
					++p;
			}
			if (!ids.empty())
				gByChar[name] = std::move(ids);
			i = rb + 1;
		}
	}
}

void ConfirmedWaypoints::Load()
{
	std::lock_guard<std::mutex> lock(gMu);
	gByChar.clear();
	gActive.clear();
	gDirty = false;
	const std::string raw = ReadUtf8File(PathW());
	if (!raw.empty())
		Parse(raw);
}

void ConfirmedWaypoints::Save(bool force)
{
	std::string payload;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!force && !gDirty)
			return;
		const DWORD now = GetTickCount();
		if (!force && gLastSave != 0 && (now - gLastSave) < kSaveDebounceMs)
			return;
		gLastSave = now;
		payload = SerializeLocked();
		gDirty = false;
	}
	AddonPaths::ConfigDir();
	WriteUtf8File(PathW(), payload);
}

void ConfirmedWaypoints::Tick()
{
	const DWORD now = GetTickCount();
	if (gLastTick != 0 && (now - gLastTick) < kTickMs)
		return;
	gLastTick = now;

	const std::string name = MumbleIdentity::CharacterNameStr();
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (name != gActive)
			gActive = name;
	}
	if (name.empty())
	{
		Save(false);
		return;
	}
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return;
	const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	if (!ctx || ctx->mapId == 0)
		return;

	WaypointsData::EnsureLoaded(false);
	WaypointsData::Tick();
	if (!WaypointsData::Ready())
		return;

	std::vector<WaypointsData::Poi> pois;
	WaypointsData::ListForMap(static_cast<int>(ctx->mapId), true, pois);
	const float px = ctx->playerX;
	const float py = ctx->playerY;
	if (!std::isfinite(px) || !std::isfinite(py))
		return;

	bool added = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto& set = gByChar[name];
		for (const WaypointsData::Poi& p : pois)
		{
			if (!p.hasCoord || p.id <= 0)
				continue;
			if (set.count(p.id))
				continue;
			const float dx = p.continentX - px;
			const float dy = p.continentY - py;
			if (dx * dx + dy * dy <= kConfirmRadius2)
			{
				set.insert(p.id);
				added = true;
			}
		}
		if (added)
			gDirty = true;
	}
	if (added)
		Save(false);
	else
		Save(false);
}

bool ConfirmedWaypoints::IsConfirmed(int waypointId)
{
	if (waypointId <= 0)
		return false;
	std::lock_guard<std::mutex> lock(gMu);
	if (gActive.empty())
		return false;
	auto it = gByChar.find(gActive);
	if (it == gByChar.end())
		return false;
	return it->second.count(waypointId) != 0;
}

size_t ConfirmedWaypoints::CountForActive()
{
	std::lock_guard<std::mutex> lock(gMu);
	if (gActive.empty())
		return 0;
	auto it = gByChar.find(gActive);
	if (it == gByChar.end())
		return 0;
	return it->second.size();
}

size_t ConfirmedWaypoints::CountOnMap(int mapId)
{
	if (mapId <= 0 || !WaypointsData::Ready())
		return 0;
	std::vector<WaypointsData::Poi> pois;
	WaypointsData::ListForMap(mapId, true, pois);
	size_t n = 0;
	std::lock_guard<std::mutex> lock(gMu);
	if (gActive.empty())
		return 0;
	auto it = gByChar.find(gActive);
	if (it == gByChar.end())
		return 0;
	for (const WaypointsData::Poi& p : pois)
	{
		if (it->second.count(p.id))
			++n;
	}
	return n;
}

bool ConfirmedWaypoints::PreferConfirmed()
{
	std::lock_guard<std::mutex> lock(gMu);
	return gPreferConfirmed;
}

void ConfirmedWaypoints::SetPreferConfirmed(bool on)
{
	std::lock_guard<std::mutex> lock(gMu);
	if (gPreferConfirmed == on)
		return;
	gPreferConfirmed = on;
	gDirty = true;
}
