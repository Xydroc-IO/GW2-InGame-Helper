#pragma once

namespace WatchPad
{
	void Open();
	void CloseAll();
	/* Side rail / keybind: show or hide the Start/Stop control pad only.
	   Does not stop Mirror / capture (use Stop or close Mirror for that). */
	void ToggleControl();

	/* Control pad (ShowWatch) + separate mirror window (ShowWatchMirror).
	   Returns true if pointer is over either window. */
	bool Render();
}
