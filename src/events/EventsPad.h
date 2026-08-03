#pragma once

/* ImGui world-event timers + track list (UTC timetable).
   Official API used only for optional claim marks — never reads game memory. */
namespace EventsPad
{
	void OpenAndRefresh();

	/* Draw when G::ShowEvents. Returns true if pointer is over the window. */
	bool Render();
}
