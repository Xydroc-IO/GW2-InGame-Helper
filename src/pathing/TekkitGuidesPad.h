#pragma once

/* ImGui Pathing panel — wrapping PadNav sections (no ◀ ▶ tab scroll):
   Overview / Features / Categories / Route. Pack data stays in TekkitTrails. */
namespace TekkitGuidesPad
{
	void Open();

	/* Draw when G::ShowTekkitGuides. Returns true if pointer is over the window. */
	bool Render();
}
