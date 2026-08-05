#pragma once

#include "UnlocksData.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

/* Shared state / helpers for UnlocksData.cpp + UnlocksDataLoad.cpp. */
namespace UnlocksDetail
{
	constexpr int kHttpTimeoutMs = 12000;
	constexpr int kNameBatch = 200;
	constexpr DWORD kCacheTtlMs = 6u * 60u * 60u * 1000u;

	struct KindState
	{
		std::unordered_set<int> ids;
		std::unordered_map<int, std::string> names;
		std::string status = "Not loaded.";
		std::atomic<bool> busy{false};
		std::atomic<bool> ready{false};
		std::atomic<bool> pending{false};
		std::unordered_set<int> pendingIds;
		std::unordered_map<int, std::string> pendingNames;
		std::string pendingStatus;
		HANDLE thread = nullptr;
		DWORD loadedAt = 0;
	};

	extern std::mutex gMu;
	extern KindState gKinds[static_cast<int>(UnlocksData::Kind::Count)];
	extern const char* kLabels[];
	extern const char* kAccountPaths[];
	extern const char* kPublicPaths[];
	extern const char* kCacheNames[];

	int KindIndex(UnlocksData::Kind k);
	bool ValidKind(UnlocksData::Kind k);
	std::wstring CachePathW(UnlocksData::Kind k);
	std::string ToLowerCopy(std::string s);

	/* UnlocksDataLoad.cpp */
	bool WriteUtf8File(const std::wstring& path, const std::string& data);
	std::string ReadUtf8File(const std::wstring& path);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	void ParseIdArray(const std::string& body, std::unordered_set<int>& out);
	void ParseFinisherIds(const std::string& body, std::unordered_set<int>& out);
	void ParseNameObjects(const std::string& body, std::unordered_map<int, std::string>& names);
	bool LoadCache(UnlocksData::Kind k, std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names);
	void SaveCache(UnlocksData::Kind k, const std::unordered_set<int>& ids,
		const std::unordered_map<int, std::string>& names);
	void ResolveNames(UnlocksData::Kind k, const std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names, const char* key);

	struct LoadArg
	{
		UnlocksData::Kind kind = UnlocksData::Kind::Skins;
		bool force = false;
	};
	DWORD WINAPI LoadProc(void* p);
}
