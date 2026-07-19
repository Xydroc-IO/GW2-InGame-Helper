#pragma once

void UI_Render();
void UI_Options();

/* True while the wiki UI should eat keyboard (no game skills / movement). */
bool UI_BlocksGameKeyboard();
/* True while the wiki UI should eat mouse buttons (no game skill clicks). */
bool UI_BlocksGameMouse();

/* Client-space hit test against the open wiki window (for WndProc). */
bool UI_IsPointerOverWiki(int clientX, int clientY);
/* Drop wiki keyboard focus so the game can move/skills again. */
void UI_ReleaseGameInput();
