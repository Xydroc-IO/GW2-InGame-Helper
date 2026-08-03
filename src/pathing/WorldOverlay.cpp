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
	/* Soft-hide POI icons that would cover the avatar. */
	constexpr float kAvatarMarkerHideM = 2.0f;
	constexpr float kAvatarMarkerFadeM = 5.5f;
	/* Hard world clear — always on so ImGui trails never sit on the mesh.
	   Soft fade beyond this is scaled by G::WorldTrailPlayerClear. */
	constexpr float kAvatarTrailHardHideM = 2.75f;

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
		if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cw) || cw <= 0.35f)
			return false;
		const float ndcX = cx / cw;
		const float ndcY = cy / cw;
		if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
			return false;
		if (ndcX < -3.5f || ndcX > 3.5f || ndcY < -3.5f || ndcY > 3.5f)
			return false;
		sx = (ndcX + 1.f) * 0.5f * screenW;
		sy = (-ndcY + 1.f) * 0.5f * screenH;
		return std::isfinite(sx) && std::isfinite(sy);
	}

	Vec3 Lerp3(const Vec3& a, const Vec3& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
		};
	}

	float ClipW(const Vec3& world, const Mat4& viewProj)
	{
		float cx, cy, cz, cw;
		viewProj.Transform(world.x, world.y, world.z, cx, cy, cz, cw);
		return cw;
	}

	/* Pull segment endpoints in front of the camera near plane so look-along /
	   underfoot camera angles don't blank whole ribbons. */
	bool ClipSegmentToNearPlane(Vec3& a, Vec3& b, const Mat4& viewProj)
	{
		/* Higher than kNearClip — points barely past the plane project wildly
		   and stretch gold lines when looking along the trail. */
		constexpr float kEps = 0.45f;
		const float aw = ClipW(a, viewProj);
		const float bw = ClipW(b, viewProj);
		if (!std::isfinite(aw) || !std::isfinite(bw))
			return false;
		if (aw <= kEps && bw <= kEps)
			return false;
		if (aw <= kEps)
		{
			const float denom = bw - aw;
			if (!(std::fabs(denom) > 1e-6f))
				return false;
			const float t = (kEps - aw) / denom;
			if (!(t > 0.f && t < 1.f))
				return false;
			a = Lerp3(a, b, t);
		}
		else if (bw <= kEps)
		{
			const float denom = aw - bw;
			if (!(std::fabs(denom) > 1e-6f))
				return false;
			const float t = (kEps - bw) / denom;
			if (!(t > 0.f && t < 1.f))
				return false;
			b = Lerp3(b, a, t);
		}
		return true;
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
		const float baseA = (bright ? 0.98f : 0.92f) *
			std::clamp(seg.alpha > 0.05f ? seg.alpha : 1.f, 0.f, 1.f);

		float fadeStart = 0.f, fadeEnd = 0.f;
		TrailFadeRange(seg, maxDist, fadeStart, fadeEnd);
		const float fadeEnd2 = fadeEnd * fadeEnd;

		constexpr float kBlishHalfM = 28.f * 0.0254f; /* ~Blish chevron ribbon width */
		/* Pack trailScale × Lady edition bias × GPS width slider.
		   Bias normalizes Barefoot/WP/Mounts so 1.0 looks correct; the slider
		   then scales every edition by the same factor. */
		const float widthMul =
			std::clamp(seg.trailScale, 0.5f, 2.0f) *
			std::clamp(seg.widthBias, 0.5f, 8.0f) *
			std::clamp(thickness, 0.5f, 4.0f);
		const float halfM = kBlishHalfM * widthMul;

		Texture_t* texture = nullptr;
		if (seg.textureId[0] && G::API && G::API->Textures_Get)
		{
			texture = G::API->Textures_Get(seg.textureId);
			if (texture && !texture->Resource)
				texture = nullptr;
		}

		int drawn = 0;
		constexpr float kMinSpacing2 = 0.75f * 0.75f;
		/* Continuity vs stretch — break only on big jumps; subdivide for draw. */
		constexpr float kMaxGap2 = 40.f * 40.f;
		constexpr float kDrawStepM = 6.0f;
		const float maxLineScreen = std::min(280.f, std::min(screenW, screenH) * 0.35f);

		const float clearMul = std::clamp(G::WorldTrailPlayerClear, 0.f, 3.f);
		/* Soft bubble grows with the slider; hard hide always stays on. */
		const float softHideM = kAvatarTrailHardHideM + 2.25f * clearMul;
		const float softFadeM = (clearMul <= 0.001f)
			? softHideM
			: softHideM + 4.0f * clearMul;

		auto distFade = [&](const Vec3& w, float& fadeOut) -> bool
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
			/* Hard clear always — even at Player clear 0. */
			if (dist <= kAvatarTrailHardHideM)
				return false;
			if (clearMul > 0.001f && dist < softFadeM)
			{
				const float lo = softHideM;
				if (dist <= lo)
					return false;
				const float t = (dist - lo) / std::max(0.25f, softFadeM - lo);
				fadeOut *= std::clamp(t, 0.f, 1.f);
			}
			fadeOut = std::clamp(fadeOut, 0.f, 1.f);
			return baseA * fadeOut >= 0.05f;
		};

		/* Closest 3D approach of a segment to the avatar. */
		auto segDistToAvatar = [&](const Vec3& a, const Vec3& b) -> float
		{
			const float abx = b.x - a.x;
			const float aby = b.y - a.y;
			const float abz = b.z - a.z;
			const float ab2 = abx * abx + aby * aby + abz * abz;
			float t = 0.f;
			if (ab2 > 1.0e-6f)
			{
				t = ((avatar.x - a.x) * abx + (avatar.y - a.y) * aby +
					(avatar.z - a.z) * abz) / ab2;
				t = std::clamp(t, 0.f, 1.f);
			}
			const float cx = a.x + abx * t - avatar.x;
			const float cy = a.y + aby * t - avatar.y;
			const float cz = a.z + abz * t - avatar.z;
			return std::sqrt(cx * cx + cy * cy + cz * cz);
		};

		auto projectWorld = [&](Vec3 w, float& sx, float& sy) -> bool
		{
			return WorldToScreen(w, viewProj, screenW, screenH, sx, sy);
		};

		/* Screen-space clear — trails ahead still land on the character silhouette
		   because ImGui draws over the scene with no depth test. */
		float avatarSx = 0.f, avatarSy = 0.f;
		float avatarScreenR = std::min(screenW, screenH) * 0.10f;
		bool haveAvatarScreen = false;
		{
			/* Feet + torso + mount-ish height — pick the strongest on-screen radius. */
			const float heights[] = { 0.35f, 1.15f, 2.35f };
			for (float hy : heights)
			{
				float sx = 0.f, sy = 0.f;
				if (!projectWorld(Vec3{avatar.x, avatar.y + hy, avatar.z}, sx, sy))
					continue;
				if (!haveAvatarScreen)
				{
					avatarSx = sx;
					avatarSy = sy;
					haveAvatarScreen = true;
				}
				else
				{
					/* Bias toward torso for the clear center. */
					if (hy > 0.9f && hy < 1.5f)
					{
						avatarSx = sx;
						avatarSy = sy;
					}
				}
				float ox = 0.f, oy = 0.f;
				const float side = (hy > 1.8f) ? 2.0f : 1.4f;
				if (projectWorld(Vec3{avatar.x + side, avatar.y + hy, avatar.z}, ox, oy))
				{
					const float dx = ox - sx;
					const float dy = oy - sy;
					const float r = std::sqrt(dx * dx + dy * dy);
					if (std::isfinite(r) && r > 8.f)
					{
						const float cand = std::clamp(r * 2.6f, 64.f,
							std::min(screenW, screenH) * 0.28f);
						avatarScreenR = std::max(avatarScreenR, cand);
					}
				}
			}
		}

		auto screenLen = [](float x0, float y0, float x1, float y1) -> float
		{
			const float dx = x1 - x0;
			const float dy = y1 - y0;
			const float len = std::sqrt(dx * dx + dy * dy);
			return std::isfinite(len) ? len : 1.0e30f;
		};

		auto pointToSeg2D = [](float px, float py,
			float x0, float y0, float x1, float y1) -> float
		{
			const float abx = x1 - x0;
			const float aby = y1 - y0;
			const float ab2 = abx * abx + aby * aby;
			float t = 0.f;
			if (ab2 > 1.0e-6f)
			{
				t = ((px - x0) * abx + (py - y0) * aby) / ab2;
				t = std::clamp(t, 0.f, 1.f);
			}
			const float cx = x0 + abx * t - px;
			const float cy = y0 + aby * t - py;
			return std::sqrt(cx * cx + cy * cy);
		};

		auto drawSeg = [&](Vec3 raw0, Vec3 raw1, float& alongM) -> void
		{
			if (drawn >= segsLeft)
				return;
			Vec3 dir{raw1.x - raw0.x, raw1.y - raw0.y, raw1.z - raw0.z};
			const float segLen = std::sqrt(dir.LengthSq());
			if (!(segLen > 0.2f) || !std::isfinite(segLen) || segLen > 40.f)
				return;

			float fade0 = 1.f, fade1 = 1.f;
			const bool in0 = distFade(raw0, fade0);
			const bool in1 = distFade(raw1, fade1);
			if (!in0 && !in1)
			{
				alongM += segLen;
				return;
			}
			/* Segment may still skim the player/mount even if endpoints are outside. */
			const float nearD = segDistToAvatar(raw0, raw1);
			if (nearD <= kAvatarTrailHardHideM)
			{
				alongM += segLen;
				return;
			}
			float avatarMul = 1.f;
			if (clearMul > 0.001f && nearD < softFadeM)
			{
				if (nearD <= softHideM)
				{
					alongM += segLen;
					return;
				}
				avatarMul = (nearD - softHideM) / std::max(0.25f, softFadeM - softHideM);
				avatarMul = std::clamp(avatarMul, 0.f, 1.f);
			}

			Vec3 p0 = raw0;
			Vec3 p1 = raw1;
			if (!ClipSegmentToNearPlane(p0, p1, viewProj))
			{
				alongM += segLen;
				return;
			}

			float cx0 = 0.f, cy0 = 0.f, cx1 = 0.f, cy1 = 0.f;
			if (!projectWorld(p0, cx0, cy0) || !projectWorld(p1, cx1, cy1))
			{
				alongM += segLen;
				return;
			}
			const float centerLen = screenLen(cx0, cy0, cx1, cy1);
			if (centerLen < 0.5f || centerLen > maxLineScreen)
			{
				alongM += segLen;
				return;
			}

			/* Screen-space ribbon — world-space side offsets blow up look-along
			   and we used to fall back to a solid cyan/white line. Pixel width
			   stays stable so pack chevron textures actually show. */
			const float sdx = cx1 - cx0;
			const float sdy = cy1 - cy0;
			const float invLen = 1.f / std::max(0.001f, centerLen);
			const float px = -sdy * invLen;
			const float py = sdx * invLen;
			const float avgDist = 0.5f * (
				std::sqrt(
					(avatar.x - p0.x) * (avatar.x - p0.x) +
					(avatar.y - p0.y) * (avatar.y - p0.y) +
					(avatar.z - p0.z) * (avatar.z - p0.z)) +
				std::sqrt(
					(avatar.x - p1.x) * (avatar.x - p1.x) +
					(avatar.y - p1.y) * (avatar.y - p1.y) +
					(avatar.z - p1.z) * (avatar.z - p1.z)));
			/* Perspective screen width — old fixed [5,16]px clamp saturated near
			   the avatar, so GPS width looked inert up close (and on dense Barefoot
			   feet trails). Scale the clamp with widthMul so the slider always bites. */
			const float halfPx = std::clamp(
				halfM * 900.f / std::max(4.f, avgDist),
				4.f * widthMul, 14.f * widthMul);

			/* Hard silhouette clear always includes ribbon half-width — previously
			   center-line distance alone let thick trails paint over the player
			   when Player clear was low / fade was off. Soft fade scales separately. */
			if (haveAvatarScreen)
			{
				const float segScreen = pointToSeg2D(
					avatarSx, avatarSy, cx0, cy0, cx1, cy1);
				const float hardR = avatarScreenR + halfPx + 10.f;
				if (segScreen <= hardR)
				{
					alongM += segLen;
					return;
				}
				if (clearMul > 0.001f)
				{
					const float softR = hardR + avatarScreenR * (0.45f + 1.25f * clearMul);
					if (segScreen < softR)
					{
						const float t = (segScreen - hardR) / std::max(1.f, softR - hardR);
						avatarMul *= std::clamp(t, 0.f, 1.f);
					}
				}
			}

			const float avgA = baseA * avatarMul * (
				(in0 ? fade0 : fade1) + (in1 ? fade1 : fade0)) * 0.5f;
			const int aCh = static_cast<int>(std::clamp(avgA, 0.f, 1.f) * 255.f);
			if (aCh < 8)
			{
				alongM += segLen;
				return;
			}

			const ImVec2 sL0{cx0 + px * halfPx, cy0 + py * halfPx};
			const ImVec2 sR0{cx0 - px * halfPx, cy0 - py * halfPx};
			const ImVec2 sL1{cx1 + px * halfPx, cy1 + py * halfPx};
			const ImVec2 sR1{cx1 - px * halfPx, cy1 - py * halfPx};

			if (texture)
			{
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
				/* Missing pack texture — soft gold, not default cyan. */
				dl->AddQuadFilled(sL0, sL1, sR1, sR0,
					IM_COL32(rr, gg, bb, aCh));
			}

			alongM += segLen;
			++drawn;
		};

		auto drawSection = [&](const std::vector<Vec3>& pts) -> void
		{
			if (pts.size() < 2 || drawn >= segsLeft)
				return;
			float alongM = 0.f;
			for (size_t i = 0; i + 1 < pts.size(); ++i)
			{
				if (drawn >= segsLeft)
					break;
				const Vec3& a = pts[i];
				const Vec3& b = pts[i + 1];
				Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
				const float len = std::sqrt(ab.LengthSq());
				if (!(len > 0.2f) || !std::isfinite(len) || len > 40.f)
					continue;
				/* Subdivide long edges so look-along never stretches one quad. */
				const int steps = std::max(1, static_cast<int>(std::ceil(len / kDrawStepM)));
				for (int s = 0; s < steps; ++s)
				{
					const float t0 = static_cast<float>(s) / static_cast<float>(steps);
					const float t1 = static_cast<float>(s + 1) / static_cast<float>(steps);
					drawSeg(Lerp3(a, b, t0), Lerp3(a, b, t1), alongM);
				}
			}
		};

		std::vector<Vec3> pts;
		pts.reserve(64);
		for (const TekkitTrails::WorldPoint& wp : seg.points)
		{
			if (!ReasonablePos(wp.x, wp.y, wp.z))
			{
				/* TacO/Taimi (0,0,0) / NaN section break — do not stitch. */
				drawSection(pts);
				pts.clear();
				continue;
			}
			Vec3 p{wp.x, wp.y + kHeightBias, wp.z};
			if (!pts.empty())
			{
				const float dx = p.x - pts.back().x;
				const float dy = p.y - pts.back().y;
				const float dz = p.z - pts.back().z;
				const float d2 = dx * dx + dy * dy + dz * dz;
				if (d2 < kMinSpacing2)
					continue;
				if (d2 > kMaxGap2)
				{
					drawSection(pts);
					pts.clear();
				}
			}
			pts.push_back(p);
			if (pts.size() >= 96)
			{
				drawSection(pts);
				Vec3 keep = pts.back();
				pts.clear();
				pts.push_back(keep);
			}
		}
		drawSection(pts);
		return drawn;
	}

	void DrawWorldMarkers(
		ImDrawList* dl, const Mat4& viewProj, float screenW, float screenH,
		const Vec3& avatar, const std::vector<TekkitTrails::Marker>& markers)
	{
		for (const TekkitTrails::Marker& marker : markers)
		{
			/* TacO heightOffset is inches (same for Numbers and Mounts icons). */
			const float heightM = marker.heightOffset * kInchesToMeters;
			const Vec3 world{
				marker.world.x,
				marker.world.y + heightM,
				marker.world.z,
			};
			const float dx = world.x - avatar.x;
			const float dy = world.y - avatar.y;
			const float dz = world.z - avatar.z;
			const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (!std::isfinite(distance) || distance < 0.05f)
				continue;

			const float horiz = std::sqrt(dx * dx + dz * dz);
			float nearFade = 1.f;
			if (horiz <= kAvatarMarkerHideM)
				continue;
			if (horiz < kAvatarMarkerFadeM)
			{
				float t = (horiz - kAvatarMarkerHideM) /
					(kAvatarMarkerFadeM - kAvatarMarkerHideM);
				t = std::clamp(t, 0.f, 1.f);
				nearFade = t * t * (3.f - 2.f * t);
			}

			float fade = nearFade;
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
					fade *= 1.f - (distance - nearM) / (farM - nearM);
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

			/* Distance label only for mount PNGs (same markers as Numbers otherwise). */
			const bool mountIcon = marker.iconId[0] &&
				(std::strstr(marker.iconId, "Mounts") || std::strstr(marker.iconId, "mounts"));
			if (mountIcon && marker.tipName[0] && distance < 90.f && alpha > 40)
			{
				char line[96];
				char upper[48]{};
				size_t ui = 0;
				for (const char* p = marker.tipName; *p && ui + 1 < sizeof(upper); ++p)
				{
					char ch = *p;
					if (ch >= 'a' && ch <= 'z')
						ch = static_cast<char>(ch - 'a' + 'A');
					if (ch == '_' || ch == '-')
						ch = ' ';
					upper[ui++] = ch;
				}
				upper[ui] = 0;
				if (distance >= 10.f)
					std::snprintf(line, sizeof(line), "%s  %.0fm", upper, distance);
				else
					std::snprintf(line, sizeof(line), "%s  %.1fm", upper, distance);
				const ImVec2 ts = ImGui::CalcTextSize(line);
				const float lx = sx - ts.x * 0.5f;
				const float ly = sy + size * 0.55f;
				dl->AddText(ImVec2(lx + 1.f, ly + 1.f), IM_COL32(0, 0, 0, alpha), line);
				dl->AddText(ImVec2(lx, ly), IM_COL32(220, 255, 255, alpha), line);
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
	TekkitTrails::TickMarkerBehaviors();

	if (!G::ShowWorldTrails && !TekkitTrails::HasSearchGuideActive())
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
	static Mat4 sLastViewProj{};
	static bool sHaveViewProj = false;
	if (!BuildViewProj(screenW, screenH, viewProj, cam))
	{
		/* Brief Mumble camera glitches used to blank the whole GPS for a frame. */
		if (!sHaveViewProj)
			return;
		viewProj = sLastViewProj;
	}
	else
	{
		sLastViewProj = viewProj;
		sHaveViewProj = true;
	}
	(void)cam;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;
	TekkitTrails::BeginFrame();

	const float maxDist = G::LadyWpOnly
		? std::clamp(std::max(G::WorldTrailMaxDist, 200.f), 160.f, 320.f)
		: std::clamp(G::WorldTrailMaxDist, 40.f, 200.f);
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
	/* WP Only refreshes sooner so the path stays ahead while moving. */
	const float refreshM = G::LadyWpOnly ? 4.0f : 10.f;
	const bool movedFar = (mdx * mdx + mdy * mdy + mdz * mdz) > (refreshM * refreshM);
	const bool needRefresh = (sGpsContent != content) || sNearCache.empty() || movedFar;

	if (G::ShowWorldTrails && needRefresh)
	{
		std::vector<TekkitTrails::WorldSnippet> snips;
		std::vector<TekkitTrails::Marker> marks;
		if (TekkitTrails::TryNearbyWorldGps(ax, ay, az, maxDist, snips, marks))
		{
			const bool got = !snips.empty() || !marks.empty();
			if (got)
			{
				sNearCache = std::move(snips);
				sMarkerCache = std::move(marks);
				sCacheAx = ax;
				sCacheAy = ay;
				sCacheAz = az;
				sGpsContent = content;
			}
			else if (!TekkitTrails::HasDrawableWorldGps())
			{
				/* Filters really off / nothing loaded — allow clear. */
				sNearCache.clear();
				sMarkerCache.clear();
				sCacheAx = ax;
				sCacheAy = ay;
				sCacheAz = az;
				sGpsContent = content;
			}
			else
			{
				/* Trails exist but this sample missed (gap / budget) — keep ribbon. */
				sCacheAx = ax;
				sCacheAy = ay;
				sCacheAz = az;
				sGpsContent = content;
			}
		}
	}

	if (TekkitTrails::HasSearchGuideActive())
	{
		if (TekkitTrails::HasSearchGuide())
		{
			const TekkitTrails::WorldSnippet guide = TekkitTrails::SearchGuideWorldSnippet();
			if (guide.points.size() >= 2)
				sGuideCache = guide;
		}
		if (sGuideCache.points.size() >= 2)
		{
			/* Huge fade window — avatar-distance fade was blinking far segments
			   of the orange route as you walked / looked along it. */
			constexpr float kGuideFadeM = 2500.f;
			segsLeft -= DrawTrailRibbon(dl, viewProj, screenW, screenH, avatar,
				sGuideCache, kGuideFadeM, thickness + 0.35f, segsLeft, true);
		}
	}
	else
		sGuideCache = {};

	if (G::ShowWorldTrails && segsLeft > 0)
	{
		float drawDist = std::max(maxDist * 1.35f, 90.f);
		if (G::LadyWpOnly)
			drawDist = std::max(maxDist * 1.85f, 260.f);
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
