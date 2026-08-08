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

		/* Armed by GPS nearest / GPS on a live node — no custom trails required. */
		bool gArriveArmed = false;
		float gArriveX = 0.f;
		float gArriveY = 0.f;
		char gArriveLabel[96]{};
		float gLastArriveX = 0.f;
		float gLastArriveY = 0.f;
		bool gHaveLastArrive = false;

		bool PlayerContinent(float& px, float& py)
		{
			if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext))
				return false;
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			px = ctx->playerX;
			py = ctx->playerY;
			return true;
		}

		void ArmArrive(float x, float y, const char* label)
		{
			gArriveArmed = true;
			gArriveX = x;
			gArriveY = y;
			std::snprintf(gArriveLabel, sizeof(gArriveLabel), "%s", label ? label : "node");
		}

		void ClearArrive()
		{
			gArriveArmed = false;
			gArriveLabel[0] = 0;
		}

		bool TooCloseToLast(float x, float y, float minDist)
		{
			if (!gHaveLastArrive) return false;
			const float dx = x - gLastArriveX;
			const float dy = y - gLastArriveY;
			return dx * dx + dy * dy < minDist * minDist;
		}

		bool GuideLiveNodeSkippingLast(float skipDist)
		{
			for (size_t i = 0; i < gNodes.size(); ++i)
			{
				if (TooCloseToLast(gNodes[i].continentX, gNodes[i].continentY, skipDist))
					continue;
				return GuideLiveNode(i);
			}
			ClearArrive();
			return false;
		}
	}

	const std::vector<LiveNode>& LiveNodes() { return gNodes; }

	void RefreshLiveNodes(size_t run)
	{
		gNodes.clear();
		gNodesRunId = -1;
		EnsureCatalog();
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
					if (p.type == "waypoint") continue;
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
		ArmArrive(n.continentX, n.continentY, n.label);
		if (gFocusStep < 0 && gSelectedRun >= 0)
			gFocusStep = NextUndoneStep(static_cast<size_t>(gSelectedRun));
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
		return GuideLiveNodeSkippingLast(gArriveRadius * 0.5f);
	}

	bool GuideStep(size_t run, size_t step)
	{
		EnsureCatalog();
		if (run >= gRuns.size() || step >= gRuns[run].steps.size()) return false;
		const RunStep& s = gRuns[run].steps[step];
		gFocusStep = static_cast<int>(step);
		if (!s.hasCoord)
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Step has no GPS coord — use live nodes or Pathing.");
			return false;
		}
		PathingTrails::SetSearchDestination(s.continentX, s.continentY);
		ArmArrive(s.continentX, s.continentY, s.text);
		std::snprintf(gStatus, sizeof(gStatus), "GPS -> %s", s.text);
		Settings::SetDirty();
		return true;
	}

	bool GuideNextStep(size_t run)
	{
		const int next = NextUndoneStep(run);
		if (next < 0)
		{
			std::snprintf(gStatus, sizeof(gStatus), "All steps done.");
			ClearArrive();
			return false;
		}
		gFocusStep = next;
		if (GuideStep(run, static_cast<size_t>(next)))
			return true;
		/* No step coords — Pathing pack / landmark nearest (no custom trails). */
		return GuideNearestLiveNode();
	}

	void TickAutoArrive()
	{
		if (!gAutoArrive || !G::ShowFarming || !gArriveArmed) return;
		if (gSelectedRun < 0 || static_cast<size_t>(gSelectedRun) >= gRuns.size()) return;

		float px = 0.f, py = 0.f;
		if (!PlayerContinent(px, py)) return;
		const float dx = gArriveX - px;
		const float dy = gArriveY - py;
		if (dx * dx + dy * dy > gArriveRadius * gArriveRadius) return;

		const int step = (gFocusStep >= 0) ? gFocusStep
			: NextUndoneStep(static_cast<size_t>(gSelectedRun));
		if (step < 0)
		{
			ClearArrive();
			std::snprintf(gStatus, sizeof(gStatus), "Arrived at %s — run complete.", gArriveLabel);
			return;
		}

		Run& r = gRuns[static_cast<size_t>(gSelectedRun)];
		if (static_cast<size_t>(step) >= r.steps.size() || r.steps[static_cast<size_t>(step)].done)
		{
			gFocusStep = NextUndoneStep(static_cast<size_t>(gSelectedRun));
			if (gFocusStep < 0)
			{
				ClearArrive();
				return;
			}
		}

		char stepBuf[120];
		const int check = (gFocusStep >= 0) ? gFocusStep : step;
		std::snprintf(stepBuf, sizeof(stepBuf), "%s",
			r.steps[static_cast<size_t>(check)].text);
		char nodeBuf[96];
		std::snprintf(nodeBuf, sizeof(nodeBuf), "%s", gArriveLabel);

		gLastArriveX = gArriveX;
		gLastArriveY = gArriveY;
		gHaveLastArrive = true;
		ClearArrive();

		if (!r.steps[static_cast<size_t>(check)].done)
			ToggleStep(static_cast<size_t>(gSelectedRun), static_cast<size_t>(check));
		gFocusStep = NextUndoneStep(static_cast<size_t>(gSelectedRun));

		RefreshLiveNodes(static_cast<size_t>(gSelectedRun));
		const float skip = gArriveRadius * 1.5f;
		if (gFocusStep >= 0 && GuideLiveNodeSkippingLast(skip))
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Arrived %s — checked \"%s\". GPS -> next marker.", nodeBuf, stepBuf);
		}
		else
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Arrived %s — checked \"%s\".", nodeBuf, stepBuf);
		}
	}
}
