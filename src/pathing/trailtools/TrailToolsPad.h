#pragma once

/* ImGui Trail Tools - hub tabs, with Trails / Markers optionally popped out
   into their own collapsible windows (title bar remains when minimized). */
namespace TrailToolsPad
{
	void Open();
	void OpenTrailsWindow();
	void OpenMarkersWindow();
	bool Render(); /* hub + any pop-outs; true while pointer over any of them */
	bool AnyOpen();
}
