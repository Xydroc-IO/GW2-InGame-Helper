#pragma once

#include <cstddef>
#include <string>

/* One entry per site the helper can open. Add new sites in Sites.cpp.
   Keep sites grouped by category (same category string, contiguous order). */
struct SiteDef
{
	const char* id;              /* stable settings key, e.g. "wiki" */
	const char* category;        /* picker group header, e.g. "Builds" */
	const char* label;           /* short picker label */
	const char* title;           /* window / status title */
	const char* homeUrl;         /* Home button + initial load */
	const char* searchUrlPrefix; /* nullptr = Search just opens home */
	const char* searchUrlSuffix; /* appended after UrlEncode(query) */
};

namespace Sites
{
	const SiteDef* All(size_t* outCount);
	const SiteDef& Active();
	int            ActiveIndex();
	const char*    ActiveId();

	/* Unique category names in registry order. Pointers into SiteDef::category. */
	const char* const* Categories(size_t* outCount);

	/* How many sites belong to category (exact string match). */
	int CountInCategory(const char* category);

	/* True if site matches a case-insensitive substring filter. */
	bool MatchesFilter(const SiteDef& site, const char* query);

	/* Switch site; returns true if the active site changed. */
	bool SetActiveIndex(int index);
	bool SetActiveById(const char* id);

	/* Build a search URL for the active site (falls back to home). */
	std::string SearchUrl(const std::string& query);

	/* True for the built-in how-to page. */
	bool IsHelpSite(const SiteDef& site);
	bool ActiveIsHelp();

	/* Resolve the URL to open for a site (help page or homeUrl). */
	std::string ResolveUrl(const SiteDef& site);
}
