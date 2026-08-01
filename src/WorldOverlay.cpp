#include "WorldOverlay.h"

#include "Globals.h"
#include "TekkitTrails.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

/* Tekkit in-world GPS: pack textures as ribbons, with a solid line always under
   them so a missing/broken texture never blanks the path. */

namespace
{
	constexpr float kNearClip = 0.5f;
	constexpr float kFarClip = 8000.f;
	constexpr float kDefaultFov = 1.222f;
	constexpr int kMaxSegments = 800;
	/* No artificial lift — Pathing/TacO draw .trl points at authored Y.
	   A height bias looks like a lateral miss once the camera pitches. */
	constexpr float kHeightBias = 0.f;
	constexpr float kInchesToMeters = 1.f / 39.3700787f;

	struct Vec3
	{
		float x = 0.f, y = 0.f, z = 0.f;
		Vec3() = default;
		Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
		float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
		Vec3 Cross(const Vec3& o) const
		{
			return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
		}
		float LengthSq() const { return x * x + y * y + z * z; }
		Vec3 Normalised() const
		{
			const float l = std::sqrt(LengthSq());
			return l > 1e-6f ? Vec3{x / l, y / l, z / l} : Vec3{};
		}
	};

	struct Mat4
	{
		float m[4][4]{};
		void Transform(float ix, float iy, float iz,
			float& ox, float& oy, float& oz, float& ow) const
		{
			ox = m[0][0] * ix + m[1][0] * iy + m[2][0] * iz + m[3][0];
			oy = m[0][1] * ix + m[1][1] * iy + m[2][1] * iz + m[3][1];
			oz = m[0][2] * ix + m[1][2] * iy + m[2][2] * iz + m[3][2];
			ow = m[0][3] * ix + m[1][3] * iy + m[2][3] * iz + m[3][3];
		}
		Mat4 operator*(const Mat4& b) const
		{
			Mat4 r{};
			for (int c = 0; c < 4; ++c)
				for (int row = 0; row < 4; ++row)
					for (int k = 0; k < 4; ++k)
						r.m[c][row] += m[k][row] * b.m[c][k];
			return r;
		}
	};

	bool Finite3(float x, float y, float z)
	{
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
	}

	bool ReasonablePos(float x, float y, float z)
	{
		return Finite3(x, y, z) &&
			std::fabs(x) < 1.0e6f && std::fabs(y) < 1.0e6f && std::fabs(z) < 1.0e6f;
	}

	float ParseFovRadians()
	{
		if (!G::Mumble)
			return kDefaultFov;
		/* Identity is a JSON wchar string; FOV is vertical radians. */
		char id[260]{};
		const wchar_t* w = G::Mumble->identity;
		size_t n = 0;
		for (; n < 255 && w[n]; ++n)
			id[n] = (w[n] < 128) ? static_cast<char>(w[n]) : ' ';
		id[n] = 0;
		const char* p = std::strstr(id, "\"fov\"");
		if (!p)
			p = std::strstr(id, "\"FOV\"");
		if (!p)
			return kDefaultFov;
		p = std::strchr(p, ':');
		if (!p)
			return kDefaultFov;
		const float fov = static_cast<float>(std::atof(p + 1));
		return (std::isfinite(fov) && fov > 0.2f && fov < 3.f) ? fov : kDefaultFov;
	}

