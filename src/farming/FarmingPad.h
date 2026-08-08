#pragma once

namespace FarmingPad
{
	void OpenAndRefresh();
	bool Render();
	/* Marker proximity auto-check — call every UI frame while pad may be open. */
	void Tick();
}
