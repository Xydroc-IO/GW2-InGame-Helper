#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingParse.h"
#include "PathingPathfind.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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

	namespace
	{
		constexpr uint32_t kGuideColor = 0xFFFFAA20u;
		constexpr float kGuideTrailScale = 1.45f;

		void DensifyContinent(std::vector<PathingTrails::Point>& pts, float maxStep)
		{
			if (pts.size() < 2 || !(maxStep > 1.f))
				return;
			std::vector<PathingTrails::Point> out;
			out.reserve(pts.size() * 4);
			out.push_back(pts.front());
			for (size_t i = 1; i < pts.size(); ++i)
			{
				const PathingTrails::Point& a = out.back();
				const PathingTrails::Point& b = pts[i];
				if (!std::isfinite(b.x) || !std::isfinite(b.y))
					continue;
				const float d = std::sqrt(Dist2(a.x, a.y, b.x, b.y));
				if (!(d > 1e-3f))
					continue;
				const int n = static_cast<int>(std::ceil(d / maxStep));
				const int segs = (std::max)(1, (std::min)(n, 64));
				for (int s = 1; s <= segs; ++s)
				{
					const float t = static_cast<float>(s) / static_cast<float>(segs);
					PathingTrails::Point p{};
					p.x = a.x + (b.x - a.x) * t;
					p.y = a.y + (b.y - a.y) * t;
					out.push_back(p);
				}
			}
			pts.swap(out);
		}

		void SoftenContinent(std::vector<PathingTrails::Point>& pts)
		{
			if (pts.size() < 3)
				return;
			std::vector<PathingTrails::Point> out;
			out.reserve(pts.size() * 2);
			out.push_back(pts.front());
			for (size_t i = 0; i + 1 < pts.size(); ++i)
			{
				const PathingTrails::Point& a = pts[i];
				const PathingTrails::Point& b = pts[i + 1];
				PathingTrails::Point q{};
				q.x = 0.75f * a.x + 0.25f * b.x;
				q.y = 0.75f * a.y + 0.25f * b.y;
				PathingTrails::Point r{};
				r.x = 0.25f * a.x + 0.75f * b.x;
				r.y = 0.25f * a.y + 0.75f * b.y;
				out.push_back(q);
				out.push_back(r);
			}
			out.push_back(pts.back());
			pts.swap(out);
		}

		void FillWorldFromContinent(PathingTrails::Trail& trail)
		{
			trail.worldPoints.clear();
			float ax = 0.f, ay = 0.f, az = 0.f;
			if (G::Mumble && G::Mumble->uiTick != 0)
			{
				ax = G::Mumble->fAvatarPosition[0];
				ay = G::Mumble->fAvatarPosition[1];
				az = G::Mumble->fAvatarPosition[2];
			}
			auto rit = gRects.find(gActiveMap);
			if (rit == gRects.end() || !rit->second.valid ||
				!std::isfinite(ax) || !std::isfinite(az) || trail.points.size() < 2)
				return;

			trail.worldPoints.reserve(trail.points.size());
			for (const PathingTrails::Point& p : trail.points)
			{
				float wx = ax, wz = az;
				ContinentToWorld(rit->second, p.x, p.y, wx, wz);
				if (!std::isfinite(wx) || !std::isfinite(wz))
					continue;
				PathingTrails::WorldPoint w{};
				w.x = wx;
				w.y = ay;
				w.z = wz;
				trail.worldPoints.push_back(w);
			}
		}

		void FinalizeGuide(PathingTrails::Trail& next, const char* label)
		{
			next.mapId = gActiveMap;
			next.color = kGuideColor;
			next.trailScale = kGuideTrailScale;
			next.alpha = 1.f;
			std::snprintf(next.label, sizeof(next.label), "%s", label);
			DensifyContinent(next.points, 160.f);
			SoftenContinent(next.points);
			DensifyContinent(next.points, 110.f);
			FillWorldFromContinent(next);
			if (next.points.size() >= 2)
				gGuide = std::move(next);
		}
	}

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
		next.points.push_back({playerX, playerY});
		next.points.push_back({destX, destY});
		FinalizeGuide(next, "GPS | direct");
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
		if (!havePlayer || !std::isfinite(playerX) || !std::isfinite(destX))
			return;

		/* 1) A* over pack trails + official waypoints. */
		{
			std::vector<PathingTrails::Point> path;
			char label[96]{};
			if (FindContinentPath(playerX, playerY, destX, destY,
					gCurrentAll, gGuideWpCache, path, label, sizeof(label)) &&
				path.size() >= 3)
			{
				PathingTrails::Trail next{};
				next.points = std::move(path);
				FinalizeGuide(next, label[0] ? label : "GPS | pathfind");
				if (gGuide.points.size() >= 2)
					return;
			}
		}

		/* 2) Legacy single-trail snap (when graph is sparse but one trail is close). */
		int bestTrail = -1;
		int bestPi = 0;
		int bestDi = 0;
		float bestScore = 1e30f;
		if (!gCurrentAll.empty())
		{
			constexpr float kMaxDestDist2 = 6000.f * 6000.f;
			constexpr float kMaxPlayerDist2 = 8000.f * 8000.f;
			for (size_t t = 0; t < gCurrentAll.size(); ++t)
			{
				const PathingTrails::Trail& tr = gCurrentAll[t];
				if (tr.points.size() < 2)
					continue;
				int di = 0, pi = 0;
				float bestD = 1e30f, bestP = 1e30f;
				for (size_t i = 0; i < tr.points.size(); ++i)
				{
					if (!std::isfinite(tr.points[i].x) || !std::isfinite(tr.points[i].y))
						continue;
					const float dD = Dist2(tr.points[i].x, tr.points[i].y, destX, destY);
					if (dD < bestD) { bestD = dD; di = static_cast<int>(i); }
					const float dP = Dist2(tr.points[i].x, tr.points[i].y, playerX, playerY);
					if (dP < bestP) { bestP = dP; pi = static_cast<int>(i); }
				}
				if (bestD > kMaxDestDist2 || bestP > kMaxPlayerDist2)
					continue;
				float score = bestD + bestP * 0.85f;
				if (tr.worldPoints.size() == tr.points.size() && tr.worldPoints.size() >= 2)
					score *= 0.55f;
				if (score < bestScore)
				{
					bestScore = score;
					bestTrail = static_cast<int>(t);
					bestDi = di;
					bestPi = pi;
				}
			}
		}

		if (bestTrail >= 0)
		{
			const PathingTrails::Trail& src = gCurrentAll[static_cast<size_t>(bestTrail)];
			int a = bestPi, b = bestDi;
			const bool reverse = bestPi > bestDi;
			if (a > b) std::swap(a, b);
			a = std::max(0, a - 1);
			b = std::min(static_cast<int>(src.points.size()) - 1, b + 1);
			if (b - a >= 1)
			{
				PathingTrails::Trail next{};
				if (src.textureId[0])
					std::snprintf(next.textureId, sizeof(next.textureId), "%s", src.textureId);
				next.points.assign(src.points.begin() + a, src.points.begin() + b + 1);
				if (reverse)
					std::reverse(next.points.begin(), next.points.end());
				constexpr float kJoin = 140.f;
				if (!next.points.empty() &&
					Dist2(playerX, playerY, next.points.front().x, next.points.front().y) >
						(kJoin * kJoin))
					next.points.insert(next.points.begin(), {playerX, playerY});
				if (!next.points.empty() &&
					Dist2(next.points.back().x, next.points.back().y, destX, destY) >
						(kJoin * kJoin))
					next.points.push_back({destX, destY});
				char lab[96];
				std::snprintf(lab, sizeof(lab), "GPS | %s", src.label);
				FinalizeGuide(next, lab);
				if (gGuide.points.size() >= 2)
					return;
			}
		}

		BuildDirectSearchGuideLocked();
	}

} // namespace PathingDetail
