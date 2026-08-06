#pragma once

#include <cstdio>

void UI_Render();
void UI_Options();

/* True while the wiki UI should eat keyboard (no game skills / movement). */
bool UI_BlocksGameKeyboard();
/* True while keystrokes belong to the CEF page (vs ImGui filter/search/find). */
bool UI_BrowserKeyboardActive();
/* True while the wiki UI should eat mouse buttons (no game skill clicks). */
bool UI_BlocksGameMouse();

/* Client-space hit test against the open wiki window (for WndProc). */
bool UI_IsPointerOverWiki(int clientX, int clientY);
/* Drop wiki keyboard focus so the game can move/skills again. */
void UI_ReleaseGameInput();
/* Send KEYUP to GW2 for held movement keys when left-clicking into the helper
   (avoids stuck autorun). Not used on hover - that broke RMB camera look. */
void UI_ReleaseHeldGameKeys();
/* Clear browser/ImGui key ownership + ImGui capture so GW2 chat works after close. */
void UI_ResetKeyRouting();

/* Persist Browse collapsing-header open state (settings.ini). */
void UI_ParseBrowseOpen(const char* val);
void UI_WriteBrowseOpen(FILE* f);

/* Mark helper popup hover for game-input capture (Browse / More / tab menus). */
void UI_NoteHelperPopupHover();

/* Frame poll for Ctrl+Shift hotkeys - defined in entry.cpp, runs even when closed. */
void HelperHotkeys_Poll();
