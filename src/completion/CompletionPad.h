#pragma once

namespace CompletionPad
{
	void OpenAndRefresh();
	bool Render();
	/* Proximity auto-tick + GPS arrow - call every UI frame. */
	void Tick();
}
