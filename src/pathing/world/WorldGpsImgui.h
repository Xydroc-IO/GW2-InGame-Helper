#pragma once

#include "PathingTrails.h"
#include "WorldGpsMath.h"

#include <vector>

struct ImDrawList;

/* ImGui fallback: billboarded trail chevrons + world markers (no depth). */
namespace WorldGpsImgui
{
	int DrawTrailBillboards(
		ImDrawList* dl,
		const WorldGpsMath::Mat4& viewProj,
		float screenW, float screenH,
		const WorldGpsMath::Vec3& avatar,
		const PathingTrails::WorldSnippet& seg,
		float maxDist, float thickness, int segsLeft, bool bright);

	void DrawMarkers(
		ImDrawList* dl,
		const WorldGpsMath::Mat4& viewProj,
		float screenW, float screenH,
		const WorldGpsMath::Vec3& avatar,
		const std::vector<PathingTrails::Marker>& markers);
}
