#pragma once

#include <string>

/* Resolve a hovered (or clipboard) item chat link into a wiki page.
   Uses Windows clipboard + optional user-triggered key macros — no GW2 memory. */

namespace ItemLookup
{
	void Init();
	void Shutdown();

	/* Hover an inventory/TP/crafting item, then call this (keybind). */
	void LookupHoveredItem();

	/* Open wiki from an existing `[&...]` chat link already on the clipboard. */
	void LookupClipboard();

	bool IsBusy();
	std::string Status();
}
