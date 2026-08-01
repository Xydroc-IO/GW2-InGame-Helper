#pragma once

/* ImGui Trading Post pad — delivery box (API key) + watchlist buy/sell prices
   + optional sell≤ alerts (highlight on refresh). Kept out of CEF for reliability.
   Read-only official API only — never buys, sells, or claims. */
namespace TpWatchPad
{
	void Load(); /* no-op; ids/alerts live in settings.ini */
	void Tick(); /* apply finished price + delivery fetches */

	/* Draw when G::ShowTpWatch. Returns true if pointer is over the window. */
	bool Render();

	/* Open panel + refresh prices / delivery (toolbar TP / Options). */
	void OpenAndRefresh();
}
