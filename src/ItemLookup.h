#pragma once

#include <string>

/* Resolve a clipboard item chat link `[&...]` into a wiki page.
   Clipboard-only — no GW2 memory reads and no injected click/chat macros. */
namespace ItemLookup
{
	void Init();
	void Shutdown();

	/* Alias: opens wiki from clipboard `[&...]` if present. */
	void LookupHoveredItem();
	void LookupClipboard();

	bool IsBusy();
	std::string Status();
}
