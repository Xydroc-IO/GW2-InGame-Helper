#pragma once

#include "PathingTrails.h"

#include <cstddef>
#include <vector>

/* Continent-space A* for GPS: pack-trail graph + official waypoints.
   No navmesh (GW2 does not expose one) — routes along authored trails and WPs. */
namespace PathingDetail
{
	/* Returns true and fills outPath (continent XY, includes start→end) on success. */
	bool FindContinentPath(
		float startX, float startY,
		float destX, float destY,
		const std::vector<PathingTrails::Trail>& trails,
		const std::vector<PathingTrails::Point>& waypoints,
		std::vector<PathingTrails::Point>& outPath,
		char* labelOut, size_t labelLen);
}
