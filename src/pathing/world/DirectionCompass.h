#pragma once

/* Direction compass — world N/E/S/W around the character. Raidcore-style
   placement with HelperTheme gold. Independent of Tekkit CompassOverlay. */

namespace DirectionCompass
{
	void Open();

	/* Enable + letter size + world radius. Used by the Compass pad and Options. */
	void DrawControls();

	/* Draw when G::ShowCompassPad. Returns true if pointer is over the window. */
	bool RenderPad();

	void Render();
}
