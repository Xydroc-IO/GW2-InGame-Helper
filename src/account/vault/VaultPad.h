#pragma once

/* Free-floating ImGui Dailies & Wizard's Vault pad (side rail + Ctrl+Shift+V). */
namespace VaultPad
{
	void OpenAndRefresh();
	void RefreshData(); /* fetch without opening the floating window */

	/* Body only - for AccountPad tabs (no own ImGui::Begin). */
	void RenderContents();

	/* Draw when G::ShowVault. Returns true if pointer is over the window. */
	bool Render();
}
