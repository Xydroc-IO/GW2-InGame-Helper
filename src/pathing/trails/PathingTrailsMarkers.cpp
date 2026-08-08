#include "PathingTrails.h"

#include "PathingIndex.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using namespace PathingDetail;

namespace
{
	void FillPackLeaf(char* out, size_t outLen, const std::wstring& packPath)
	{
		if (!out || outLen == 0)
			return;
		out[0] = '\0';
		const std::string leaf = WideLeafUtf8(packPath);
		if (leaf.empty())
			return;
		std::snprintf(out, outLen, "%s", leaf.c_str());
	}
}

size_t PathingTrails::IndexedMarkerCount()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return 0;
	return gPoiIndex.size();
}

void PathingTrails::CopyIndexedMarkers(std::vector<IndexedMarkerSnapshot>& out)
{
	out.clear();
	std::vector<IndexedPoi> pois;
	std::unordered_map<std::string, MarkerStyle> styles;
	{
		std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		pois = gPoiIndex;
		styles = gCategoryStyles;
	}
	out.reserve(pois.size());
	for (const IndexedPoi& poi : pois)
	{
		if (poi.mapId == 0)
			continue;
		IndexedMarkerSnapshot s{};
		s.mapId = poi.mapId;
		s.wx = poi.wx;
		s.wy = poi.wy;
		s.wz = poi.wz;
		if (!poi.type.empty())
			std::snprintf(s.type, sizeof(s.type), "%s", poi.type.c_str());
		if (!poi.guid.empty())
			std::snprintf(s.guid, sizeof(s.guid), "%s", poi.guid.c_str());
		FillPackLeaf(s.packLeaf, sizeof(s.packLeaf), poi.packPath);
		const MarkerStyle sty = ResolveStyle(poi.type, poi.style, styles);
		if (sty.hasTipName && !sty.tipName.empty())
			std::snprintf(s.tipName, sizeof(s.tipName), "%s", sty.tipName.c_str());
		out.push_back(s);
	}
}

bool PathingTrails::TryWorldToContinentCached(uint32_t mapId, float wxMeters, float wzMeters,
	float* outCx, float* outCy)
{
	if (!outCx || !outCy || mapId == 0)
		return false;
	if (!std::isfinite(wxMeters) || !std::isfinite(wzMeters))
		return false;
	Rects rects{};
	{
		std::lock_guard<std::mutex> lock(gMutex);
		const auto it = gRects.find(mapId);
		if (it == gRects.end() || !it->second.valid)
			return false;
		rects = it->second;
	}
	float cx = 0.f, cy = 0.f;
	WorldToContinent(rects, wxMeters, wzMeters, cx, cy);
	if (!std::isfinite(cx) || !std::isfinite(cy))
		return false;
	*outCx = cx;
	*outCy = cy;
	return true;
}

bool PathingTrails::WorldToContinentForMap(uint32_t mapId, float wxMeters, float wzMeters,
	float* outCx, float* outCy)
{
	if (TryWorldToContinentCached(mapId, wxMeters, wzMeters, outCx, outCy))
		return true;
	if (!outCx || !outCy || mapId == 0)
		return false;
	if (!std::isfinite(wxMeters) || !std::isfinite(wzMeters))
		return false;

	Rects fetched{};
	if (!FetchMapRects(mapId, fetched) || !fetched.valid)
		return false;
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gRects[mapId] = fetched;
	}
	float cx = 0.f, cy = 0.f;
	WorldToContinent(fetched, wxMeters, wzMeters, cx, cy);
	if (!std::isfinite(cx) || !std::isfinite(cy))
		return false;
	*outCx = cx;
	*outCy = cy;
	return true;
}
