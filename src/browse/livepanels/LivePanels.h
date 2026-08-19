#pragma once

#include <string>

/* Live Browse panels — DLL-generated about: HTML (same 1-DLL install as cheat sheets). */
namespace LivePanels
{
	/* Write / refresh panel under addonDir; return file:/// URL.
	   Always returns a file URL for known about:live-* (never leave CEF on raw about:).
	   Network + HTML write run on background workers (never the game thread).
	   Call Tick() each frame only to navigate CEF when a result is ready. */
	std::string ResolveAboutUrl(const std::wstring& addonDir, const std::string& url);

	/* Apply finished background fetches (reload CEF if the Live tab is open). */
	void Tick();

	/* Drop cached live-*.html so the next open refetches (e.g. after API key change). */
	void InvalidateCaches(const std::wstring& addonDir);

	/* Invalidate only the TP watchlist panel (keeps other Live pages cached). */
	void InvalidateTpCache(const std::wstring& addonDir);

	/* User opened Legendary Ledger — bust armory cache once (not on ResolveAboutUrl poll). */
	void BumpLegendaryVaultOpen(const std::wstring& addonDir);

	/* Soft-stop Live workers (bounded wait) before DLL unload. */
	void Shutdown();

	bool IsLiveAbout(const char* url);

	/* True if URL is a live panel about: or its live-*.html file. */
	bool IsLiveUrl(const char* url);

	/* Favorites add/remove/reorder — wipe Browse hub cache and reload if open. */
	void NotifyFavoritesChanged();
}
