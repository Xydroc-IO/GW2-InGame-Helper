#pragma once

/* ImGui Trading Post pad - delivery box (API key) + watchlist buy/sell prices
   + optional sell≤ alerts (highlight on refresh). Kept out of CEF for reliability.
   Read-only official API only - never buys, sells, or claims. */
namespace TpWatchPad
{
	void Load(); /* no-op; ids/alerts live in settings.ini */
	void Tick(); /* apply finished price + delivery fetches */

	void RefreshData(); /* sync watchlist + fetch without opening the window */

	/* Body only - for AccountPad tabs. forceScroll=true uses a scrolling list. */
	void RenderContents(bool forceScroll = false);

	/* Draw when G::ShowTpWatch. Returns true if pointer is over the window. */
	bool Render();

	/* Open panel + refresh prices / delivery (toolbar TP / Options). */
	void OpenAndRefresh();
}
