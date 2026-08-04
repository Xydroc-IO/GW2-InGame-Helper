#pragma once

#include "WaypointsData.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

/* Shared state / helpers for WaypointsData.cpp + WaypointsDataParse.cpp. */
namespace WaypointsDataDetail
{
	constexpr const char* kCacheVer = "5";
	constexpr DWORD kCacheTtlMs = 7u * 24u * 60u * 60u * 1000u;
	constexpr int kFloorTimeoutMs = 60000;

	extern std::mutex gMu;
	extern std::vector<WaypointsData::Poi> gPois;
	extern std::vector<WaypointsData::MapRow> gMaps;
	extern std::string gStatus;
	extern std::atomic<bool> gBusy;
	extern std::atomic<bool> gReady;
	extern std::atomic<bool> gPendingReady;
	extern std::atomic<bool> gSkipCache;
	extern std::vector<WaypointsData::Poi> gPendingPois;
	extern std::vector<WaypointsData::MapRow> gPendingMaps;
	extern std::string gPendingStatus;
	extern HANDLE gThread;
	extern DWORD gLoadedAt;

	std::wstring CachePath();
	std::string ToLowerCopy(std::string s);
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from);
	bool JsonCoordAfterKey(const std::string& json, const char* key, size_t from,
		float& outX, float& outY);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from);
	bool WriteUtf8File(const std::wstring& path, const std::string& data);
	std::string ReadUtf8File(const std::wstring& path);
	void RebuildMaps(const std::vector<WaypointsData::Poi>& pois,
		std::vector<WaypointsData::MapRow>& maps);
	std::string EscapeField(const std::string& s);
	bool SaveCache(const std::vector<WaypointsData::Poi>& pois);
	bool LoadCache(std::vector<WaypointsData::Poi>& pois, std::vector<WaypointsData::MapRow>& maps);
	void ParseFloorJson(const std::string& body, std::vector<WaypointsData::Poi>& pois);
}
