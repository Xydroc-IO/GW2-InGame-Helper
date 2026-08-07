/* Farming live nearest nodes — Pathing pack markers (location-live, not spawn API). */
#include "FarmingShared.h"

#include "Globals.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "WaypointsData.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <windows.h>

namespace FarmingDetail
{
	namespace
	{
		std::vector<LiveNode> gNodes;
		int gNodesRunId = -1;

		bool PlayerContinent(float& px, float& py)
		{
			if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext))
				return false;
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			px = ctx->playerX;
			py = ctx->playerY;
			return true;
		}
	}

	const std::vector<LiveNode>& LiveNodes() { return gNodes; }

	void RefreshLiveNodes(size_t run)
	{
		gNodes.clear();
		gNodesRunId = -1;
		EnsureSeed();
		if (run >= gRuns.size())
		{
			std::snprintf(gStatus, sizeof(gStatus), "No run selected.");
			return;
		}
		const Run& r = gRuns[run];
		gNodesRunId = r.id;

		/* Enable pack category so CurrentMarkers() includes gathering/fishing nodes. */
		G::ShowPathingTrails = true;
		PathingTrails::SetMasterEnabled(true);
		PathingTrails::SetCategoryEnabled("tw_guides", true);
		if (r.pathingHint[0])
			PathingTrails::SetCategoryEnabled(r.pathingHint, true);

		const int curMap = WaypointsData::CurrentMapId();
		float px = 0.f, py = 0.f;
		const bool havePlayer = PlayerContinent(px, py);

		std::vector<PathingTrails::Marker> marks = PathingTrails::CurrentMarkers();
		for (const auto& m : marks)
		{
			if (curMap > 0 && static_cast<int>(m.mapId) != curMap)
				continue;
			if (r.mapId > 0 && static_cast<int>(m.mapId) != r.mapId && curMap <= 0)
				continue;
			LiveNode n{};
			std::snprintf(n.label, sizeof(n.label), "%s",
				m.tipName[0] ? m.tipName : (m.label[0] ? m.label : "Node"));
			n.continentX = m.pos.x;
			n.continentY = m.pos.y;
			if (havePlayer)
			{
				const float dx = n.continentX - px;
				const float dy = n.continentY - py;
				n.distSq = dx * dx + dy * dy;
			}
			else
				n.distSq = static_cast<float>(gNodes.size());
			gNodes.push_back(n);
		}

		/* Fallback: map landmarks from official floor index when pack markers empty. */
		if (gNodes.empty() && curMap > 0)
		{
			WaypointsData::EnsureLoaded(false);
			WaypointsData::Tick();
			if (WaypointsData::Ready())
			{
				std::vector<WaypointsData::Poi> pois;
				WaypointsData::ListForMap(curMap, false, pois);
				for (const auto& p : pois)
				{
					if (!p.hasCoord) continue;
					if (p.type == "waypoint") continue; /* prefer landmarks/vistas as anchors */
					LiveNode n{};
					std::snprintf(n.label, sizeof(n.label), "%s",
						p.name.empty() ? p.type.c_str() : p.name.c_str());
					n.continentX = p.continentX;
					n.continentY = p.continentY;
					if (havePlayer)
					{
						const float dx = n.continentX - px;
						const float dy = n.continentY - py;
						n.distSq = dx * dx + dy * dy;
					}
					else
						n.distSq = static_cast<float>(gNodes.size());
					gNodes.push_back(n);
				}
			}
		}

		std::sort(gNodes.begin(), gNodes.end(),
			[](const LiveNode& a, const LiveNode& b) { return a.distSq < b.distSq; });
		if (gNodes.size() > 24)
			gNodes.resize(24);

		if (gNodes.empty())
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"No live nodes on this map yet — enable Pathing pack or wait for waypoint index.");
		}
		else
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"%zu live node(s) near you (Pathing / map index).", gNodes.size());
		}
		Settings::SetDirty();
	}

	bool GuideLiveNode(size_t idx)
	{
		if (idx >= gNodes.size()) return false;
		const LiveNode& n = gNodes[idx];
		PathingTrails::SetSearchDestination(n.continentX, n.continentY);
		if (PathingTrails::HasSearchGuide())
			std::snprintf(gStatus, sizeof(gStatus), "GPS trail -> %s", n.label);
		else if (PathingTrails::HasSearchGuideActive())
			std::snprintf(gStatus, sizeof(gStatus),
				"GPS -> %s (waiting for position...)", n.label);
		else
			std::snprintf(gStatus, sizeof(gStatus), "GPS -> %s", n.label);
		Settings::SetDirty();
		return true;
	}

	bool GuideNearestLiveNode()
	{
		if (gNodes.empty())
		{
			if (gSelectedRun >= 0)
				RefreshLiveNodes(static_cast<size_t>(gSelectedRun));
		}
		if (gNodes.empty())
			return false;
		return GuideLiveNode(0);
	}
}
