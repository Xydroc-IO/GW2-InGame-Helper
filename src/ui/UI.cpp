#include "UI.h"
#include "UIInternal.h"
#include "UI_Browse.h"

#include "BrowserTabs.h"
#include "CharacterProfiles.h"
#include "ConfirmedWaypoints.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "LivePanels.h"
#include "MumbleIdentity.h"
#include "NotesPad.h"
#include "TpWatchPad.h"
#include "LookupPad.h"
#include "WalletPad.h"
#include "VaultPad.h"
#include "AccountPad.h"
#include "EventsPad.h"
#include "LogManagerPad.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "PadNav.h"
#include "CompassOverlay.h"
#include "WorldOverlay.h"
#include "DirectionCompass.h"
#include "Settings.h"
#include "Sites.h"
#include "SyncQr.h"
#include "UiScale.h"
#include "WikiBrowser.h"
#include "WikiIpc.h"
#include "AddonPaths.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <shellapi.h>

using namespace UIDetail;

void UI_NoteHelperPopupHover()
{
	if (ImGui::IsWindowHovered(
		ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_ChildWindows))
		sHelperPopupHovered = true;
}

bool UI_BlocksGameKeyboard()
{
	return gUi.blockGameKeyboard;
}

/* True while keystrokes should go to the CEF page (not ImGui filter/search/find). */
bool UI_BrowserKeyboardActive()
{
	/* Pointer must be on the OSR page — sticky gUi.browserFocused alone stole
	   Browse/Find typing and game chat after leaving the page. */
	return G::ShowWiki && gUi.overBrowserPage;
}


bool UI_BlocksGameMouse()
{
	return gUi.blockGameMouse;
}

bool UI_IsPointerOverWiki(int clientX, int clientY)
{
	if (!G::ShowWiki || !gUi.wikiRectValid)
		return false;
	const float x = static_cast<float>(clientX);
	const float y = static_cast<float>(clientY);
	return x >= gUi.wikiMin.x && y >= gUi.wikiMin.y && x < gUi.wikiMax.x && y < gUi.wikiMax.y;
}

void UI_ReleaseGameInput()
{
	BlurBrowser();
	gUi.blockGameMouse = false;
	gUi.blockGameKeyboard = false;
	gUi.pendingDefocus = true;
	UI_ResetKeyRouting();
}
