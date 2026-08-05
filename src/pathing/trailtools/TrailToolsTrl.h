#pragma once

#include "PathingTrails.h"

#include <cstdint>
#include <string>
#include <vector>

namespace TrailToolsTrl
{
	/* Binary .trl: u32 version, u32 mapId, float3* (x,y,z Y-up). (0,0,0) = section break. */
	bool Write(const std::wstring& path, uint32_t mapId,
		const std::vector<PathingTrails::WorldPoint>& points);
	bool Read(const std::wstring& path, uint32_t& mapId,
		std::vector<PathingTrails::WorldPoint>& points);
	bool WriteUtf8(const std::string& pathUtf8, uint32_t mapId,
		const std::vector<PathingTrails::WorldPoint>& points);
}
