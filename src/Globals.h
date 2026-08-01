#pragma once

#include "nexus/Nexus.h"

#define ADDON_NAME "GW2-InGame-Helper"
#define ADDON_SIG  0x48454C50u /* 'HELP' */

namespace G
{
	extern AddonDefinition_t AddonDef;
	extern AddonAPI_t*       API;
	extern HMODULE           Self;

	extern bool  ShowWiki; /* overlay window visible (name kept for settings compat) */
	extern bool  ShowOptions;
	extern bool  ShowNotes; /* ImGui Notes + clipboard helpers window */
	extern bool  ShowTpWatch; /* ImGui TP watchlist (add/remove + prices) */
	extern float Opacity;
	extern float FontScale;
	extern float WindowWidth;
	extern float WindowHeight;
	extern float WindowPosX;
	extern float WindowPosY;
	extern bool  HasSavedPos;
	extern bool  HasSavedSize; /* false until WindowWidth/Height loaded or user resizes */
	extern bool  KeepHelperWarm; /* hide helper without killing CEF (uses more RAM) */
	extern char  LastQuery[128];
	extern char  ActiveSiteId[64];
	extern char  DefaultSiteId[64]; /* Home button + landing when no tabs restored */
	extern char  Gw2ApiKey[128]; /* optional account API key — Live panels; local only */
	extern char  TpWatchIds[1024]; /* comma-separated item ids — user TP watchlist */
}
