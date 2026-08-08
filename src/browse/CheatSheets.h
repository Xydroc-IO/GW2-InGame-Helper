#pragma once

#include <string>

/* Offline reference pages — HTML/CSS live in data/cheatsheets/ (embedded zip). */
namespace CheatSheets
{
	struct Sheet
	{
		const char* id;       /* sites id, e.g. "raidutils" */
		const char* about;    /* "about:raid-utilities" */
		const char* fileStem; /* "raid-utilities" → cheatsheets/<stem>.html */
		const char* version;
		const char* browseLabel;
		const char* browseTitle;
	};

	const Sheet* All(size_t* outCount);
	const Sheet* FindByAbout(const char* aboutUrl);

	/* Ensure pack extracted under addonDir/cheatsheets/ and return file:/// URL. */
	std::string EnsureFileUrl(const std::wstring& addonDir, const Sheet& sheet);

	/* If url is a known about: cheat sheet, resolve to file:///; else {}. */
	std::string ResolveAboutUrl(const std::wstring& addonDir, const std::string& url);

	/* Patch user-theme CSS into shared.css only if the pack is already extracted.
	   Never extracts — safe to call from Settings on the UI thread. */
	void RefreshUserThemeCss();
}