	bool BuildViewProj(float screenW, float screenH, Mat4& out, Vec3& camOut)
	{
		const float* cp = G::Mumble->fCameraPosition;
		const float* cf = G::Mumble->fCameraFront;
		const float* ct = G::Mumble->fCameraTop;
		if (!ReasonablePos(cp[0], cp[1], cp[2]))
			return false;
		if (!Finite3(cf[0], cf[1], cf[2]) || !Finite3(ct[0], ct[1], ct[2]))
			return false;

		Vec3 camPos{cp[0], cp[1], cp[2]};
		Vec3 f = Vec3{cf[0], cf[1], cf[2]}.Normalised();
		if (f.LengthSq() < 0.5f)
			return false;
		/* Prefer camera-top; fall back to world +Y. Match Nexus Pathing basis. */
		Vec3 topHint{ct[0], ct[1], ct[2]};
		Vec3 worldUp = (topHint.LengthSq() > 0.01f) ? topHint.Normalised()
			: Vec3{0.f, 1.f, 0.f};
		/* If looking nearly straight up/down, cameraTop is unstable — use world Y. */
		if (std::fabs(f.Dot(worldUp)) > 0.98f)
			worldUp = Vec3{0.f, 1.f, 0.f};
		Vec3 r = worldUp.Cross(f).Normalised();
		if (r.LengthSq() < 0.5f)
		{
			worldUp = Vec3{0.f, 1.f, 0.f};
			r = worldUp.Cross(f).Normalised();
		}
		if (r.LengthSq() < 0.5f)
			return false;
		Vec3 u = f.Cross(r).Normalised();
		if (u.LengthSq() < 0.5f)
			return false;

		Mat4 view{};
		view.m[0][0] = r.x; view.m[1][0] = r.y; view.m[2][0] = r.z;
		view.m[3][0] = -r.Dot(camPos);
		view.m[0][1] = u.x; view.m[1][1] = u.y; view.m[2][1] = u.z;
		view.m[3][1] = -u.Dot(camPos);
		view.m[0][2] = f.x; view.m[1][2] = f.y; view.m[2][2] = f.z;
		view.m[3][2] = -f.Dot(camPos);
		view.m[3][3] = 1.f;

		const float fov = ParseFovRadians();
		const float aspect = (screenH > 0.f) ? screenW / screenH : 1.7778f;
		const float tanHalfFov = std::tan(fov * 0.5f);
		if (!std::isfinite(tanHalfFov) || tanHalfFov < 1e-4f)
			return false;

		Mat4 proj{};
		proj.m[0][0] = 1.f / (aspect * tanHalfFov);
		proj.m[1][1] = 1.f / tanHalfFov;
		proj.m[2][2] = kFarClip / (kFarClip - kNearClip);
		proj.m[2][3] = 1.f;
		proj.m[3][2] = -(kNearClip * kFarClip) / (kFarClip - kNearClip);

		out = proj * view;
		camOut = camPos;
		return true;
	}

