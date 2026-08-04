#include "WaypointsData.h"
#include "WaypointsDataInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace WaypointsDataDetail
{
	std::mutex gMu;
	std::vector<WaypointsData::Poi> gPois;
	std::vector<WaypointsData::MapRow> gMaps;
	std::string gStatus = "Open Waypoints to load map data.";
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gReady{false};
	std::atomic<bool> gPendingReady{false};
	std::atomic<bool> gSkipCache{false};
	std::vector<WaypointsData::Poi> gPendingPois;
	std::vector<WaypointsData::MapRow> gPendingMaps;
	std::string gPendingStatus;
	HANDLE gThread = nullptr;
	DWORD gLoadedAt = 0;

std::wstring CachePath()
{
	return AddonPaths::DataDir() + L"\\waypoints-index.cache";
}

std::string ToLowerCopy(std::string s)
{
	for (char& c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

bool WriteUtf8File(const std::wstring& path, const std::string& data)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
	CloseHandle(h);
	return ok && written == data.size();
}

std::string ReadUtf8File(const std::wstring& path)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return {};
	LARGE_INTEGER sz{};
	if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 32 * 1024 * 1024)
	{
		CloseHandle(h);
		return {};
	}
	std::string out(static_cast<size_t>(sz.QuadPart), '\0');
	DWORD read = 0;
	const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
	CloseHandle(h);
	if (!ok || read != out.size()) return {};
	return out;
}

void RebuildMaps(const std::vector<WaypointsData::Poi>& pois,
	std::vector<WaypointsData::MapRow>& maps)
{
	std::unordered_map<int, WaypointsData::MapRow> byId;
	for (const WaypointsData::Poi& p : pois)
	{
		auto& m = byId[p.mapId];
		m.id = p.mapId;
		if (m.name.empty())
			m.name = p.mapName;
		if (p.type == "waypoint")
			++m.waypointCount;
	}
	maps.clear();
	maps.reserve(byId.size());
	for (auto& kv : byId)
		maps.push_back(std::move(kv.second));
	std::sort(maps.begin(), maps.end(),
		[](const WaypointsData::MapRow& a, const WaypointsData::MapRow& b) {
			return ToLowerCopy(a.name) < ToLowerCopy(b.name);
		});
}

std::string EscapeField(const std::string& s)
{
	std::string o;
	o.reserve(s.size());
	for (char c : s)
	{
		if (c == '\t' || c == '\n' || c == '\r')
			o.push_back(' ');
		else
			o.push_back(c);
	}
	return o;
}

bool SaveCache(const std::vector<WaypointsData::Poi>& pois)
{
	std::string out;
	out.reserve(pois.size() * 64);
	out += "#gw2igh-waypoints v";
	out += kCacheVer;
	out += '\n';
	for (const WaypointsData::Poi& p : pois)
	{
		out += std::to_string(p.mapId);
		out += '\t';
		out += EscapeField(p.mapName);
		out += '\t';
		out += std::to_string(p.id);
		out += '\t';
		out += EscapeField(p.type);
		out += '\t';
		out += EscapeField(p.name);
		out += '\t';
		out += EscapeField(p.chatLink);
		out += '\t';
		if (p.hasCoord)
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%.3f\t%.3f", p.continentX, p.continentY);
			out += buf;
		}
		else
			out += "\t";
		out += '\n';
	}
	return WriteUtf8File(CachePath(), out);
}

