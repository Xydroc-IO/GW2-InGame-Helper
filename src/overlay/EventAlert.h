#pragma once

/* On-screen toast when a timetable row is soon, spawn-live, or ending. */
namespace EventAlert
{
	bool Render(); /* returns true if pointer is over the toast (for input capture) */

	/* Show a long-lived sample toast the user can drag to set position. */
	void BeginPlacement();
	void ResetPosition();
}