	bool WorldToScreen(const Vec3& world, const Mat4& viewProj,
		float screenW, float screenH, float& sx, float& sy)
	{
		if (!ReasonablePos(world.x, world.y, world.z))
			return false;
		float cx, cy, cz, cw;
		viewProj.Transform(world.x, world.y, world.z, cx, cy, cz, cw);
		if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cw) || cw <= 0.08f)
			return false;
		const float ndcX = cx / cw;
		const float ndcY = cy / cw;
		if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
			return false;
		if (ndcX < -2.5f || ndcX > 2.5f || ndcY < -2.5f || ndcY > 2.5f)
			return false;
		sx = (ndcX + 1.f) * 0.5f * screenW;
		sy = (-ndcY + 1.f) * 0.5f * screenH;
		return std::isfinite(sx) && std::isfinite(sy);
	}

	void TrailFadeRange(const TekkitTrails::WorldSnippet& /*seg*/, float maxDist,
		float& fadeStart, float& fadeEnd)
	{
		/* Soft edge — match the longer along-trail GPS window. */
		fadeStart = maxDist * 0.75f;
		fadeEnd = maxDist * 1.05f;
		fadeEnd = std::max(fadeStart + 1.f, fadeEnd);
	}

	int DrawTrailRibbon(ImDrawList* dl, const Mat4& viewProj,
		float screenW, float screenH,
		const Vec3& avatar, const TekkitTrails::WorldSnippet& seg,
		float maxDist, float thickness, int segsLeft, bool bright)
	{
		if (!dl || seg.points.size() < 2 || segsLeft <= 0)
			return 0;

		int rr = static_cast<int>((seg.color >> 16) & 0xFFu);
		int gg = static_cast<int>((seg.color >> 8) & 0xFFu);
		int bb = static_cast<int>(seg.color & 0xFFu);
		if (rr > 245 && gg > 245 && bb > 245)
		{
			rr = 0;
			gg = 220;
			bb = 255;
		}
		const float baseA = (bright ? 0.98f : 0.92f) *
			std::clamp(seg.alpha > 0.05f ? seg.alpha : 1.f, 0.f, 1.f);

		float fadeStart = 0.f, fadeEnd = 0.f;
		TrailFadeRange(seg, maxDist, fadeStart, fadeEnd);
		const float fadeEnd2 = fadeEnd * fadeEnd;

		/* Blish Pathing: TRAIL_WIDTH = 20 inches → meters, × trailScale.
		   Full ribbon is 2× that (offset both sides). User thickness scales it. */
		constexpr float kBlishHalfM = 20.f * 0.0254f; /* ~0.508 m */
		const float halfM = kBlishHalfM *
			std::max(0.35f, seg.trailScale) *
			std::clamp(thickness, 0.35f, 4.f);

		Texture_t* texture = nullptr;
		if (seg.textureId[0] && G::API && G::API->Textures_Get)
		{
			texture = G::API->Textures_Get(seg.textureId);
			if (texture && !texture->Resource)
				texture = nullptr;
		}

		const Vec3 worldUp{0.f, 1.f, 0.f};
		int drawn = 0;
		float pathV = 0.f;
		float alongM = 0.f;

		auto project = [&](const Vec3& w, float& sx, float& sy, float& fadeOut) -> bool
		{
			const float adx = avatar.x - w.x;
			const float ady = avatar.y - w.y;
			const float adz = avatar.z - w.z;
			const float d2 = adx * adx + ady * ady + adz * adz;
			if (!std::isfinite(d2) || d2 > fadeEnd2)
				return false;
			const float dist = std::sqrt(d2);
			fadeOut = 1.f;
			if (dist > fadeStart)
				fadeOut = 1.f - (dist - fadeStart) / std::max(1.f, fadeEnd - fadeStart);
			fadeOut = std::clamp(fadeOut, 0.f, 1.f);
			if (baseA * fadeOut < 0.05f)
				return false;
			return WorldToScreen(w, viewProj, screenW, screenH, sx, sy);
		};

		for (size_t i = 0; i + 1 < seg.points.size(); ++i)
		{
			if (drawn >= segsLeft)
				break;
			const TekkitTrails::WorldPoint& a = seg.points[i];
			const TekkitTrails::WorldPoint& b = seg.points[i + 1];
			if (!ReasonablePos(a.x, a.y, a.z) || !ReasonablePos(b.x, b.y, b.z))
				continue;

			Vec3 p0{a.x, a.y + kHeightBias, a.z};
			Vec3 p1{b.x, b.y + kHeightBias, b.z};
			Vec3 dir{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
			const float segLen = std::sqrt(dir.LengthSq());
			if (!(segLen > 0.05f) || !std::isfinite(segLen))
				continue;

			/* Horizontal perpendicular (Blish Cross(path, Forward) ≈ XZ plane). */
			Vec3 side = dir.Cross(worldUp);
			if (side.LengthSq() < 1e-8f)
				side = dir.Cross(Vec3{1.f, 0.f, 0.f});
			side = side.Normalised();
			if (side.LengthSq() < 0.5f)
				continue;

			const Vec3 left0{p0.x + side.x * halfM, p0.y + side.y * halfM, p0.z + side.z * halfM};
			const Vec3 right0{p0.x - side.x * halfM, p0.y - side.y * halfM, p0.z - side.z * halfM};
			const Vec3 left1{p1.x + side.x * halfM, p1.y + side.y * halfM, p1.z + side.z * halfM};
			const Vec3 right1{p1.x - side.x * halfM, p1.y - side.y * halfM, p1.z - side.z * halfM};

			float fade0 = 1.f, fade1 = 1.f;
			float lx0 = 0.f, ly0 = 0.f, rx0 = 0.f, ry0 = 0.f;
			float lx1 = 0.f, ly1 = 0.f, rx1 = 0.f, ry1 = 0.f;
			if (!project(left0, lx0, ly0, fade0) || !project(right0, rx0, ry0, fade0))
				continue;
			if (!project(left1, lx1, ly1, fade1) || !project(right1, rx1, ry1, fade1))
				continue;

			/* Reject wild projections (behind camera / across screen). */
			const float qdx = lx1 - lx0;
			const float qdy = ly1 - ly0;
			const float qlen = std::sqrt(qdx * qdx + qdy * qdy);
			if (!std::isfinite(qlen) || qlen < 0.5f || qlen > screenW * 0.9f)
				continue;

			const float avgA = baseA * (fade0 + fade1) * 0.5f;
			const int aCh = static_cast<int>(std::clamp(avgA, 0.f, 1.f) * 255.f);
			const ImVec2 sL0{lx0, ly0}, sR0{rx0, ry0}, sL1{lx1, ly1}, sR1{rx1, ry1};

			if (texture)
			{
				/* UV along path in meters — Blish uses pastDistance / (TRAIL_WIDTH*2). */
				constexpr float kUvPeriod = kBlishHalfM * 2.f;
				const float v0 = -(alongM / kUvPeriod);
				const float v1 = -((alongM + segLen) / kUvPeriod);
				dl->AddImageQuad(
					reinterpret_cast<ImTextureID>(texture->Resource),
					sL0, sL1, sR1, sR0,
					ImVec2(0.f, v0), ImVec2(0.f, v1),
					ImVec2(1.f, v1), ImVec2(1.f, v0),
					IM_COL32(255, 255, 255, aCh));
			}
			else
			{
				dl->AddQuadFilled(sL0, sL1, sR1, sR0, IM_COL32(rr, gg, bb, aCh));
			}
			/* Soft edge so gaps between segments are less obvious. */
			dl->AddQuad(sL0, sL1, sR1, sR0, IM_COL32(rr, gg, bb, std::min(aCh, 180)), 1.25f);

			alongM += segLen;
			pathV = alongM;
			(void)pathV;
			++drawn;
		}
		return drawn;
	}

	void DrawWorldMarkers(
		ImDrawList* dl, const Mat4& viewProj, float screenW, float screenH,
		const Vec3& avatar, const std::vector<TekkitTrails::Marker>& markers)
	{
		for (const TekkitTrails::Marker& marker : markers)
		{
			const Vec3 world{
				marker.world.x,
				marker.world.y + marker.heightOffset,
				marker.world.z,
			};
			const float dx = world.x - avatar.x;
			const float dy = world.y - avatar.y;
			const float dz = world.z - avatar.z;
			const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (!std::isfinite(distance) || distance < 0.05f)
				continue;

			float fade = 1.f;
			float maxVis = 160.f;
			if (marker.fadeFar > 0.f)
				maxVis = std::max(maxVis, marker.fadeFar * kInchesToMeters);
			if (distance >= maxVis)
				continue;
			if (marker.fadeNear >= 0.f && marker.fadeFar > marker.fadeNear)
			{
				const float nearM = marker.fadeNear * kInchesToMeters;
				const float farM = std::max(nearM + 1.f, marker.fadeFar * kInchesToMeters);
				if (distance > nearM)
					fade = 1.f - (distance - nearM) / (farM - nearM);
			}
			float sx = 0.f, sy = 0.f;
			if (!WorldToScreen(world, viewProj, screenW, screenH, sx, sy))
				continue;
			const float size = std::clamp(
				marker.iconSize * 700.f / std::max(1.f, distance),
				marker.minSize, std::min(marker.maxSize, 128.f));
			const int alpha = static_cast<int>(
				std::clamp(marker.alpha * fade, 0.f, 1.f) * 255.f);
			if (alpha < 5)
				continue;

			Texture_t* texture = nullptr;
			if (marker.iconId[0] && G::API && G::API->Textures_Get)
			{
				texture = G::API->Textures_Get(marker.iconId);
				if (texture && !texture->Resource)
					texture = nullptr;
			}
			if (texture)
			{
				const float half = size * 0.5f;
				dl->AddImage(
					reinterpret_cast<ImTextureID>(texture->Resource),
					ImVec2(sx - half, sy - half), ImVec2(sx + half, sy + half),
					ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
					IM_COL32(255, 255, 255, alpha));
			}
			else
			{
				const int rr = static_cast<int>((marker.color >> 16) & 0xFFu);
				const int gg = static_cast<int>((marker.color >> 8) & 0xFFu);
				const int bb = static_cast<int>(marker.color & 0xFFu);
				dl->AddCircleFilled(
					ImVec2(sx, sy), std::max(2.f, size * 0.3f),
					IM_COL32(rr, gg, bb, alpha), 10);
			}
		}
	}

	const MumbleContext* Ctx()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return nullptr;
		return reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	}
}

