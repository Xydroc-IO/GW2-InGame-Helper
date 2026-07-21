#pragma once

#include <string>

/* Built-in offline reference pages (Raid Food style). */
namespace CheatSheets
{
	struct Sheet
	{
		const char* id;       /* sites id, e.g. "raidutils" */
		const char* about;    /* "about:raid-utilities" */
		const char* fileStem; /* "raid-utilities" → .html / .ver */
		const char* version;
		const char* browseLabel;
		const char* browseTitle;
	};

	const Sheet* All(size_t* outCount);
	const Sheet* FindByAbout(const char* aboutUrl);

	/* Write sheet HTML under addonDir and return a file:/// URL. */
	std::string EnsureFileUrl(const std::wstring& addonDir, const Sheet& sheet);

	/* If url is a known about: cheat sheet, resolve to file:///; else {}. */
	std::string ResolveAboutUrl(const std::wstring& addonDir, const std::string& url);
}
