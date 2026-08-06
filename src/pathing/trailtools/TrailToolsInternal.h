#pragma once

#include "TrailToolsShared.h"

/* Trail Tools pad tab drawers (TrailToolsPad*.cpp). */
namespace TrailToolsDetail
{
	void DrawLiveTab();
	void DrawTrailTab(); /* full tab when docked: desk + raw (legacy entry) */
	void DrawTrailDesk(); /* hub: XML project + trail list + open Trails1 */
	void DrawTrailRawEditor(); /* Trails1 window: .trl record/edit */
	void DrawMarkersTab();
	void DrawMarkersDesk(); /* hub: XML project + marker list + open Markers1 */
	void DrawMarkerRawEditor(); /* Markers1 window: selected POI attrs */
	void DrawXmlProjectDesk(); /* shared New/Load/Save/Save As OverlayData */
	void DrawPackTab();
	void DrawKeybindsTab();
	void DrawPoiScriptAttrs(DraftPoi& p);
	void DrawLuaFilesUi();
}
