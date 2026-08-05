#pragma once

#include <cstddef>

/* Phone sync via QR — versioned URI so we can grow into seamless sync later.
 * Format: gw2helper://sync/v1?favorites=id1,id2,...
 */

namespace SyncQr
{
	/* Build current favorites into the sync URI (out must be >= 2048). Returns false if empty/fail. */
	bool BuildFavoritesUri(char* out, size_t outLen, int* outCount);

	/* Options-panel button + modal with scannable QR. Call from UI_Options(). */
	void DrawOptionsSection();
}
