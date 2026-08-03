#pragma once

#include <string>

/* Parses MumbleLink identity JSON for the active character name.
   Detects swaps; does not drive UI by itself. */
namespace MumbleIdentity
{
	void Tick();

	/* Empty while on character select / before identity is ready. */
	const char* CharacterName();
	std::string CharacterNameStr();

	/* True once after the character name changes (including first non-empty). */
	bool TakeChanged();
}