void WorldOverlay::Render()
{
	try
	{
	if (!G::ShowTekkitTrails)
		return;
	if (G::HideOutOfGameplay && G::NexusLink && !G::NexusLink->IsGameplay)
		return;
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return;

	const MumbleContext* ctx = Ctx();
	if (!ctx || ctx->mapId == 0)
		return;

	/* Always drive trail loading — even when in-world GPS is off or the world
	   map is open — so enabling a category reloads without needing Reload packs. */
	TekkitTrails::Update(ctx->mapId);

	if (!G::ShowWorldTrails && !TekkitTrails::HasSearchGuide())
		return;
	if (G::HideWhenMapOpen && (ctx->uiState & static_cast<uint32_t>(UiStateBits::MapOpen)))
		return;

	/* Keep drawing whatever we already loaded — do not blank GPS while the
	   worker is indexing / reloading the pack. */

	const float ax = G::Mumble->fAvatarPosition[0];
	const float ay = G::Mumble->fAvatarPosition[1];
	const float az = G::Mumble->fAvatarPosition[2];
	if (!ReasonablePos(ax, ay, az))
		return;
	if (ax * ax + ay * ay + az * az < 0.25f)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	/* Must match ImGui draw-list space. NexusLink Width/Height can differ under
	   Wine/DPI and systematically shove GPS off the real path. */
	const float screenW = io.DisplaySize.x;
	const float screenH = io.DisplaySize.y;
	if (screenW < 64.f || screenH < 64.f || !std::isfinite(screenW) || !std::isfinite(screenH))
		return;

	Mat4 viewProj{};
	Vec3 cam{};
	if (!BuildViewProj(screenW, screenH, viewProj, cam))
		return;
	(void)cam;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;
	TekkitTrails::BeginFrame();

	const float maxDist = std::clamp(G::WorldTrailMaxDist, 40.f, 200.f);
	/* 1.0 ≈ Blish/TacO default trail width; slider is a multiplier. */
	const float thickness = std::clamp(G::WorldTrailWidth, 0.5f, 4.0f);
	int segsLeft = kMaxSegments;
	const Vec3 avatar{ax, ay, az};

	/* Nearby route windows only (not full-map copies on the render thread). */
	static std::vector<TekkitTrails::WorldSnippet> sNearCache;
	static std::vector<TekkitTrails::Marker> sMarkerCache;
	static TekkitTrails::WorldSnippet sGuideCache;
	static float sCacheAx = 0.f, sCacheAy = 0.f, sCacheAz = 0.f;
	static uint32_t sGpsMap = 0;
	static uint64_t sGpsContent = 0;

	const uint64_t content = TekkitTrails::ContentRevision();
	if (sGpsMap != ctx->mapId)
	{
		sNearCache.clear();
		sMarkerCache.clear();
		sGuideCache = {};
		sGpsMap = ctx->mapId;
		sGpsContent = 0;
		sCacheAx = ax;
		sCacheAy = ay;
		sCacheAz = az;
	}

	const float mdx = ax - sCacheAx;
	const float mdy = ay - sCacheAy;
	const float mdz = az - sCacheAz;
	const bool movedFar = (mdx * mdx + mdy * mdy + mdz * mdz) > (4.f * 4.f);
	const bool needRefresh = (sGpsContent != content) || sNearCache.empty() || movedFar;

	if (G::ShowWorldTrails && needRefresh)
	{
		std::vector<TekkitTrails::WorldSnippet> snips;
		std::vector<TekkitTrails::Marker> marks;
		if (TekkitTrails::TryNearbyWorldGps(ax, ay, az, maxDist, snips, marks))
		{
			/* Keep last ribbon if a refresh returns empty (lock timing / brief gap). */
			if (!snips.empty() || sGpsContent != content)
			{
				sNearCache = std::move(snips);
				sMarkerCache = std::move(marks);
			}
			sCacheAx = ax;
			sCacheAy = ay;
			sCacheAz = az;
			sGpsContent = content;
		}
	}

	if (TekkitTrails::HasSearchGuide())
	{
		const TekkitTrails::WorldSnippet guide = TekkitTrails::SearchGuideWorldSnippet();
		if (guide.points.size() >= 2)
			sGuideCache = guide;
		if (sGuideCache.points.size() >= 2)
		{
			segsLeft -= DrawTrailRibbon(dl, viewProj, screenW, screenH, avatar,
				sGuideCache, maxDist * 2.5f, thickness + 0.35f, segsLeft, true);
		}
	}
	else
		sGuideCache = {};

	if (G::ShowWorldTrails && segsLeft > 0)
	{
		const float drawDist = std::max(maxDist * 2.0f, 180.f);
		for (const TekkitTrails::WorldSnippet& seg : sNearCache)
		{
			if (segsLeft <= 0)
				break;
			segsLeft -= DrawTrailRibbon(dl, viewProj, screenW, screenH, avatar,
				seg, drawDist, thickness, segsLeft, false);
		}
		DrawWorldMarkers(dl, viewProj, screenW, screenH, avatar, sMarkerCache);
	}
	}
	catch (...)
	{
	}
}
