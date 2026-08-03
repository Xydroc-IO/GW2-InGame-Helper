#include "RoutingSuggest.h"

#include "Globals.h"
#include "TekkitTrails.h"
#include "WaypointsData.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	RoutingSuggest::Result gLast;

	bool CopyUtf8Clipboard(const char* text)
	{
		if (!text || !text[0])
			return false;
		if (!OpenClipboard(nullptr))
			return false;
		EmptyClipboard();
		const SIZE_T bytes = std::strlen(text) + 1;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!mem)
		{
			CloseClipboard();
			return false;
		}
		void* locked = GlobalLock(mem);
		if (!locked)
		{
			GlobalFree(mem);
			CloseClipboard();
			return false;
		}
		std::memcpy(locked, text, bytes);
		GlobalUnlock(mem);
		SetClipboardData(CF_TEXT, mem);
		CloseClipboard();
		return true;
	}
}

RoutingSuggest::Result RoutingSuggest::SuggestNearTrailStart(size_t maxN)
{
	Result r;
	if (maxN == 0)
		maxN = 3;

	uint32_t mapId = 0;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
			mapId = ctx->mapId;
	}
	if (!mapId)
	{
		r.status = "MumbleLink map unavailable.";
		gLast = r;
		return r;
	}

	TekkitTrails::Update(mapId);

	char label[96]{};
	if (!TekkitTrails::TryTrailStartContinent(&r.trailX, &r.trailY, label, sizeof(label), true))
	{
		r.status = "No trail points on this map — enable a category or wait for packs.";
		gLast = r;
		return r;
	}
	std::snprintf(r.trailLabel, sizeof(r.trailLabel), "%s", label);

	WaypointsData::EnsureLoaded(false);
	if (!WaypointsData::Ready())
	{
		r.status = WaypointsData::Busy()
			? "Loading waypoint index…"
			: "Waypoint index not ready — open Notes → Waypoints or retry.";
		gLast = r;
		return r;
	}

	std::vector<WaypointsData::Poi> pois;
	WaypointsData::ListForMap(static_cast<int>(mapId), true, pois);
	if (pois.empty())
	{
		r.status = "No waypoints indexed for this map.";
		gLast = r;
		return r;
	}

	struct Ranked
	{
		WaypointsData::Poi poi;
		float dist = 0.f;
	};
	std::vector<Ranked> ranked;
	ranked.reserve(pois.size());
	for (WaypointsData::Poi& p : pois)
	{
		if (!p.hasCoord)
			continue;
		const float dx = p.continentX - r.trailX;
		const float dy = p.continentY - r.trailY;
		Ranked row;
		row.poi = std::move(p);
		row.dist = std::sqrt(dx * dx + dy * dy);
		ranked.push_back(std::move(row));
	}
	if (ranked.empty())
	{
		r.status = "Waypoints on this map have no coordinates yet — refresh the index.";
		gLast = r;
		return r;
	}

	std::sort(ranked.begin(), ranked.end(),
		[](const Ranked& a, const Ranked& b) { return a.dist < b.dist; });
	if (ranked.size() > maxN)
		ranked.resize(maxN);

	r.nearest.reserve(ranked.size());
	for (Ranked& row : ranked)
	{
		Candidate c;
		c.name = std::move(row.poi.name);
		c.chatLink = std::move(row.poi.chatLink);
		c.continentX = row.poi.continentX;
		c.continentY = row.poi.continentY;
		c.dist = row.dist;
		c.hasCoord = true;
		r.nearest.push_back(std::move(c));
	}

	r.ok = !r.nearest.empty();
	if (r.ok)
	{
		r.status = "Nearest waypoints to trail start — copy a chat code to teleport.";
		ApplyOrangeGuide(r);
	}
	gLast = r;
	return r;
}

void RoutingSuggest::ApplyOrangeGuide(const Result& r)
{
	if (!r.trailLabel[0] && r.trailX == 0.f && r.trailY == 0.f)
		return;
	TekkitTrails::SetSearchDestination(r.trailX, r.trailY);
}

void RoutingSuggest::ClearGuide()
{
	TekkitTrails::ClearSearchGuide();
}

bool RoutingSuggest::CopyChatLink(const char* chatLink)
{
	return CopyUtf8Clipboard(chatLink);
}

const RoutingSuggest::Result& RoutingSuggest::Last()
{
	return gLast;
}

void RoutingSuggest::SetLast(Result r)
{
	gLast = std::move(r);
}
