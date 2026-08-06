#pragma once

/* ImGui Trail Tools — hub + Trails/Markers desks + multiple TrailsN/MarkersN editors
   (each collapsible to a title bar). */
namespace TrailToolsPad
{
	void Open();
	void OpenTrailsWindow();  /* open next TrailsN raw editor */
	void OpenMarkersWindow(); /* open MarkersN for selected POI */
	void OpenTrailsDesk();
	void OpenMarkersDesk();
	bool Render(); /* hub + desks + editors; true while pointer over any */
	bool AnyOpen();
}
