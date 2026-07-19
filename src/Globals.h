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
	extern float Opacity;
	extern float FontScale;
	extern float WindowWidth;
	extern float WindowHeight;
	extern float WindowPosX;
	extern float WindowPosY;
	extern bool  HasSavedPos;
	extern char  LastQuery[128];
	extern char  ActiveSiteId[64];
}