bool LoadCache(std::vector<WaypointsData::Poi>& pois, std::vector<WaypointsData::MapRow>& maps)
{
	const std::wstring path = CachePath();
	WIN32_FILE_ATTRIBUTE_DATA fad{};
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
		return false;
	FILETIME nowFt{};
	GetSystemTimeAsFileTime(&nowFt);
	ULARGE_INTEGER now{}, then{};
	now.LowPart = nowFt.dwLowDateTime;
	now.HighPart = nowFt.dwHighDateTime;
	then.LowPart = fad.ftLastWriteTime.dwLowDateTime;
	then.HighPart = fad.ftLastWriteTime.dwHighDateTime;
	const ULONGLONG ageMs = (now.QuadPart - then.QuadPart) / 10000ULL;
	if (ageMs > kCacheTtlMs)
		return false;

	const std::string raw = ReadUtf8File(path);
	if (raw.size() < 20 || raw.rfind("#gw2igh-waypoints v", 0) != 0)
		return false;
	const size_t nl = raw.find('\n');
	if (nl == std::string::npos) return false;
	std::string verLine = raw.substr(0, nl);
	if (verLine.find(kCacheVer) == std::string::npos)
		return false;

	pois.clear();
	size_t p = nl + 1;
	while (p < raw.size())
	{
		size_t end = raw.find('\n', p);
		if (end == std::string::npos) end = raw.size();
		std::string line = raw.substr(p, end - p);
		p = end + 1;
		if (line.empty() || line[0] == '#') continue;
		std::string fields[8];
		size_t fi = 0, start = 0;
		for (size_t i = 0; i <= line.size() && fi < 8; ++i)
		{
			if (i == line.size() || line[i] == '\t')
			{
				fields[fi++] = line.substr(start, i - start);
				start = i + 1;
			}
		}
		if (fi < 6) continue;
		WaypointsData::Poi poi;
		poi.mapId = std::atoi(fields[0].c_str());
		poi.mapName = fields[1];
		poi.id = std::atoi(fields[2].c_str());
		poi.type = fields[3];
		poi.name = fields[4];
		poi.chatLink = fields[5];
		if (fi >= 8 && !fields[6].empty() && !fields[7].empty())
		{
			poi.continentX = static_cast<float>(std::atof(fields[6].c_str()));
			poi.continentY = static_cast<float>(std::atof(fields[7].c_str()));
			poi.hasCoord = true;
		}
		if (poi.mapId > 0 && poi.id > 0 && !poi.chatLink.empty())
			pois.push_back(std::move(poi));
	}
	if (pois.empty()) return false;
	RebuildMaps(pois, maps);
	return true;
}

DWORD WINAPI LoadProc(void*)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	std::vector<WaypointsData::Poi> pois;
	std::vector<WaypointsData::MapRow> maps;
	std::string status;
	const bool skipCache = gSkipCache.exchange(false);

	if (!skipCache && LoadCache(pois, maps))
	{
		status = "Loaded " + std::to_string(pois.size()) + " POIs (cache).";
	}
	else
	{
		status = "Downloading Tyria map floor…";
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPendingStatus = status;
		}
		auto t1 = Gw2Http::Api("/v2/continents/1/floors/1", nullptr, kFloorTimeoutMs);
		if (!t1.ok)
		{
			status = "Could not download Tyria floors — try again later.";
		}
		else
		{
			ParseFloorJson(t1.body, pois);
			status = "Downloading Mists floor…";
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPendingStatus = status;
			}
			auto t2 = Gw2Http::Api("/v2/continents/2/floors/1", nullptr, kFloorTimeoutMs);
			if (t2.ok)
				ParseFloorJson(t2.body, pois);

			if (pois.empty())
			{
				status = "No waypoints found in API response.";
			}
			else
			{
				RebuildMaps(pois, maps);
				SaveCache(pois);
				size_t wps = 0;
				for (const auto& p : pois)
					if (p.type == "waypoint") ++wps;
				status = "Indexed " + std::to_string(wps) + " waypoints / " +
					std::to_string(pois.size()) + " POIs on " +
					std::to_string(maps.size()) + " maps.";
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(gMu);
		gPendingPois = std::move(pois);
		gPendingMaps = std::move(maps);
		gPendingStatus = status;
		gPendingReady = true;
		gBusy = false;
	}
	return 0;
}
} // namespace WaypointsDataDetail

using namespace WaypointsDataDetail;

void WaypointsData::EnsureLoaded(bool force)
{
	if (!force && (gReady || gBusy))
		return;
	if (gBusy.exchange(true))
		return;
	if (gThread)
	{
		WaitForSingleObject(gThread, 0);
		CloseHandle(gThread);
		gThread = nullptr;
	}
	if (force)
	{
		gSkipCache = true;
		gReady = false;
		DeleteFileW(CachePath().c_str());
	}
	{
		std::lock_guard<std::mutex> lock(gMu);
		gStatus = "Loading waypoints…";
		gPendingStatus = gStatus;
	}
	gThread = CreateThread(nullptr, 0, LoadProc, nullptr, 0, nullptr);
	if (!gThread)
	{
		gBusy = false;
		std::lock_guard<std::mutex> lock(gMu);
		gStatus = "Could not start waypoint loader.";
	}
}

