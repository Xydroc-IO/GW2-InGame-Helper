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

	void RebuildSearchGuideLocked()
	{
		if (!gGuideActive || gCurrentAll.empty())
		{
			/* Keep any previous guide polyline — clearing here makes the orange
			   ribbon blink whenever packs are mid-reload. */
			return;
		}

		const float destX = gGuideDestX;
		const float destY = gGuideDestY;
		const bool havePlayer = gGuideHavePlayer;
		const float playerX = havePlayer ? gGuidePlayerX : destX;
		const float playerY = havePlayer ? gGuidePlayerY : destY;

		/* Continent units — allow a long snap radius so WP search can latch onto
		   Tekkit / Lady Elyssa trails that don't pass exactly through the WP. */
		constexpr float kMaxDestDist2 = 6000.f * 6000.f;
		constexpr float kMaxPlayerDist2 = 8000.f * 8000.f;

		int bestTrail = -1;
		int bestPi = 0;
		int bestDi = 0;
		float bestScore = 1e30f;

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

			/* Prefer trails that still have world samples — in-world GPS needs them. */
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

		if (bestTrail < 0)
			return; /* caller may force-reload; keep prior until a better snap exists */

		const PathingTrails::Trail& src = gCurrentAll[static_cast<size_t>(bestTrail)];
		int a = bestPi;
		int b = bestDi;
		if (a > b)
			std::swap(a, b);
		a = std::max(0, a - 1);
		b = std::min(static_cast<int>(src.points.size()) - 1, b + 1);
		if (b - a < 1)
			return;

		PathingTrails::Trail next{};
		next.mapId = src.mapId;
		next.color = 0xFFFFAA20u;
		std::snprintf(next.label, sizeof(next.label), "Search route · %s", src.label);
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
	}


} // namespace PathingDetail
