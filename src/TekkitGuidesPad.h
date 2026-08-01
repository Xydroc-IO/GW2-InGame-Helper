#pragma once

/* ImGui control panel for Tekkit's Guides — credit + category toggles.
   Compass / world drawing lives in CompassOverlay + WorldOverlay. */
namespace TekkitGuidesPad
{
	void Open();

	/* Draw when G::ShowTekkitGuides. Returns true if pointer is over the window. */
	bool Render();
}
