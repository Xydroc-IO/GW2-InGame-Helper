#pragma once

#include <string>

/* Runtime data directory: <GW2>/addons/GW2-InGame-Helper/
   The DLL itself lives in <GW2>/addons/ — only that file is outside this folder.

   Nested layout:
     pages/       generated HTML + home assets
     live/cache/  live-*.json API caches
     cache/       unlocks / waypoints / stash caches
     cmds/        helper ↔ DLL cmd files
     cheatsheets/ cef/ pathing/ ei/  (unchanged) */
namespace AddonPaths
{
	std::wstring DataDir();
	std::string  DataDirUtf8();

	/* Subdirs under DataDir(); CreateDirectoryW on access. */
	std::wstring PagesDir();
	std::wstring LiveCacheDir();
	std::wstring CacheDir();
	std::wstring CmdsDir();

	/* Join DataDir (or an explicit root) with a relative subpath, ensuring parents exist. */
	std::wstring EnsureUnder(const std::wstring& root, const wchar_t* relative);
}
