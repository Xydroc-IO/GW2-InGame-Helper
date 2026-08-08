#include "CompletionShared.h"

#include "Globals.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "WaypointsData.h"

#include <cstdio>
#include <vector>

namespace CompletionDetail
{
	bool GuideToObjective(size_t idx)
	{
		Objective* op = ObjectiveAt(idx);
		if (!op) return false;
		const Objective o = *op;
		if (!o.hasCoord)
		{
			std::snprintf(gStatus, sizeof(gStatus), "No coordinates for \"%s\".", o.name);
			return false;
		}
		gFocusObjective = static_cast<int>(idx);
		/* Orange search guide only - direct line or nearby pack snap.
		   Never enables pathing categories / map-completion packs. */
		PathingTrails::SetSearchDestination(o.continentX, o.continentY);
		if (PathingTrails::HasSearchGuide())
			std::snprintf(gStatus, sizeof(gStatus), "GPS trail -> %s", o.name);
		else if (PathingTrails::HasSearchGuideActive())
			std::snprintf(gStatus, sizeof(gStatus),
				"GPS -> %s (waiting for position...)", o.name);
		else
			std::snprintf(gStatus, sizeof(gStatus), "GPS -> %s", o.name);
		Settings::SetDirty();
		return true;
	}

	bool GuideNearestRemaining()
	{
		EnsureCatalog();
		float px = 0.f, py = 0.f;
		bool havePlayer = false;
		if (G::Mumble && G::Mumble->context_len >= sizeof(MumbleContext))
		{
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			px = ctx->playerX;
			py = ctx->playerY;
			havePlayer = true;
		}
		int best = -1;
		float bestD = 1.0e30f;
		const size_t n = ObjectiveCount();
		for (size_t i = 0; i < n; ++i)
		{
			Objective* op = ObjectiveAt(i);
			if (!op) continue;
			const Objective& o = *op;
			if (o.done || !o.hasCoord) continue;
			if (!IsMapCompletionRouteKind(o.kind)) continue;
			if (gFocusMapId != 0 && o.mapId != gFocusMapId) continue;
			float d = 0.f;
			if (havePlayer)
			{
				const float dx = o.continentX - px;
				const float dy = o.continentY - py;
				d = dx * dx + dy * dy;
			}
			else
				d = static_cast<float>(i);
			if (d < bestD) { bestD = d; best = static_cast<int>(i); }
		}
		if (best < 0)
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"No remaining guided objectives on this map (tick or wait for waypoint index).");
			return false;
		}
		return GuideToObjective(static_cast<size_t>(best));
	}

	bool GuideZoneLoopNext()
	{
		EnsureCatalog();
		/* Catalog order on this map only - never fall back to Nearest. */
		if (gFocusMapId == 0)
		{
			const int cur = WaypointsData::CurrentMapId();
			if (cur > 0)
				SetFocusMap(static_cast<uint32_t>(cur));
		}
		if (gFocusMapId == 0)
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Zone loop needs a focus map (open Checklist or enter a zone).");
			return false;
		}
		std::vector<size_t> idxs;
		ObjectivesForMap(gFocusMapId, idxs);
		if (idxs.empty())
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Zone loop - no objectives catalogued for this map.");
			return false;
		}
		size_t start = 0;
		if (gFocusObjective >= 0)
		{
			for (size_t i = 0; i < idxs.size(); ++i)
				if (static_cast<int>(idxs[i]) == gFocusObjective)
				{ start = i + 1; break; }
		}
		for (size_t n = 0; n < idxs.size(); ++n)
		{
			const size_t i = idxs[(start + n) % idxs.size()];
			Objective* o = ObjectiveAt(i);
			if (o && !o->done && o->hasCoord && IsMapCompletionRouteKind(o->kind))
				return GuideToObjective(i);
		}
		std::snprintf(gStatus, sizeof(gStatus), "Zone loop complete - no remaining coords.");
		return false;
	}

	bool OpenLadyMapCompletionPathing()
	{
		/* Lady Elyssa Map Completion only - not Tekkit MC presets / whole pack. */
		G::LadyBarefoot = true;
		G::LadyWithMounts = false;
		G::LadyWpOnly = false;
		G::LadyHearts = true;
		G::LadyHeroPointTrain = false;
		G::ShowPathingTrails = true;
		G::ShowWorldTrails = true;
		PathingTrails::EnableLadyMapCompletionCategories();
		PathingTrails::NotifyVisibilityFilterChanged();
		PathingGuidesPad::Open();
		std::snprintf(gStatus, sizeof(gStatus),
			"Lady Elyssa MC (Barefoot) + hearts - Pathing Features to tune.");
		Settings::SetDirty();
		return true;
	}

	bool OpenLadyAchievementPathing(const char* categoryPath)
	{
		if (!categoryPath || !categoryPath[0])
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Select an achievement category first.");
			return false;
		}
		G::ShowPathingTrails = true;
		G::ShowWorldTrails = true;
		PathingTrails::SetCategoryEnabled(categoryPath, true);
		PathingTrails::NotifyVisibilityFilterChanged();
		PathingGuidesPad::Open();
		std::snprintf(gStatus, sizeof(gStatus),
			"Pathing category on: %s", categoryPath);
		Settings::SetDirty();
		return true;
	}

	void TickAutoArrive()
	{
		if (!gAutoArrive || gFocusObjective < 0) return;
		if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext)) return;
		Objective* o = ObjectiveAt(static_cast<size_t>(gFocusObjective));
		if (!o || o->done || !o->hasCoord) return;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		const float dx = o->continentX - ctx->playerX;
		const float dy = o->continentY - ctx->playerY;
		if (dx * dx + dy * dy > gArriveRadius * gArriveRadius) return;
		char nameBuf[96];
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", o->name);
		ToggleDone(static_cast<size_t>(gFocusObjective));
		std::snprintf(gStatus, sizeof(gStatus), "Arrived - marked \"%s\" done.", nameBuf);
		if (gRouteMode == RouteMode::ZoneLoop)
			GuideZoneLoopNext();
		else if (gRouteMode == RouteMode::Nearest)
			GuideNearestRemaining();
	}

} // namespace CompletionDetail
