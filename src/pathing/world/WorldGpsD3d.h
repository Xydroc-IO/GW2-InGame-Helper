#pragma once

#include "PathingTrails.h"
#include "WorldGpsMath.h"

#include <vector>

/* Nexus-compliant D3D11 world-space GPS ribbons (SwapChain device, no Present hook).
   Falls back silently when device/shader init fails (Wine, lost device). */
namespace WorldGpsD3d
{
	bool Available();
	void Shutdown();

	/* Draw camera-facing world ribbons. Returns true if this path handled trails
	   (caller should skip ImGui trail billboards). Markers stay ImGui. */
	bool DrawTrails(
		const WorldGpsMath::Mat4& viewProj,
		const WorldGpsMath::Vec3& cam,
		const WorldGpsMath::Vec3& avatar,
		float maxDist,
		float thickness,
		const std::vector<PathingTrails::WorldSnippet>& trails,
		const PathingTrails::WorldSnippet* guideOrNull);
}
