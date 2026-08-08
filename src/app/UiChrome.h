#pragma once

#include <string>

/* Embedded curated GW2 UI chrome (data/ui-chrome → DLL zip → DataDir/ui-chrome).
   ArenaNet owns these textures; not relicensed under MIT. */

namespace UiChrome
{
	/* Extract pack when stamp mismatches. Safe to call often. */
	bool Ensure(const std::wstring& addonDir);

	/* Absolute path to an extracted PNG (empty if missing). */
	std::wstring PngPath(const std::wstring& addonDir, int assetId);
	std::wstring NamedPngPath(const std::wstring& addonDir, const char* fileName);

	/* file:/// URL for HTML backgrounds (empty if Ensure/file missing). */
	std::string FillFileUrl(const std::wstring& addonDir, int assetId = 155985);
	/* file:/// URL for a named pack PNG (e.g. "browse-hero.png"). */
	std::string NamedFileUrl(const std::wstring& addonDir, const char* fileName);

	/* CSS that layers curated plaque/button/divider textures onto HTML pages. */
	std::string DecorCss(const std::wstring& addonDir);

	/* Request Nexus upload from extracted files (prefer over CDN for chrome). */
	void WarmTextures(const std::wstring& addonDir);

	/* Texture id used with Textures_Get / FromFile. */
	void MakeTexId(int assetId, char* out, size_t outLen);
	void MakeNamedTexId(const char* fileStem, char* out, size_t outLen);
}
