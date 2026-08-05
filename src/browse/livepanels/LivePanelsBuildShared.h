#pragma once

/* Internal shared helpers for LivePanelsBuild* translation units. */

#include "LivePanelsBuild.h"

#include "Gw2Http.h"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
	constexpr DWORD kColorsTtlSec = 7u * 24u * 60u * 60u;
	constexpr DWORD kArmoryTtlSec = 24u * 60u * 60u;
	constexpr DWORD kPublicTtlSec = 60u * 60u;      /* craft / bosses / vault objs */
	constexpr DWORD kSeasonTtlSec = 6u * 60u * 60u;
	constexpr DWORD kAccountTtlSec = 3u * 60u;      /* personal vault / armory / chars */
	/* Per-request budget on workers only — UI always gets a shell instantly. */
	constexpr int kLiveHttpTimeoutMs = 4000;
	constexpr int kLiveBulkTimeoutMs = 8000;

	std::string WideToUtf8(const std::wstring& w);
	std::string PathToFileUrl(const std::wstring& path);
	std::wstring StemPath(const std::wstring& addonDir, const char* stem, const wchar_t* ext);
	bool FileFresh(const std::wstring& path, DWORD ttlSec);
	bool WriteUtf8File(const std::wstring& path, const std::string& data);
	std::string ReadUtf8File(const std::wstring& path);
	bool TryCacheHit(const std::wstring& addonDir, const char* stem, DWORD ttlSec,
		Gw2Http::Result& out);
	void StoreCache(const std::wstring& addonDir, const char* stem, const Gw2Http::Result& r);
	void PreferStaleCache(const std::wstring& addonDir, const char* stem, Gw2Http::Result& r);

	struct ParallelApiJob
	{
		const char* path = nullptr;
		const char* bearer = nullptr;
		int timeoutMs = kLiveHttpTimeoutMs;
		Gw2Http::Result* out = nullptr;
	};
	void RunParallelApis(ParallelApiJob* jobs, size_t n);

	std::string HtmlEscape(const std::string& s);
	std::string NowLocalStamp();
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0);
	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0);
	bool JsonBoolAfterKey(const std::string& json, const char* key, size_t from = 0);
	std::string ExtractTagInner(const std::string& xml, const char* tag, size_t from, size_t* nextOut);
	size_t JsonObjectEnd(const std::string& json, size_t openBrace);
	std::string ReadJsonQuoted(const std::string& s, size_t openQuote, size_t* after);
	std::string FormatIsoDateUtc(const std::string& iso);
	bool ParseIsoUtc(const std::string& iso, time_t* out);
	std::string SeasonDateBlurb(const std::string& startIso, const std::string& endIso);
	std::string HumanizeApiId(const std::string& id);
	void AppendChecklistSection(std::string& body, const char* sectionId, const char* title,
		const char* blurb, const std::string& jsonArray);
	void AppendVaultObjectives(std::string& body, const char* sectionId, const char* title,
		const std::string& json, bool accountScoped, const char* trackFilter = nullptr,
		int maxItems = 80);
}
