#pragma once

/* ImGui Trail Tools — separate windows so Trails + Markers can stay open together
   (place markers along a trail). Shared draft state in TrailToolsDetail::gDraft. */
namespace TrailToolsPad
{
	void Open();          /* hub: Live + Pack */
	void OpenTrails();    /* trail recorder */
	void OpenMarkers();   /* marker drop / edit */

	bool Render();        /* hub */
	bool RenderTrails();
	bool RenderMarkers();

	bool AnyOpen();
}
