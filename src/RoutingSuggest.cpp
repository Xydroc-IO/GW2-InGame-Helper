#include "RoutingSuggest.h"

#include "ConfirmedWaypoints.h"
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

	std::string ReadUtf8Clipboard()
	{
		if (!OpenClipboard(nullptr))
			return {};
		HANDLE h = GetClipboardData(CF_TEXT);
		std::string out;
		if (h)
		{
			const char* p = static_cast<const char*>(GlobalLock(h));
			if (p)
			{
				out = p;
				GlobalUnlock(h);
			}
		}
		CloseClipboard();
		return out;
	}

	std::string ExtractChatLink(const std::string& text)
	{
		const size_t a = text.find("[&");
		if (a == std::string::npos)
			return {};
		const size_t b = text.find(']', a);
		if (b == std::string::npos || b <= a + 2)
			return {};
		return text.substr(a, b - a + 1);
	}
}

RoutingSuggest::Result RoutingSuggest::SuggestNearTrailStart(size_t maxN)
{
	Result r;
	if (maxN == 0)
		maxN = 3;

	uint32_t mapId = 0;
	float playerX = 0.f;
	float playerY = 0.f;
	bool havePlayer = false;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
		{
			mapId = ctx->mapId;
			if (std::isfinite(ctx->playerX) && std::isfinite(ctx->playerY))
			{
				playerX = ctx->playerX;
				playerY = ctx->playerY;
				havePlayer = true;
			}
		}
	}
	if (!mapId)
	{
		r.status = "MumbleLink map unavailable.";
		gLast = r;
		return r;
	}

	/* Packs optional: with no categories enabled, rank against the player.
	   With packs on, prefer trail start when one is available. */
	TekkitTrails::Update(mapId);

	char label[96]{};
	bool fromPlayer = false;
	const bool anyEnabled = !TekkitTrails::EnabledPaths().empty();
	if (anyEnabled &&
		TekkitTrails::TryTrailStartContinent(&r.trailX, &r.trailY, label, sizeof(label), true))
	{
		/* trail start locked in */
	}
	else if (havePlayer)
	{
		r.trailX = playerX;
		r.trailY = playerY;
		std::snprintf(label, sizeof(label), "your position");
		fromPlayer = true;
	}
	else if (TekkitTrails::TryTrailStartContinent(&r.trailX, &r.trailY, label, sizeof(label), false))
	{
		/* any loaded trail (search-rank) while Mumble player coords missing */
	}
	else
	{
		r.status = TekkitTrails::IsLoading()
			? "Trail packs still loading — retry in a moment, or wait for MumbleLink."
			: "MumbleLink position unavailable — open the map in-game and retry.";
		gLast = r;
		return r;
	}
	std::snprintf(r.trailLabel, sizeof(r.trailLabel), "%s", label);

	WaypointsData::EnsureLoaded(false);
	WaypointsData::Tick();
	if (!WaypointsData::Ready())
	{
		r.status = WaypointsData::Busy()
			? std::string(WaypointsData::Status())
			: "Waypoint index not ready — retry in a moment.";
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
		bool confirmed = false;
	};
	std::vector<Ranked> ranked;
	ranked.reserve(pois.size());
	const bool prefer = ConfirmedWaypoints::PreferConfirmed();
	size_t confirmedN = 0;
	for (WaypointsData::Poi& p : pois)
	{
		if (!p.hasCoord)
			continue;
		const float dx = p.continentX - r.trailX;
		const float dy = p.continentY - r.trailY;
		Ranked row;
		row.confirmed = ConfirmedWaypoints::IsConfirmed(p.id);
		if (row.confirmed)
			++confirmedN;
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

	if (prefer && confirmedN > 0)
	{
		std::vector<Ranked> only;
		only.reserve(confirmedN);
		for (Ranked& row : ranked)
		{
			if (row.confirmed)
				only.push_back(std::move(row));
		}
		ranked = std::move(only);
	}

	std::sort(ranked.begin(), ranked.end(),
		[](const Ranked& a, const Ranked& b) {
			if (a.confirmed != b.confirmed)
				return a.confirmed && !b.confirmed;
			return a.dist < b.dist;
		});
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
		c.confirmed = row.confirmed;
		r.nearest.push_back(std::move(c));
	}

	r.ok = !r.nearest.empty();
	if (r.ok)
	{
		if (prefer && confirmedN > 0)
			r.status = "Nearest walk-confirmed waypoints — copy a chat code to teleport.";
		else if (fromPlayer)
			r.status = anyEnabled
				? "No trail start on this map — nearest waypoints to your position."
				: "Nearest waypoints to your position (no pathing categories enabled).";
		else
			r.status = "Nearest waypoints to trail start — copy a chat code to teleport.";

		/* Orange guide: toward trail start, or toward the closest WP when anchored on the player. */
		if (fromPlayer && r.nearest[0].hasCoord)
		{
			Result guide = r;
			guide.trailX = r.nearest[0].continentX;
			guide.trailY = r.nearest[0].continentY;
			std::snprintf(guide.trailLabel, sizeof(guide.trailLabel), "%s",
				r.nearest[0].name.c_str());
			ApplyOrangeGuide(guide);
		}
		else
			ApplyOrangeGuide(r);
	}
	gLast = r;
	return r;
}

RoutingSuggest::Result RoutingSuggest::SuggestFromClipboard()
{
	Result r;
	const std::string clip = ReadUtf8Clipboard();
	const std::string link = ExtractChatLink(clip);
	if (link.empty())
	{
		r.status = "Clipboard has no GW2 chat link [&…]. Shift+click a waypoint in-game, then retry.";
		gLast = r;
		return r;
	}

	WaypointsData::EnsureLoaded(false);
	WaypointsData::Tick();
	if (!WaypointsData::Ready())
	{
		r.status = WaypointsData::Busy()
			? std::string(WaypointsData::Status())
			: "Waypoint index not ready — retry in a moment.";
		gLast = r;
		return r;
	}

	WaypointsData::Poi poi;
	if (!WaypointsData::FindByChatLink(link.c_str(), poi))
	{
		r.status = "Chat link not in the public waypoint index: " + link;
		gLast = r;
		return r;
	}
	if (!poi.hasCoord)
	{
		r.status = "Matched " + poi.name + " but it has no coordinates.";
		gLast = r;
		return r;
	}

	r.trailX = poi.continentX;
	r.trailY = poi.continentY;
	std::snprintf(r.trailLabel, sizeof(r.trailLabel), "%s", poi.name.c_str());
	Candidate c;
	c.name = poi.name;
	c.chatLink = poi.chatLink;
	c.continentX = poi.continentX;
	c.continentY = poi.continentY;
	c.hasCoord = true;
	c.confirmed = ConfirmedWaypoints::IsConfirmed(poi.id);
	c.dist = 0.f;
	r.nearest.push_back(std::move(c));
	r.ok = true;
	r.status = std::string("Routing to ") + poi.name + " (" + poi.type + ") — orange guide set.";
	ApplyOrangeGuide(r);
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
