#pragma once

#include <string>

/* Built-in raid feast / seasoning reference page. */
namespace RaidFood
{
	const char* Html();

	/* Write raid-food.html under addonDir and return a file:/// URL. */
	std::string EnsureFileUrl(const std::wstring& addonDir);
}
