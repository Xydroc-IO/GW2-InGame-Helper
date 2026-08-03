#pragma once

namespace Settings
{
	void Load();
	/* Writes if dirty. Debounced (~2.5s) unless force — use force on unload / pathing toggles. */
	void Save(bool force = false);
	void SetDirty();
	/* Mark dirty and write immediately (pathing toggles must survive Nexus reload). */
	void SaveNow();
}
