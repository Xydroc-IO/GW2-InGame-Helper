#pragma once

/* Direction compass — heading strip, world N/E/S/W around the character, and
   bearing indicator. Behavior mirrors Raidcore GW2-Compass; drawing and gold
   theming are our own (HelperTheme). Independent of Tekkit CompassOverlay. */

namespace DirectionCompass
{
	void Render();
}
