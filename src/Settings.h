#pragma once

namespace Settings
{
	void Load();
	/* Writes if dirty. Debounced (~2.5s) unless force — use force only on AddonUnload. */
	void Save(bool force = false);
	void SetDirty();
}
