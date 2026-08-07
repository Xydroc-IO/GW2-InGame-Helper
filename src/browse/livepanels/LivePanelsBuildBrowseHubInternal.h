#pragma once

/* Internal helpers shared by LivePanelsBuildBrowseHub*.cpp translation units. */

#include "LivePanelsBuildShared.h"

#include "Sites.h"

#include <string>

namespace LivePanelsBuild
{
	std::string Esc(const std::string& s);
	std::string ToLower(std::string s);
	const char* HubCss();
	const char* HubJs();
	void AppendBrowseHeroArt(std::string& html);
	std::string SectionAnchorId(const std::string& section, int index);
	void AppendTile(std::string& html, const SiteDef& s, const std::string& pathBlurb,
		bool withFolderMove, int currentFolderId);
	void AppendTile(std::string& html, const SiteDef& s, const std::string& pathBlurb);
}
