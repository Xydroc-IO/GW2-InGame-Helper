#pragma once

/* Our Account pad - ImGui tabs for Overview / Progress / Unlocks / History.
   Vault is its own side-rail pad. Original UI on the official API - no
   third-party site ties inside this pad. Stash / trading / item / crafting
   live on EconomyPad. */
namespace AccountPad
{
	void OpenAndRefresh();

	/* Draw when G::ShowAccount. Returns true if pointer is over the window. */
	bool Render();
}
