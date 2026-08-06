#pragma once

/* ImGui Settings pad - same controls that used to live only in Nexus Options. */
namespace SettingsPad
{
	void Open();

	/* Draw when G::ShowSettings. Returns true if pointer is over the window. */
	bool Render();

	/* Shared form body (also available if Nexus stub needs a one-click open). */
	void DrawContents();
}
