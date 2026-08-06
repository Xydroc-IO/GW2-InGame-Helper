#include "WorldGpsImgui.h"

#include "Globals.h"
#include "PathingIndex.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

int WorldGpsImgui::DrawTrailBillboards(
	ImDrawList* dl,
	const WorldGpsMath::Mat4& viewProj,
	float screenW, float screenH,
	const WorldGpsMath::Vec3& avatar,
	const PathingTrails::WorldSnippet& seg,
	float maxDist, float thickness, int segsLeft, bool bright)
{
	using WorldGpsMath::Vec3;
	if (!dl || seg.points.size() < 2 || segsLeft <= 0)
		return 0;

	/* Blish/Taimi draw world-space strips with a real depth buffer.
	   Nexus only gives us ImGui - continuous screen ribbons stretch into
	   giant chevrons when looking along the path. Match their *look* with
	   discrete billboarded chevrons sized by camera distance instead. */

	int rr = static_cast<int>((seg.color >> 16) & 0xFFu);
	int gg = static_cast<int>((seg.color >> 8) & 0xFFu);
	int bb = static_cast<int>(seg.color & 0xFFu);
	const float baseA = (bright ? 0.98f : 0.92f) *
		std::clamp(seg.alpha > 0.05f ? seg.alpha : 1.f, 0.f, 1.f);

	float fadeStart = 0.f, fadeEnd = 0.f;
	WorldGpsMath::TrailFadeRange(maxDist, fadeStart, fadeEnd);
	const float fadeEnd2 = fadeEnd * fadeEnd;

	const float halfM = WorldGpsMath::TrailHalfWidthM(seg.trailScale, thickness);
	const float widthMul = halfM / WorldGpsMath::kBlishHalfM;
	const float stepM = std::clamp(halfM * 2.15f, 0.85f, 2.4f);

	Texture_t* texture = nullptr;
	if (seg.textureId[0] && G::API && G::API->Textures_Get)
	{
		texture = G::API->Textures_Get(seg.textureId);
		if (texture && !texture->Resource)
			texture = nullptr;
	}

	Vec3 camPos{};
	if (G::Mumble)
	{
		camPos.x = G::Mumble->fCameraPosition[0];
		camPos.y = G::Mumble->fCameraPosition[1];
		camPos.z = G::Mumble->fCameraPosition[2];
	}
	auto camDist = [&](const Vec3& w) -> float
	{
		const float dx = w.x - camPos.x;
		const float dy = w.y - camPos.y;
		const float dz = w.z - camPos.z;
		const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
		return std::isfinite(d) ? d : 1.0e30f;
	};

	const float clearMul = std::clamp(G::WorldTrailPlayerClear, 0.f, 3.f);
	const float hideM = WorldGpsMath::kAvatarTrailHideAt1 * clearMul;
	const float fadeM = hideM + WorldGpsMath::kAvatarTrailFadeExtraAt1 * clearMul;
	const bool clearOn = clearMul > 0.001f;

	auto sampleFade = [&](const Vec3& w, float& fadeOut) -> bool
	{
		const float adx = avatar.x - w.x;
		const float ady = avatar.y - w.y;
		const float adz = avatar.z - w.z;
		const float d2 = adx * adx + ady * ady * 0.25f + adz * adz;
		if (!std::isfinite(d2) || d2 > fadeEnd2)
			return false;
		const float dist = std::sqrt(d2);
		fadeOut = 1.f;
		if (dist > fadeStart)
			fadeOut = 1.f - (dist - fadeStart) / std::max(1.f, fadeEnd - fadeStart);
		if (clearOn)
		{
			const float hDist = std::sqrt(adx * adx + adz * adz);
			const float vDist = std::fabs(ady);
			if (vDist < 3.5f)
			{
				if (hDist <= hideM)
					return false;
				if (hDist < fadeM)
				{
					const float t = (hDist - hideM) / std::max(0.25f, fadeM - hideM);
					fadeOut *= std::clamp(t, 0.f, 1.f);
				}
			}
		}
		fadeOut = std::clamp(fadeOut, 0.f, 1.f);
		return baseA * fadeOut >= 0.05f;
	};

	auto projectWorld = [&](const Vec3& w, float& sx, float& sy) -> bool
	{
		return WorldGpsMath::WorldToScreen(w, viewProj, screenW, screenH, sx, sy);
	};

	int drawn = 0;
	constexpr float kMinSpacing2 = 0.45f * 0.45f;
	constexpr float kMaxGap2 = 120.f * 120.f;

	auto drawBillboards = [&](const std::vector<Vec3>& pts) -> void
	{
		if (pts.size() < 2 || drawn >= segsLeft)
			return;

		struct Sample { Vec3 p; Vec3 tangent; };
		std::vector<Sample> samples;
		samples.reserve(256);
		float carry = 0.f;
		for (size_t i = 0; i + 1 < pts.size(); ++i)
		{
			Vec3 a = pts[i];
			Vec3 b = pts[i + 1];
			Vec3 d{b.x - a.x, b.y - a.y, b.z - a.z};
			float len = std::sqrt(d.LengthSq());
			if (!(len > 0.05f) || !std::isfinite(len) || len > 120.f)
				continue;
			Vec3 tan = d.Normalised();
			float t = 0.f;
			if (carry > 0.f && carry < stepM)
				t = stepM - carry;
			while (t <= len + 1.0e-4f)
			{
				const float u = std::clamp(t / len, 0.f, 1.f);
				samples.push_back({WorldGpsMath::Lerp3(a, b, u), tan});
				t += stepM;
				if (static_cast<int>(samples.size()) >= segsLeft)
					break;
			}
			const float consumed = (t - stepM);
			carry = (consumed > 0.f) ? (len - consumed) : (carry + len);
			if (carry >= stepM)
				carry = std::fmod(carry, stepM);
			if (static_cast<int>(samples.size()) >= segsLeft)
				break;
		}
		if (samples.empty())
			return;

		float prevSx = 0.f, prevSy = 0.f;
		bool havePrev = false;
		for (size_t i = 0; i < samples.size(); ++i)
		{
			if (drawn >= segsLeft)
				break;
			const Vec3& p = samples[i].p;
			float fade = 1.f;
			if (!sampleFade(p, fade))
			{
				havePrev = false;
				continue;
			}
			const float cd = camDist(p);
			if (cd < 2.75f || WorldGpsMath::ClipW(p, viewProj) < 2.0f)
			{
				havePrev = false;
				continue;
			}
			float sx = 0.f, sy = 0.f;
			if (!projectWorld(p, sx, sy))
			{
				havePrev = false;
				continue;
			}
			float halfPx = std::clamp(
				halfM * 780.f / std::max(6.f, cd),
				4.0f * widthMul, 18.f * widthMul);
			if (cd < 8.f)
				halfPx = std::min(halfPx, 10.f * widthMul);

			Vec3 ahead = {
				p.x + samples[i].tangent.x * 0.75f,
				p.y + samples[i].tangent.y * 0.75f,
				p.z + samples[i].tangent.z * 0.75f,
			};
			float ax = sx, ay = sy;
			float bx = sx + 1.f, by = sy;
			if (projectWorld(ahead, bx, by))
			{
				ax = sx; ay = sy;
			}
			float tdx = bx - ax;
			float tdy = by - ay;
			float tlen = std::sqrt(tdx * tdx + tdy * tdy);
			if (!(tlen > 0.5f))
			{
				tdx = 0.f; tdy = -1.f; tlen = 1.f;
			}
			tdx /= tlen; tdy /= tlen;
			const float px = -tdy;
			const float py = tdx;

			const int aCh = static_cast<int>(std::clamp(baseA * fade, 0.f, 1.f) * 255.f);
			if (aCh < 8)
			{
				havePrev = false;
				continue;
			}

			const float along = halfPx * 1.15f;
			const ImVec2 c0{sx - tdx * along + px * halfPx, sy - tdy * along + py * halfPx};
			const ImVec2 c1{sx + tdx * along + px * halfPx, sy + tdy * along + py * halfPx};
			const ImVec2 c2{sx + tdx * along - px * halfPx, sy + tdy * along - py * halfPx};
			const ImVec2 c3{sx - tdx * along - px * halfPx, sy - tdy * along - py * halfPx};

			if (texture)
			{
				dl->AddImageQuad(
					reinterpret_cast<ImTextureID>(texture->Resource),
					c0, c1, c2, c3,
					ImVec2(0.f, 0.f), ImVec2(0.f, 1.f),
					ImVec2(1.f, 1.f), ImVec2(1.f, 0.f),
					IM_COL32(255, 255, 255, aCh));
			}
			else
			{
				dl->AddQuadFilled(c0, c1, c2, c3, IM_COL32(rr, gg, bb, aCh));
			}

			if (havePrev)
			{
				const float ldx = sx - prevSx;
				const float ldy = sy - prevSy;
				const float llen = std::sqrt(ldx * ldx + ldy * ldy);
				if (llen > 2.f && llen < std::min(screenW, screenH) * 0.22f)
				{
					dl->AddLine(ImVec2(prevSx, prevSy), ImVec2(sx, sy),
						IM_COL32(rr, gg, bb, std::min(aCh, 110)),
						std::clamp(halfPx * 0.22f, 1.2f, 3.5f));
				}
			}
			prevSx = sx;
			prevSy = sy;
			havePrev = true;
			++drawn;
		}
	};

	std::vector<Vec3> pts;
	pts.reserve(64);
	for (const PathingTrails::WorldPoint& wp : seg.points)
	{
		if (!WorldGpsMath::ReasonablePos(wp.x, wp.y, wp.z))
		{
			drawBillboards(pts);
			pts.clear();
			continue;
		}
		Vec3 p{wp.x, wp.y + WorldGpsMath::kHeightBias, wp.z};
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
				drawBillboards(pts);
				pts.clear();
			}
		}
		pts.push_back(p);
		if (pts.size() >= 160)
		{
			drawBillboards(pts);
			Vec3 keep = pts.back();
			pts.clear();
			pts.push_back(keep);
		}
	}
	drawBillboards(pts);
	return drawn;
}

