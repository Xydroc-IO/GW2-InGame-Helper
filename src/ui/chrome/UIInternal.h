#pragma once

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

/* Shared helper-window state for UI*.cpp. */
namespace UIDetail
{
	float Clampf(float v, float lo, float hi);

	struct UiContext
	{
		bool forceHelperOnScreen = false;
		bool browserFocused = false;
		bool overBrowserPage = false;
		bool blockGameKeyboard = false;
		bool blockGameMouse = false;
		bool wikiRectValid = false;
		ImVec2 wikiMin{};
		ImVec2 wikiMax{};
		bool pendingDefocus = false;
	};
	extern UiContext gUi;

	bool HelperGeomOffscreen(float dispW, float dispH);
	void ClampHelperGeomToDisplay();

	extern const ImVec4& kGold;
	extern const ImVec4& kGoldBright;
	extern const ImVec4& kGoldDim;
	extern const ImVec4& kGoldMuted;
	extern const ImVec4& kMuted;
	extern const ImVec4& kBg;
	extern const ImVec4& kPanel;
	extern const ImVec4& kBorder;
	extern const ImVec4& kTabActive;
	extern const ImVec4& kTabIdle;
	extern const ImVec4& kWarn;

	void PushWikiTheme();
	void PopWikiTheme();
	bool SoftButton(const char* label, bool enabled);
	void BlurBrowser();
	void FocusBrowser();
	void FocusBrowserForce();
	unsigned CefModsFromImGui(const ImGuiIO& io);
	void MapToCef(float localX, float localY, float drawW, float drawH, int* outX, int* outY);
	bool HostWantsDesktopAdViewport(const char* url);
	void DesktopAdCefSize(float panelW, float panelH, float* outW, float* outH);
	/* Uniform fit of contentWxcontentH into panel (letterbox). Keeps aspect. */
	void FitContentInPanel(float panelW, float panelH, float contentW, float contentH,
		float* outX, float* outY, float* outW, float* outH);

	extern bool sShowFind;
	extern bool sHelperPopupHovered;
	extern bool sFocusFind;
	extern bool sFocusAddress;
	extern char sFindQuery[128];
	extern bool sFindMatchCase;

	bool AnyToolPadOpen();
	bool HoveringComboPopup();
	bool ComboPopupOpen();
	void CaptureForToolPads(bool padsHover);

	void DrawTabBar();
	void CopyCurrentUrl();
	void OpenCurrentExternal();
	void DuplicateActiveTab();
	void DrawStatusChip();
	void DrawMoreMenu();
	void DrawToolbar();
	void DrawBookmarkBar();
	void OpenStarBookmarkPopup(const char* title, const char* url);
	void DrawStarBookmarkPopup();
	void DrawHelperSideRail();
	/* Same width DrawHelperSideRail uses (for title-bar left flush). */
	float HelperSideRailWidth();

	/* Companion ImGui pads — call AFTER DrawHelperSideRail so submit order
	   cannot leave pads between the nav column and the helper body. */
	void RenderCompanionPads();

	/* Keep ##gw2igh_side_dock immediately under the helper in display order so
	   focused pads cannot sandwich between rail and helper content. */
	void GlueSideRailDisplayOrder();

	/* OSR page slot + input capture (open helper path). Pads render after the rail. */
	void DrawWikiPageSlot(bool open);
}