void WaypointsData::Tick()
{
	if (!gPendingReady)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	if (!gPendingReady)
		return;
	gPois = std::move(gPendingPois);
	gMaps = std::move(gPendingMaps);
	gPendingPois.clear();
	gPendingMaps.clear();
	gStatus = std::move(gPendingStatus);
	gPendingStatus.clear();
	gPendingReady = false;
	gReady = !gPois.empty();
	gLoadedAt = GetTickCount();
	if (gThread)
	{
		WaitForSingleObject(gThread, 0);
		CloseHandle(gThread);
		gThread = nullptr;
	}
}

bool WaypointsData::Busy()
{
	return gBusy.load();
}

bool WaypointsData::Ready()
{
	return gReady.load();
}

const char* WaypointsData::Status()
{
	static char buf[256];
	std::lock_guard<std::mutex> lock(gMu);
	const std::string& s = (gBusy.load() && !gPendingStatus.empty()) ? gPendingStatus : gStatus;
	std::snprintf(buf, sizeof(buf), "%s", s.c_str());
	return buf;
}

void WaypointsData::Search(const char* query, bool waypointsOnly, std::vector<Poi>& out, size_t maxN)
{
	out.clear();
	if (!query || !query[0] || maxN == 0) return;
	const std::string q = ToLowerCopy(query);
	std::lock_guard<std::mutex> lock(gMu);
	for (const Poi& p : gPois)
	{
		if (waypointsOnly && p.type != "waypoint") continue;
		const std::string n = ToLowerCopy(p.name);
		const std::string m = ToLowerCopy(p.mapName);
		if (n.find(q) == std::string::npos && m.find(q) == std::string::npos)
			continue;
		out.push_back(p);
		if (out.size() >= maxN) break;
	}
}

void WaypointsData::ListForMap(int mapId, bool waypointsOnly, std::vector<Poi>& out)
{
	out.clear();
	if (mapId <= 0) return;
	std::lock_guard<std::mutex> lock(gMu);
	for (const Poi& p : gPois)
	{
		if (p.mapId != mapId) continue;
		if (waypointsOnly && p.type != "waypoint") continue;
		out.push_back(p);
	}
	std::sort(out.begin(), out.end(),
		[](const Poi& a, const Poi& b) {
			return ToLowerCopy(a.name) < ToLowerCopy(b.name);
		});
}

bool WaypointsData::FindByChatLink(const char* chatLink, Poi& out)
{
	out = {};
	if (!chatLink || !chatLink[0])
		return false;
	std::string needle = chatLink;
	/* Allow pasting with surrounding text — keep first [&…]. */
	const size_t a = needle.find("[&");
	if (a != std::string::npos)
	{
		const size_t b = needle.find(']', a);
		if (b != std::string::npos)
			needle = needle.substr(a, b - a + 1);
	}
	std::lock_guard<std::mutex> lock(gMu);
	for (const Poi& p : gPois)
	{
		if (p.chatLink == needle)
		{
			out = p;
			return true;
		}
	}
	return false;
}

void WaypointsData::ListMaps(const char* filter, std::vector<MapRow>& out, size_t maxN)
{
	out.clear();
	const std::string q = filter && filter[0] ? ToLowerCopy(filter) : std::string{};
	std::lock_guard<std::mutex> lock(gMu);
	for (const MapRow& m : gMaps)
	{
		if (m.waypointCount <= 0) continue;
		if (!q.empty() && ToLowerCopy(m.name).find(q) == std::string::npos)
			continue;
		out.push_back(m);
		if (out.size() >= maxN) break;
	}
}

int WaypointsData::CurrentMapId()
{
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return 0;
	const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	if (!ctx || ctx->mapId == 0)
		return 0;
	return static_cast<int>(ctx->mapId);
}
