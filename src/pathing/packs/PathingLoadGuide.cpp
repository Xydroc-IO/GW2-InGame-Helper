#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingParse.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace PathingDetail
{

	float Dist2(float ax, float ay, float bx, float by)
	{
		const float dx = ax - bx;
		const float dy = ay - by;
		return dx * dx + dy * dy;
	}

	/* Straight orange ribbon player -> dest. Never enables pack categories. */
	void BuildDirectSearchGuideLocked()
	{
		if (!gGuideActive || !gGuideHavePlayer)
			return;

		const float destX = gGuideDestX;
		const float destY = gGuideDestY;
		const float playerX = gGuidePlayerX;
		const float playerY = gGuidePlayerY;
		if (!std::isfinite(destX) || !std::isfinite(destY) ||
			!std::isfinite(playerX) || !std::isfinite(playerY))
			return;

		PathingTrails::Trail next{};
		next.mapId = gActiveMap;
		next.color = 0xFFFFAA20u;
		std::snprintf(next.label, sizeof(next.label), "GPS | direct");

		constexpr int kSegs = 12;
		next.points.reserve(static_cast<size_t>(kSegs + 1));
		for (int i = 0; i <= kSegs; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(kSegs);
			PathingTrails::Point p{};
			p.x = playerX + (destX - playerX) * t;
			p.y = playerY + (destY - playerY) * t;
			next.points.push_back(p);
		}

		/* World meters for in-world ribbon (avatar start + rect-inverted dest). */
		float ax = 0.f, ay = 0.f, az = 0.f;
		if (G::Mumble && G::Mumble->uiTick != 0)
		{
			ax = G::Mumble->fAvatarPosition[0];
			ay = G::Mumble->fAvatarPosition[1];
			az = G::Mumble->fAvatarPosition[2];
		}
		float dxW = ax, dzW = az;
		bool haveDestWorld = false;
		/* Use cached map rects only - never HTTP under gMutex. */
		auto rit = gRects.find(gActiveMap);
		if (rit != gRects.end() && rit->second.valid)
		{
			ContinentToWorld(rit->second, destX, destY, dxW, dzW);
			haveDestWorld = std::isfinite(dxW) && std::isfinite(dzW);
		}
		if (haveDestWorld && std::isfinite(ax) && std::isfinite(az))
		{
			next.worldPoints.reserve(static_cast<size_t>(kSegs + 1));
			for (int i = 0; i <= kSegs; ++i)
			{
				const float t = static_cast<float>(i) / static_cast<float>(kSegs);
				PathingTrails::WorldPoint w{};
				w.x = ax + (dxW - ax) * t;
				w.y = ay;
				w.z = az + (dzW - az) * t;
				next.worldPoints.push_back(w);
			}
		}

		if (next.points.size() >= 2)
			gGuide = std::move(next);
	}

	void RebuildSearchGuideLocked()
	{
		if (!gGuideActive)
			return;

		const float destX = gGuideDestX;
		const float destY = gGuideDestY;
		const bool havePlayer = gGuideHavePlayer;
		const float playerX = havePlayer ? gGuidePlayerX : destX;
		const float playerY = havePlayer ? gGuidePlayerY : destY;

		/* Prefer snapping onto a loaded pack trail when one is already nearby.
		   Never enable categories - if nothing snaps, fall back to a direct line. */
		int bestTrail = -1;
		int bestPi = 0;
		int bestDi = 0;
		float bestScore = 1e30f;

		if (!gCurrentAll.empty())
		{
			/* Continent units - allow a long snap radius so WP search can latch onto
			   Tekkit / Lady Elyssa trails that don't pass exactly through the WP. */
			constexpr float kMaxDestDist2 = 6000.f * 6000.f;
			constexpr float kMaxPlayerDist2 = 8000.f * 8000.f;

			for (size_t t = 0; t < gCurrentAll.size(); ++t)
			{
				const PathingTrails::Trail& tr = gCurrentAll[t];
				if (tr.points.size() < 2)
					continue;

				int di = 0;
				int pi = 0;
				float bestD = 1e30f;
				float bestP = 1e30f;
				for (size_t i = 0; i < tr.points.size(); ++i)
				{
					if (!std::isfinite(tr.points[i].x) || !std::isfinite(tr.points[i].y))
						continue;
					const float dD = Dist2(tr.points[i].x, tr.points[i].y, destX, destY);
					if (dD < bestD)
					{
						bestD = dD;
						di = static_cast<int>(i);
					}
					if (havePlayer)
					{
						const float dP = Dist2(tr.points[i].x, tr.points[i].y, playerX, playerY);
						if (dP < bestP)
						{
							bestP = dP;
							pi = static_cast<int>(i);
						}
					}
				}
				if (bestD > kMaxDestDist2)
					continue;
				if (havePlayer && bestP > kMaxPlayerDist2)
					continue;

				/* Prefer trails that still have world samples - in-world GPS needs them. */
				float score = havePlayer ? (bestD + bestP * 0.85f) : bestD;
				if (tr.worldPoints.size() == tr.points.size() && tr.worldPoints.size() >= 2)
					score *= 0.55f;
				if (score < bestScore)
				{
					bestScore = score;
					bestTrail = static_cast<int>(t);
					bestDi = di;
					bestPi = havePlayer ? pi : 0;
				}
			}
		}

		if (bestTrail >= 0)
		{
			const PathingTrails::Trail& src = gCurrentAll[static_cast<size_t>(bestTrail)];
			int a = bestPi;
			int b = bestDi;
			if (a > b)
				std::swap(a, b);
			a = std::max(0, a - 1);
			b = std::min(static_cast<int>(src.points.size()) - 1, b + 1);
			if (b - a >= 1)
			{
				PathingTrails::Trail next{};
				next.mapId = src.mapId;
				next.color = 0xFFFFAA20u;
				std::snprintf(next.label, sizeof(next.label), "Search route | %s", src.label);
				next.points.assign(src.points.begin() + a, src.points.begin() + b + 1);
				if (src.worldPoints.size() == src.points.size())
				{
					next.worldPoints.assign(
						src.worldPoints.begin() + a, src.worldPoints.begin() + b + 1);
				}
				if (bestPi > bestDi)
				{
					std::reverse(next.points.begin(), next.points.end());
					std::reverse(next.worldPoints.begin(), next.worldPoints.end());
				}
				gGuide = std::move(next);
				return;
			}
		}

		/* No pack trail to snap - direct orange line. Does not toggle categories. */
		BuildDirectSearchGuideLocked();
	}


} // namespace PathingDetail
