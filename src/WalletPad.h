#pragma once

/* Free-floating ImGui wallet + searchable account stash (API key).
   Wallet currencies, material storage, bank, shared slots, and per-character bags.
   Does not use PadDock (user places the window). */
namespace WalletPad
{
	void OpenAndRefresh();

	/* Draw when G::ShowWallet. Returns true if pointer is over the window. */
	bool Render();
}
