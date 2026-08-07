#include "PathingPathfind.h"

#include "PathingParse.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PathingDetail
{
	namespace
	{
		constexpr float kBridge = 280.f;
		constexpr float kBridge2 = kBridge * kBridge;
		constexpr float kJoin = 800.f;
		constexpr float kJoin2 = kJoin * kJoin;
		constexpr float kWpEdge = 3800.f;
		constexpr float kWpEdge2 = kWpEdge * kWpEdge;
		constexpr int   kMaxNodes = 1200;     /* keep GPS rebuild cheap under gMutex */
		constexpr int   kMaxEdgesPerNode = 12;
		constexpr int   kMaxAStarPops = 8000;
		constexpr int   kCell = 500;

		inline float D2(float ax, float ay, float bx, float by)
		{
			const float dx = ax - bx;
			const float dy = ay - by;
			return dx * dx + dy * dy;
		}

		inline float D(float ax, float ay, float bx, float by)
		{
			return std::sqrt(D2(ax, ay, bx, by));
		}

		/* Tight match — do NOT treat every "trail" label as a route corridor. */
		bool PreferTrail(const char* label)
		{
			if (!label || !label[0])
				return false;
			const std::string low = PathingParse::ToLower(label);
			if (PathingParse::LooksLikeMapCompletion(low, low))
				return true;
			return low.find("barefoot") != std::string::npos ||
				low.find("waypoint") != std::string::npos ||
				low.find("wp only") != std::string::npos ||
				low.find("wponly") != std::string::npos ||
				low.find(".route") != std::string::npos ||
				low.find("mapcompletion") != std::string::npos;
		}

		bool SkipTrail(const char* label)
		{
			if (!label || !label[0])
				return false;
			const std::string low = PathingParse::ToLower(label);
			/* Heart / HP ribbons are dense and not walk corridors. */
			return low.find("heart") != std::string::npos ||
				low.find("hp.") != std::string::npos ||
				low.find("fishing") != std::string::npos ||
				low.find("bounty") != std::string::npos;
		}

		struct Node { float x = 0.f; float y = 0.f; };
		struct Edge { int to = 0; float w = 0.f; };
		struct AStarItem
		{
			float f = 0.f;
			int idx = 0;
			bool operator>(const AStarItem& o) const { return f > o.f; }
		};

		int CellKey(float x, float y)
		{
			const int cx = static_cast<int>(std::floor(x / static_cast<float>(kCell)));
			const int cy = static_cast<int>(std::floor(y / static_cast<float>(kCell)));
			return (cx * 73856093) ^ (cy * 19349663);
		}

		void AddUndirected(std::vector<std::vector<Edge>>& adj, int a, int b, float w)
		{
			if (a == b || !(w > 0.f) || !std::isfinite(w))
				return;
			auto& aa = adj[static_cast<size_t>(a)];
			auto& bb = adj[static_cast<size_t>(b)];
			if (static_cast<int>(aa.size()) >= kMaxEdgesPerNode ||
				static_cast<int>(bb.size()) >= kMaxEdgesPerNode)
				return;
			aa.push_back({b, w});
			bb.push_back({a, w});
		}

		/* Ring of cells covering radius (in continent units). */
		int CellRing(float radius)
		{
			const int r = static_cast<int>(std::ceil(radius / static_cast<float>(kCell))) + 1;
			return (std::min)(r, 10);
		}
	}

	bool FindContinentPath(
		float startX, float startY,
		float destX, float destY,
		const std::vector<PathingTrails::Trail>& trails,
		const std::vector<PathingTrails::Point>& waypoints,
		std::vector<PathingTrails::Point>& outPath,
		char* labelOut, size_t labelLen)
	{
		outPath.clear();
		if (!std::isfinite(startX) || !std::isfinite(startY) ||
			!std::isfinite(destX) || !std::isfinite(destY))
			return false;

		const float straight = D(startX, startY, destX, destY);
		if (!(straight > 40.f))
			return false;

		std::vector<Node> nodes;
		nodes.reserve(512);
		std::vector<std::vector<Edge>> adj;
		std::unordered_map<int, std::vector<int>> cells;

		auto addNode = [&](float x, float y) -> int {
			if (!std::isfinite(x) || !std::isfinite(y))
				return -1;
			if (static_cast<int>(nodes.size()) >= kMaxNodes)
				return -1;
			const int idx = static_cast<int>(nodes.size());
			nodes.push_back({x, y});
			adj.emplace_back();
			cells[CellKey(x, y)].push_back(idx);
			return idx;
		};

		auto linkNear = [&](int idx, float maxD2, float costMul) {
			if (idx < 0)
				return;
			const Node& n = nodes[static_cast<size_t>(idx)];
			const float maxD = std::sqrt(maxD2);
			const int ring = CellRing(maxD);
			const int cx = static_cast<int>(std::floor(n.x / static_cast<float>(kCell)));
			const int cy = static_cast<int>(std::floor(n.y / static_cast<float>(kCell)));
			for (int ox = -ring; ox <= ring; ++ox)
			{
				for (int oy = -ring; oy <= ring; ++oy)
				{
					const int key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663);
					auto it = cells.find(key);
					if (it == cells.end())
						continue;
					for (int j : it->second)
					{
						if (j <= idx)
							continue;
						const float d2 = D2(n.x, n.y,
							nodes[static_cast<size_t>(j)].x,
							nodes[static_cast<size_t>(j)].y);
						if (d2 <= maxD2 && d2 > 1.f)
							AddUndirected(adj, idx, j, std::sqrt(d2) * costMul);
					}
				}
			}
		};

		int trailNodes = 0;
		for (const PathingTrails::Trail& tr : trails)
		{
			if (tr.points.size() < 2 || SkipTrail(tr.label))
				continue;
			const bool prefer = PreferTrail(tr.label);
			/* Non-preferred packs: very sparse samples or skip entirely when we
			   already have plenty of preferred corridor nodes. */
			if (!prefer && trailNodes > 200)
				continue;
			const size_t stride = prefer
				? std::max<size_t>(1, tr.points.size() / 48)
				: std::max<size_t>(4, tr.points.size() / 24);

			int prev = -1;
			for (size_t i = 0; i < tr.points.size(); i += stride)
			{
				const auto& p = tr.points[i];
				if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
					(p.x == 0.f && p.y == 0.f))
				{
					prev = -1;
					continue;
				}
				const int cur = addNode(p.x, p.y);
				if (cur < 0)
					break;
				++trailNodes;
				if (prev >= 0)
				{
					const float w = D(nodes[static_cast<size_t>(prev)].x,
						nodes[static_cast<size_t>(prev)].y, p.x, p.y);
					AddUndirected(adj, prev, cur, prefer ? w * 0.8f : w);
				}
				prev = cur;
				if (static_cast<int>(nodes.size()) >= kMaxNodes)
					break;
			}
			if (static_cast<int>(nodes.size()) >= kMaxNodes)
				break;
		}

		const int nTrail = static_cast<int>(nodes.size());
		for (int i = 0; i < nTrail; ++i)
			linkNear(i, kBridge2, 1.2f);

		std::vector<int> wpIdx;
		wpIdx.reserve(waypoints.size());
		for (const PathingTrails::Point& wp : waypoints)
		{
			if (!std::isfinite(wp.x) || !std::isfinite(wp.y))
				continue;
			const int idx = addNode(wp.x, wp.y);
			if (idx < 0)
				break;
			wpIdx.push_back(idx);
			linkNear(idx, kWpEdge2, 1.0f);
		}

		const int startIdx = addNode(startX, startY);
		const int destIdx = addNode(destX, destY);
		if (startIdx < 0 || destIdx < 0)
			return false;
		linkNear(startIdx, kJoin2, 1.3f);
		linkNear(destIdx, kJoin2, 1.3f);
		AddUndirected(adj, startIdx, destIdx, straight * 3.5f);

		const int n = static_cast<int>(nodes.size());
		if (n < 2)
			return false;

		std::vector<float> gScore(static_cast<size_t>(n), std::numeric_limits<float>::infinity());
		std::vector<int> came(static_cast<size_t>(n), -1);
		std::vector<char> closed(static_cast<size_t>(n), 0);
		std::priority_queue<AStarItem, std::vector<AStarItem>, std::greater<AStarItem>> open;

		gScore[static_cast<size_t>(startIdx)] = 0.f;
		open.push({straight, startIdx});

		bool found = false;
		int pops = 0;
		while (!open.empty() && pops < kMaxAStarPops)
		{
			const AStarItem cur = open.top();
			open.pop();
			++pops;
			if (closed[static_cast<size_t>(cur.idx)])
				continue;
			closed[static_cast<size_t>(cur.idx)] = 1;
			if (cur.idx == destIdx)
			{
				found = true;
				break;
			}

			for (const Edge& e : adj[static_cast<size_t>(cur.idx)])
			{
				if (closed[static_cast<size_t>(e.to)])
					continue;
				const float ng = gScore[static_cast<size_t>(cur.idx)] + e.w;
				if (ng < gScore[static_cast<size_t>(e.to)])
				{
					came[static_cast<size_t>(e.to)] = cur.idx;
					gScore[static_cast<size_t>(e.to)] = ng;
					const float h = D(
						nodes[static_cast<size_t>(e.to)].x,
						nodes[static_cast<size_t>(e.to)].y,
						destX, destY);
					open.push({ng + h, e.to});
				}
			}
		}

		if (!found || !std::isfinite(gScore[static_cast<size_t>(destIdx)]))
			return false;

		const float pathLen = gScore[static_cast<size_t>(destIdx)];
		std::vector<int> order;
		for (int c = destIdx; c >= 0; c = came[static_cast<size_t>(c)])
		{
			order.push_back(c);
			if (c == startIdx)
				break;
			if (order.size() > static_cast<size_t>(n) + 2)
				return false; /* cycle guard */
		}
		std::reverse(order.begin(), order.end());
		if (order.size() < 3 || order.front() != startIdx)
			return false;
		if (pathLen > straight * 2.8f)
			return false;

		outPath.reserve(order.size());
		for (int idx : order)
		{
			PathingTrails::Point p{};
			p.x = nodes[static_cast<size_t>(idx)].x;
			p.y = nodes[static_cast<size_t>(idx)].y;
			outPath.push_back(p);
		}

		if (labelOut && labelLen)
		{
			const bool usedTrail = trailNodes > 0 && order.size() > 3;
			const bool usedWp = !wpIdx.empty();
			if (usedTrail && usedWp)
				std::snprintf(labelOut, labelLen, "GPS | pathfind (trails+WPs)");
			else if (usedTrail)
				std::snprintf(labelOut, labelLen, "GPS | pathfind (trails)");
			else if (usedWp)
				std::snprintf(labelOut, labelLen, "GPS | pathfind (waypoints)");
			else
				std::snprintf(labelOut, labelLen, "GPS | pathfind");
		}
		return outPath.size() >= 3;
	}
}
