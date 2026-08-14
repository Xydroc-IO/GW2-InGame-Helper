#pragma once

/* Economy companion — flips / charts / cart, trading, item lookup
   (read-only official API). */
namespace EconomyPad
{
	void OpenAndRefresh();
	/* Trading + flip scan. force=true bypasses caches. */
	void RefreshAll(bool force = false);
	bool Render();
}
