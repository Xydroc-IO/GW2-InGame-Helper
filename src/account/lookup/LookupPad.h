#pragma once

/* Free-floating ImGui item lookup - chat code / ID / name -> official API + wiki/BLTC.
   Does not use PadDock (user places the window). */
namespace LookupPad
{
	void OpenAndLookup(); /* show + focus; fetch if input already set */

	/* Body only - for AccountPad tabs (no own ImGui::Begin). */
	void RenderContents();

	/* Draw when G::ShowLookup. Returns true if pointer is over the window. */
	bool Render();
}
