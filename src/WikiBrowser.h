#pragma once

#include <cstdint>
#include <string>

struct ID3D11ShaderResourceView;

namespace WikiBrowser
{
	void Init();
	void Shutdown();
	/* Poll graceful helper quit + deferred work — call every RT_Render. */
	void Tick();

	void SetVisible(bool visible);
	void SetBounds(float screenX, float screenY, float width, float height);

	void Navigate(const std::string& url);
	void NavigateHome();       /* always the how-to homepage */
	void NavigateActiveSite(); /* active site's home / help URL */
	void Search(const std::string& query);
	void GoBack();
	void GoForward();
	void Reload();

	void CreateTab(int slot, const char* url);
	void ActivateTab(int slot);
	void CloseTab(int slot);
	void Find(const char* text, bool forward, bool matchCase, bool findNext);
	void StopFind(bool clearSelection);

	void PresentFrame();

	/* Mouse coords are in CEF view pixels (same space as FrameWidth/Height). */
	void FeedMouseMove(int x, int y, bool leave, unsigned mods);
	void FeedMouseClick(int x, int y, int button, bool up, int clicks, unsigned mods);
	void FeedMouseWheel(int x, int y, int dx, int dy, unsigned mods);
	void FeedKey(int cefKeyType, int windowsVk, unsigned mods, unsigned character,
		int nativeKeyCode = 0, bool systemKey = false);
	void FeedFocus(bool focused);

	bool IsReady();
	bool HasFrame();
	bool HasTab(int slot); /* true if helper has a live CEF browser for this slot */
	int ActiveTabSlot(); /* helper IPC active_tab, or -1 if unknown */
	ID3D11ShaderResourceView* FrameSrv();
	int FrameWidth();
	int FrameHeight();

	bool CanGoBack();
	bool CanGoForward();
	const char* CurrentUrlCStr();   /* fenced cache; valid until next call */
	const char* CurrentTitleCStr();
	std::string CurrentUrl();
	std::string CurrentTitle(); /* active page title from CEF */
	std::string Status();
	const char* StatusCStr(); /* cached; no per-frame allocation */

	/* Used region of the OSR texture (content / tex size). Defaults to 1,1. */
	void FrameUvMax(float* outU, float* outV);

	uint32_t FindCount();
	uint32_t FindOrdinal();

	std::string UrlEncode(const std::string& value);
}
