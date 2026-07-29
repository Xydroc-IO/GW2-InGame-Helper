#pragma once

#include <string>

/* Runtime data directory: <GW2>/addons/GW2-InGame-Helper/
   The DLL itself lives in <GW2>/addons/ — only that file is outside this folder. */
namespace AddonPaths
{
	std::wstring DataDir();
	std::string  DataDirUtf8();
}
