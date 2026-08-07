#pragma once

/* Economy companion — flips / charts / cart plus stash, trading, item lookup,
   and crafting (read-only official API). */
namespace EconomyPad
{
	void OpenAndRefresh();
	/* Stash + trading + crafting dailies + flip scan. force=true bypasses caches. */
	void RefreshAll(bool force = false);
	bool Render();
}
