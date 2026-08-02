#pragma once

/* Local ImGui notes + clipboard helpers, plus waypoint / POI search (chat codes). */
namespace NotesPad
{
	void Load();
	void Save(bool force = false);

	/* Show + dock beside the helper (user can still drag afterward). */
	void Open();

	/* Draw the Notes window when G::ShowNotes. Returns true if pointer is over it. */
	bool Render();
}
