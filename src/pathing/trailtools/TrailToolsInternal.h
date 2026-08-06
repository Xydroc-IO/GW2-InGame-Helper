#pragma once

#include "TrailToolsShared.h"

/* Trail Tools pad tab drawers (TrailToolsPad*.cpp). */
namespace TrailToolsDetail
{
	void DrawLiveTab();
	void DrawTrailTab(); /* full tab when docked: desk + raw (legacy entry) */
	void DrawTrailDesk(); /* XML project + trail list + open TrailsN */
	void DrawTrailRawEditor(); /* uses gDraft.active (after PushTrailEditorToActive) */
	void DrawMarkersTab();
	void DrawMarkersDesk(); /* XML project + marker list + open MarkersN */
	void DrawMarkerRawEditor(); /* uses gDraft.selectedPoi */
	void DrawMarkerRawEditorForSlot(int slot); /* MarkersN bound to slot.poiIndex */
	void DrawXmlProjectDesk(); /* shared New/Load/Save/Save As OverlayData */
	void DrawPackTab();
	void DrawKeybindsTab();
	void DrawPoiScriptAttrs(DraftPoi& p);
	void DrawLuaFilesUi();
}
