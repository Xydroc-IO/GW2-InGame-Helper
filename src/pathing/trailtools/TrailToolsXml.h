#pragma once

#include "TrailToolsShared.h"

#include <string>

namespace TrailToolsXml
{
	/* Combined: MarkerCategory tree + <POIs> (trails + markers) in one OverlayData. */
	std::string EmitOverlayData(const TrailToolsDetail::DraftPack& pack);
	/* Menu file: categories only (what appears in the Pathing menu). */
	std::string EmitMenuOverlay(const TrailToolsDetail::DraftPack& pack);
	/* Data file: <POIs> with Trail + POI elements only. */
	std::string EmitDataOverlay(const TrailToolsDetail::DraftPack& pack);

	std::string EmitTrailElement(const TrailToolsDetail::DraftTrail& trail);
	std::string EmitPoiElement(const TrailToolsDetail::DraftPoi& poi);

	bool WriteUtf8File(const std::wstring& path, const std::string& utf8);
	bool WriteOverlayFile(const std::wstring& path, const TrailToolsDetail::DraftPack& pack);
	/* Respects pack.xmlLayout: combined Pack.xml, or Pack_Menu.xml + Pack_Data.xml.
	   Removes the unused layout's files so Pathing does not double-index. */
	bool WritePackXmlLayout(const TrailToolsDetail::DraftPack& pack);
}
