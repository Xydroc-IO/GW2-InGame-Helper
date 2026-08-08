#pragma once

namespace WatchPad
{
	void Open();
	void CloseAll();

	/* Control pad (ShowWatch) + separate mirror window (ShowWatchMirror).
	   Returns true if pointer is over either window. */
	bool Render();
}
