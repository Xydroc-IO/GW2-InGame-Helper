#pragma once

#include "TrailToolsShared.h"

/* Trail Tools pad tab drawers (TrailToolsPad*.cpp). */
namespace TrailToolsDetail
{
	void DrawLiveTab();
	void DrawTrailTab();
	void DrawMarkersTab();
	void DrawPackTab();
	void DrawKeybindsTab();
	void DrawPoiScriptAttrs(DraftPoi& p);
	void DrawLuaFilesUi();
}
