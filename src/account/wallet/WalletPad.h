#pragma once

/* Free-floating ImGui wallet + searchable account stash (API key).
   Wallet currencies, material storage, bank, shared slots, and per-character bags.
   Does not use PadDock (user places the window). */
namespace WalletPad
{
	void OpenAndRefresh();
	void RefreshData(); /* fetch without opening the floating window */

	/* Open Wallet on Characters location filtered to this toon name. */
	void FocusCharacterBags(const char* characterName);

	/* Body only - for AccountPad tabs (no own ImGui::Begin). */
	void RenderContents();

	/* Draw when G::ShowWallet. Returns true if pointer is over the window. */
	bool Render();
}