void WorldGpsImgui::DrawMarkers(
	ImDrawList* dl,
	const WorldGpsMath::Mat4& viewProj,
	float screenW, float screenH,
	const WorldGpsMath::Vec3& avatar,
	const std::vector<PathingTrails::Marker>& markers)
{
	using WorldGpsMath::Vec3;
	for (const PathingTrails::Marker& marker : markers)
	{
		const float heightM = marker.heightOffset * WorldGpsMath::kInchesToMeters;
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
		/* Soft-clear around the avatar - Marker clear slider (default ~2-5.5m).
		   Mount/bfs guide icons keep a smaller bubble so they stay visible on path. */
		const bool guideIcon = PathingDetail::IsMountShortcutMarker(marker) ||
			(marker.label[0] && std::strstr(marker.label, ".bfs") != nullptr);
		const float clearMul = std::clamp(G::WorldMarkerPlayerClear, 0.f, 3.f);
		if (clearMul > 0.01f)
		{
			float hideM = WorldGpsMath::kAvatarMarkerHideAt1 * clearMul;
			float fadeM = hideM + WorldGpsMath::kAvatarMarkerFadeExtraAt1 * clearMul;
			if (guideIcon)
			{
				hideM *= 0.35f;
				fadeM = hideM + (fadeM - hideM) * 0.45f;
			}
			if (horiz <= hideM)
				continue;
			if (horiz < fadeM)
			{
				float t = (horiz - hideM) / std::max(0.25f, fadeM - hideM);
				t = std::clamp(t, 0.f, 1.f);
				nearFade = t * t * (3.f - 2.f * t);
			}
		}

		float fade = nearFade;
		float maxVis = 160.f;
		if (marker.fadeFar > 0.f)
			maxVis = std::max(maxVis, marker.fadeFar * WorldGpsMath::kInchesToMeters);
		if (distance >= maxVis)
			continue;
		if (marker.fadeNear >= 0.f && marker.fadeFar > marker.fadeNear)
		{
			const float nearM = marker.fadeNear * WorldGpsMath::kInchesToMeters;
			const float farM = std::max(nearM + 1.f, marker.fadeFar * WorldGpsMath::kInchesToMeters);
			if (distance > nearM)
				fade *= 1.f - (distance - nearM) / (farM - nearM);
		}
		float sx = 0.f, sy = 0.f;
		if (!WorldGpsMath::WorldToScreen(world, viewProj, screenW, screenH, sx, sy))
			continue;
		const float iconMul = std::clamp(G::WorldMarkerScale, 0.5f, 3.f);
		float size = std::clamp(
			marker.iconSize * 700.f / std::max(1.f, distance),
			marker.minSize, std::min(marker.maxSize, 128.f));
		size = std::clamp(size * iconMul, 4.f, 256.f);
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
