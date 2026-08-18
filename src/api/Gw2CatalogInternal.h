#pragma once

#include "Gw2Catalog.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace Gw2CatalogDetail
{
	extern std::mutex gMu;
	extern std::unordered_map<char, std::unordered_map<int, std::string>> gNames;
	extern std::unordered_map<char, std::unordered_map<int, std::string>> gIcons;
	extern std::unordered_map<char, std::unordered_map<int, std::string>> gExtra;
	extern std::unordered_map<int, Gw2Catalog::Recipe> gRecipes;
	extern std::unordered_map<int, std::vector<int>> gByOutput;
	extern std::string gBuild;
	extern bool gDiskLoaded;
	extern bool gRecipesOnDisk;
	extern std::atomic<bool> gBusy;
	extern HANDLE gThread;
	extern DWORD gLastCheckMs;

	std::wstring VerPath();
	std::wstring TsvPath();
	std::wstring RecipesPath();
	std::wstring PackCachePath();
	std::wstring IconsCachePath();
	std::wstring IconsVerPath();
	std::string ReadAll(const std::wstring& path, size_t maxBytes);
	bool WriteAll(const std::wstring& path, const std::string& data);
	bool ApplyIghBytes(const std::string& pack);
	bool TryApplyLocalIgh();
	bool TryOpenLocalIcons();
	void FetchRemoteIcons();
	bool LookupLocked(const std::unordered_map<int, std::string>& map, int id, std::string* out);
	bool LookupKind(char kind,
		const std::unordered_map<char, std::unordered_map<int, std::string>>& root,
		int id, std::string* out);
	void ParseTsv(const std::string& tsv);
	void ParseRecipes(const std::string& tsv);
	void LoadDisk();
	bool DiscMatch(const std::string& discs, const char* prefer);
}
