#pragma once

/* Free-floating ImGui Dailies & Wizard’s Vault pad.
   Same official API data as Browse → Live → Dailies & Vault; not PadDocked. */
namespace VaultPad
{
	void OpenAndRefresh();

	/* Draw when G::ShowVault. Returns true if pointer is over the window. */
	bool Render();
}
