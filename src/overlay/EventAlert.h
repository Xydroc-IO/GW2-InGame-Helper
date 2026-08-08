#pragma once

/* On-screen toast when a tracked Events timetable row is soon or live. */
namespace EventAlert
{
	bool Render(); /* returns true if pointer is over the toast (for input capture) */

	/* Show a long-lived sample toast the user can drag to set position. */
	void BeginPlacement();
	void ResetPosition();
}
