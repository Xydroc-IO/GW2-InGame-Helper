#pragma once

/* Load-time catalog storage / JSON parse helpers for SitesLoad*.cpp. */

#include "SitesInternal.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace SitesDetail
{
	constexpr int kMaxBrowsePath = 8;

	struct SiteOwned
	{
		std::string id;
		std::string category;
		std::string label;
		std::string title;
		std::string homeUrl;
		std::string searchPrefix;
		std::string searchSuffix;
		bool hasSearchPrefix = false;
		bool hasSearchSuffix = false;
		std::string browsePath[kMaxBrowsePath];
		int browsePathCount = 0;
		const char* browsePtrs[kMaxBrowsePath]{};
	};

	extern std::vector<SiteOwned> gOwned;
	extern std::vector<SiteDef> gDefs;

	struct SectionList
	{
		std::vector<std::string> names;
		std::vector<const char*> ptrs;
	};
	extern std::unordered_map<std::string, SectionList> gSections;

	extern SiteOwned gFallbackOwned;
	extern SiteDef gFallbackDef;
	extern const char* gFallbackBrowse[1];

	extern const char* kSitesStamp;

	void SkipWs(const char*& p, const char* end);
	bool ParseString(const char*& p, const char* end, std::string& out);
	bool Expect(const char*& p, const char* end, char ch);
	void SkipValue(const char*& p, const char* end);
	bool ParseStringOrNull(const char*& p, const char* end, std::string& out, bool& isNull);
	bool ParseBrowsePath(const char*& p, const char* end, SiteOwned& site);
	bool ParseSiteObject(const char*& p, const char* end, SiteOwned& site);
	bool ParseStringArray(const char*& p, const char* end, std::vector<std::string>& out);
	bool ParseBrowseSections(const char*& p, const char* end);
	bool ParseRoot(const std::string& json);

	void InstallFallback();
	std::wstring SitesPathW();
	bool ExtractEmbeddedSites();
	bool ReadFileUtf8(const std::wstring& path, std::string& out);
}
