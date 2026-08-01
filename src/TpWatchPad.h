#pragma once

/* ImGui Trading Post watchlist — add/remove any item, show buy/sell prices.
   Kept out of CEF so clicks and API fetches are reliable under Wine. */
namespace TpWatchPad
{
	void Load(); /* no-op; ids live in settings.ini via G::TpWatchIds */
	void Tick(); /* apply finished price fetches */

	/* Draw when G::ShowTpWatch. Returns true if pointer is over the window. */
	bool Render();

	/* Open panel + refresh prices (toolbar TP / Options). */
	void OpenAndRefresh();
}
