#pragma once

/* ImGui Pathing panel — credits + category toggles for TacO packs.
   Compass / world drawing lives in CompassOverlay + WorldOverlay. */
namespace TekkitGuidesPad
{
	void Open();

	/* Draw when G::ShowTekkitGuides. Returns true if pointer is over the window. */
	bool Render();
}
