#pragma once

#include <cstddef>

/* Curated TacO packs downloaded into addons/<name>/pathing/:
     - LadyElyssa.taco / LadyElyssaAP.taco (GitHub releases/latest)
     - tw_ALL_IN_ONE.taco (Tekkit's Workshop CDN)
   User-dropped .taco files in the same folder are never deleted.
   Call EnsureCurated from a worker thread only (blocking HTTP). */

namespace PathingPacks
{
	/* Request a re-download even when local .ver stamps match. */
	void RequestForceUpdate();

	/* True while EnsureCurated is running. */
	bool IsUpdating();

	/* Last status line for the Pathing panel (may be empty). */
	void GetStatus(char* out, size_t outLen);

	/* Ensure curated packs are present / up to date under pathingDir. BLOCKING. */
	bool EnsureCurated(const wchar_t* pathingDirWide);
}
