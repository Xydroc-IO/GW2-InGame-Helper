#pragma once

#include "LookupPad.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "imgui/imgui.h"

/* Shared state / helpers for LookupPad.cpp + LookupFetch.cpp. */
namespace LookupDetail
{
	constexpr int kHttpTimeoutMs = 2500;

	struct Hit
	{
		int id = 0;
		std::string name;
		std::string rarity;
		std::string type;
		std::string level;
		long long buy = 0;
		long long sell = 0;
		bool hasPrices = false;
		bool ok = false;
		std::string status;
		std::vector<std::string> nameHints; /* wiki search titles when no id */
	};

	extern std::mutex gMu;
	extern Hit gHit;
	extern Hit gPending;
	extern std::atomic<bool> gBusy;
	extern std::atomic<bool> gReady;
	extern HANDLE gThread;
	extern char gQuery[192];
	extern char gThreadQuery[192];
	extern bool gFocus;
	extern bool gPlaceOnce;

	/* LookupFetch.cpp */
	std::string FormatCoins(long long copper);
	ImVec4 RarityColor(const std::string& r);
	int ParseItemId(const char* text);
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	std::string UrlEncode(const char* s);
	std::string WikiTitleToPath(const std::string& title);
	int ExtractWikiItemId(const std::string& wikitext);
	bool FillFromItemId(int id, Hit& hit);
	void WikiNameSearch(const char* query, Hit& hit);
	DWORD WINAPI LookupProc(void* param);
	void StartLookup();
	void Tick();
	void OpenUrl(const char* url);
}
