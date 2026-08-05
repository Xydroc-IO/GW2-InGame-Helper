#pragma once

/* ImGui Pathing panel — wrapping PadNav sections (no ◀ ▶ tab scroll):
   Overview / Features / Categories / Route. Pack data stays in PathingTrails. */
namespace PathingGuidesPad
{
	void Open();

	/* Draw when G::ShowPathingGuides. Returns true if pointer is over the window. */
	bool Render();
}
