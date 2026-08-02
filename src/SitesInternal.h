#pragma once

/* Internal catalog storage shared by Sites.cpp / SitesLoad.cpp. */

#include "Sites.h"

#include <cstddef>

namespace SitesDetail
{
	extern SiteDef* gSites;
	extern int gSiteCount;

	/* Load/extract DataDir/sites.json into gSites. Returns false if only fallback. */
	bool LoadCatalog();
	void ClearCatalog();

	const char* const* BrowseSectionsFor(const char* category, size_t* outCount);
}
