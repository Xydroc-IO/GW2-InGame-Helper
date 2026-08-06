#pragma once

#include "TrailToolsShared.h"

#include <string>

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

	/* Shared by TrailToolsPadTrailDesk / Raw / Helpers. */
	void SyncActiveType();
	void SyncActiveFileRelFromStem();
	void ApplyStemFromFileRel();
	void MarkDirty();

	std::wstring Utf8ToWide(const char* u);
	std::string WideToUtf8(const std::wstring& w);
	std::wstring PackRelToAbs(const std::string& fileRel);
	std::wstring TrailsFolder();
	std::wstring ActiveTrlPath();
	void RememberDirFromPath(const std::wstring& fullPath);
	std::wstring DialogStartDir();
	bool IsSectionBreak(const PathingTrails::WorldPoint& p);
	bool TryAbsUnderPack(const std::wstring& absPath, std::string& outRel);
	void RegisterActiveInPack();
	bool SaveActiveToPath(const std::wstring& path);
	bool DialogPickTrl(bool saveAs, std::wstring& outPath);
}
