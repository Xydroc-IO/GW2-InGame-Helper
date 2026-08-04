#pragma once

#include <cmath>

/* Shared camera / projection math for ImGui GPS and D3D world ribbons. */
namespace WorldGpsMath
{
	constexpr float kNearClip = 0.5f;
	constexpr float kFarClip = 8000.f;
	constexpr float kDefaultFov = 1.222f;
	constexpr float kHeightBias = 0.f;
	constexpr float kInchesToMeters = 1.f / 39.3700787f;
	constexpr float kAvatarMarkerHideM = 2.0f;
	constexpr float kAvatarMarkerFadeM = 5.5f;
	constexpr float kAvatarTrailHideAt1 = 5.0f;
	constexpr float kAvatarTrailFadeExtraAt1 = 4.0f;
	/* Blish StandardTrail: TRAIL_WIDTH = 20" half-offset × trailScale.
	   UV tiles use the unscaled base (TRAIL_WIDTH*2), not width*scale. */
	constexpr float kBlishHalfM = 20.f * 0.0254f;
	constexpr float kBlishUvPeriodM = kBlishHalfM * 2.f;
	constexpr int   kMaxSegments = 4000;

	/* halfWidth = Blish TRAIL_WIDTH (20") × pack trailScale × user GPS width.
	   No soft-clamp — Blish uses TrailScale as authored. */
	inline float TrailHalfWidthM(float packTrailScale, float userMul)
	{
		float scale = packTrailScale;
		if (!(scale >= 0.05f && scale <= 8.f))
			scale = 1.f;
		float mul = userMul;
		if (!(mul >= 0.5f && mul <= 4.f))
			mul = 1.f;
		return kBlishHalfM * scale * mul;
	}

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
		Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
		Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
		Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
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

	bool Finite3(float x, float y, float z);
	bool ReasonablePos(float x, float y, float z);
	float ParseFovRadians();
	bool BuildViewProj(float screenW, float screenH, Mat4& out, Vec3& camOut);
	bool WorldToScreen(const Vec3& world, const Mat4& viewProj,
		float screenW, float screenH, float& sx, float& sy);
	float ClipW(const Vec3& world, const Mat4& viewProj);
	Vec3 Lerp3(const Vec3& a, const Vec3& b, float t);
	void TrailFadeRange(float maxDist, float& fadeStart, float& fadeEnd);
}
