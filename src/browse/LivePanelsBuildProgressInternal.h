#pragma once

/* Internal helpers for LivePanelsBuildProgress*.cpp translation units. */

#include "LivePanelsBuildShared.h"

#include <string>
#include <vector>

namespace LivePanelsBuild
{
	struct PriceRow
	{
		int id = 0;
		std::string name;
		long long buy = 0;
		long long sell = 0;
		bool custom = false;
	};

	std::string IdsQuery(const std::vector<int>& ids, size_t from, size_t count);
	void FetchItemNames(const std::vector<int>& ids, std::vector<PriceRow>& rows, int timeoutMs);
	void ApplyNamesFromJson(const std::string& json, std::vector<PriceRow>& rows);
	void EnsureArmoryNames(const std::wstring& addonDir,
		const std::vector<int>& ids, std::vector<PriceRow>& rows);
	std::string UrlEncodePathSegment(const std::string& s);
	void ParseArmoryCatalog(const std::string& body,
		std::vector<int>& armoryIds, std::vector<int>& maxCounts);
	std::string EnsureArmoryCatalogJson(const std::wstring& addonDir);
}
