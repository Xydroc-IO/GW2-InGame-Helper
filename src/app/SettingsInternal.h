#pragma once

#include <cstddef>

/* Shared by Settings.cpp / SettingsSave.cpp — one defining TU for gDirty. */
namespace SettingsDetail
{
	extern bool gDirty;
	const char* SettingsPath(char* out, size_t outLen);
}
