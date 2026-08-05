#pragma once

/* ImGui DPS Logs pad — browse ArcDPS EVTC/ZEVTC logs via Elite Insights JSON.
   Original Helper UI (not a clone of third-party log managers). */
namespace LogManagerPad
{
	void OpenAndRefresh();

	/* Draw when G::ShowLogManager. Returns true if pointer is over the window. */
	bool Render();
}
