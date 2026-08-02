#pragma once

/* Our Account pad — ImGui tabs for Overview / Stash / Vault / Trading /
   Item / Crafting / Progress (legendaries + characters). Original UI on
   the official API — no third-party site ties inside this pad. */
namespace AccountPad
{
	void OpenAndRefresh();

	/* Draw when G::ShowAccount. Returns true if pointer is over the window. */
	bool Render();
}
