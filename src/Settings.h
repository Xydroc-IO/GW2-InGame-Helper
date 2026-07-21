#pragma once

namespace Settings
{
	void Load();
	/* Writes if dirty. Debounced unless force (use force on unload). */
	void Save(bool force = false);
	void SetDirty();
}
