#include "CompassOverlay.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingTrails.h"
#include "TrailToolsPreviewCompass.h"
#include "TrailToolsShared.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

/* Compass overlay math mirrors TacO GetMinimapRectangle + Blish Pathing
   FlatMap/GetScaledLocation (mapScale * 0.897, UI size from identity "uisz"). */

namespace
{
	const MumbleContext* Ctx()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return nullptr;
		return reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	}

	/* Identity JSON "uisz": 0 Small, 1 Normal, 2 Large, 3 Larger. */
	int ParseUiSize()
	{
		if (!G::Mumble)
			return 1;
		char id[260]{};
		const wchar_t* w = G::Mumble->identity;
		size_t n = 0;
		for (; n < 255 && w[n]; ++n)
			id[n] = (w[n] < 128) ? static_cast<char>(w[n]) : ' ';
		id[n] = 0;
		const char* p = std::strstr(id, "\"uisz\"");
		if (!p)
			return 1;
		p = std::strchr(p, ':');
		if (!p)
			return 1;
		const int v = static_cast<int>(std::atoi(p + 1));
		return (v >= 0 && v <= 3) ? v : 1;
	}

	/* TacO GetUIScale() */
	float UiScale(int uiSize)
	{
		switch (uiSize)
		{
		case 0:  return 0.9f;
		case 2:  return 1.111f;
		case 3:  return 1.224f;
		default: return 1.f;
		}
	}

	/* Bottom chrome inset under the compass (TacO GetMinimapRectangle). */
	int BottomChromeDelta(int uiSize)
	{
		switch (uiSize)
		{
		case 0:  return 33;
		case 2:  return 41;
		case 3:  return 45;
		default: return 37;
		}
	}

	/* Blish FlatMap GetOffset - soft padding around the compass widget. */
	int BlishPad(float curr, float maxV, float minV, float val)
	{
		constexpr float kMapOffsetMin = 19.f;
		if (maxV <= minV)
			return static_cast<int>(kMapOffsetMin);
		const float t = (curr - minV) / (maxV - minV);
		return static_cast<int>(std::lround(t * (val - kMapOffsetMin) + kMapOffsetMin));
	}

	struct CompassLayout
	{
		ImVec2 min{};
		ImVec2 max{};
		ImVec2 mid{}; /* projection center = compass center */
		float  uiScale = 1.f;
	};

	bool BuildLayout(const MumbleContext* ctx, float screenW, float screenH, CompassLayout& out)
	{
		const int uiSize = ParseUiSize();
		out.uiScale = UiScale(uiSize);

		/* TacO stores compassWidth/Height as mumble * UIScale. */
		float cw = static_cast<float>(ctx->compassWidth) * out.uiScale;
		float ch = static_cast<float>(ctx->compassHeight) * out.uiScale;
		if (cw < 32.f || ch < 32.f || !std::isfinite(cw) || !std::isfinite(ch))
			return false;
		cw = std::min(cw, screenW);
		ch = std::min(ch, screenH);

		const bool topRight =
			(ctx->uiState & static_cast<uint32_t>(UiStateBits::CompassTopRight)) != 0;

		/* TacO rectangle (flush to right edge). */
		float x1 = screenW - cw;
		float x2 = screenW;
		float y1, y2;
		if (topRight)
		{
			y1 = 1.f;
			y2 = ch + 1.f;
		}
		else
		{
			const float delta = static_cast<float>(BottomChromeDelta(uiSize)) * out.uiScale;
			y1 = screenH - ch - delta;
			y2 = screenH - delta;
		}

		/* Blend Blish padding so we sit on the painted compass, not chrome alone.
		   Keep projection mid on the TacO compass center (not the padded bounds). */
		constexpr float kMapWMax = 362.f, kMapWMin = 170.f;
		constexpr float kMapHMax = 338.f, kMapHMin = 170.f;
		const float rawW = static_cast<float>(ctx->compassWidth);
		const float rawH = static_cast<float>(ctx->compassHeight);
		const int padW = BlishPad(rawW, kMapWMax, kMapWMin, 40.f);
		const int padH = BlishPad(rawH, kMapHMax, kMapHMin, 40.f);

		out.min = ImVec2(x1, y1);
		out.max = ImVec2(x2, y2);
		/* Project from the padded compass content center (Blish FlatMap offset).
		   Using the unpadded TacO mid left trails slightly off the painted disc. */
		const float cx0 = x1 + static_cast<float>(padW);
		const float cy0 = y1 + static_cast<float>(padH);
		const float cx1 = x2 - static_cast<float>(padW);
		const float cy1 = y2 - static_cast<float>(padH);
		out.mid = ImVec2((cx0 + cx1) * 0.5f, (cy0 + cy1) * 0.5f);
		return true;
	}

	/* Blish Pathing: scale = MapScale * 0.897; then rotate about origin; then + mid. */
	ImVec2 ContinentToCompass(float continentX, float continentY,
		float mapCenterX, float mapCenterY, float mapScale,
		bool rotateOn, float compassRotation, ImVec2 mid)
	{
		/* Blish FlatMap workaround for pixel↔continent scaling. */
		float scale = mapScale * 0.897f;
		if (!(scale > 1e-6f) || !std::isfinite(scale))
			scale = 1.f;

		float dx = (continentX - mapCenterX) / scale;
		float dy = (continentY - mapCenterY) / scale;

		if (rotateOn && std::fabs(compassRotation) > 1e-6f)
		{
			const float c = std::cos(compassRotation);
			const float s = std::sin(compassRotation);
			const float rx = dx * c - dy * s;
			const float ry = dx * s + dy * c;
			dx = rx;
			dy = ry;
		}

		return ImVec2(mid.x + dx, mid.y + dy);
	}
}

void CompassOverlay::Render()
{
	try
	{
	if (!G::ShowPathingTrails || !G::ShowCompassOverlay)
		return;
	if (G::HideOutOfGameplay && G::NexusLink && !G::NexusLink->IsGameplay)
		return;
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return;

	const MumbleContext* ctx = Ctx();
	if (!ctx || ctx->mapId == 0)
		return;

	PathingTrails::Update(ctx->mapId);
	PathingTrails::TickMarkerBehaviors();

	const bool mapOpen = (ctx->uiState & static_cast<uint32_t>(UiStateBits::MapOpen)) != 0;
	if (G::HideWhenMapOpen && mapOpen)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	/* Match WorldOverlay - ImGui draw-list space only. Mixing Nexus sizes on
	   Windows DPI builds misplaces compass trails. */
	const float screenW = io.DisplaySize.x;
	const float screenH = io.DisplaySize.y;
	if (screenW < 64.f || screenH < 64.f)
		return;

	CompassLayout lay{};
	if (!BuildLayout(ctx, screenW, screenH, lay))
		return;

	const float centerX = ctx->mapCenterX;
	const float centerY = ctx->mapCenterY;
	const float mapScale = ctx->mapScale;
	if (!std::isfinite(centerX) || !std::isfinite(centerY) || !std::isfinite(mapScale))
		return;

	const bool rotateOn =
		(ctx->uiState & static_cast<uint32_t>(UiStateBits::CompassRotation)) != 0;
	const float rot = ctx->compassRotation;

	auto ToScreen = [&](float cx, float cy) -> ImVec2
	{
		return ContinentToCompass(cx, cy, centerX, centerY, mapScale, rotateOn, rot, lay.mid);
	};

	auto InCompass = [&](ImVec2 p) -> bool
	{
		return p.x >= lay.min.x - 4.f && p.x <= lay.max.x + 4.f &&
			p.y >= lay.min.y - 4.f && p.y <= lay.max.y + 4.f;
	};

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;

	PathingTrails::BeginFrame();
	dl->PushClipRect(lay.min, lay.max, true);

	const std::vector<PathingTrails::Trail> trails = PathingTrails::CurrentTrails();
	for (const PathingTrails::Trail& tr : trails)
	{
		if (tr.points.size() < 2 || !tr.minimapVisible)
			continue;

		const uint32_t argb = tr.color;
		int a = static_cast<int>((argb >> 24) & 0xFFu);
		int r = static_cast<int>((argb >> 16) & 0xFFu);
		int g = static_cast<int>((argb >> 8) & 0xFFu);
		int b = static_cast<int>(argb & 0xFFu);
		if (r > 245 && g > 245 && b > 245)
		{
			r = 0; g = 220; b = 255;
		}
		a = std::clamp(static_cast<int>(a * tr.alpha), 40, 230);
		const ImU32 col = IM_COL32(r, g, b, a);
		/* Pack trailScale only - same baseline as world GPS (no edition bias). */
		const float thickness = std::clamp(2.6f * tr.trailScale * G::WorldTrailWidth, 1.6f, 6.0f);

		const size_t step = (tr.points.size() > 160) ? 2u : 1u;
		size_t start = 0;
		while (start < tr.points.size() &&
			(!std::isfinite(tr.points[start].x) || !std::isfinite(tr.points[start].y)))
			++start;
		if (start >= tr.points.size())
			continue;
		ImVec2 prev = ToScreen(tr.points[start].x, tr.points[start].y);
		bool prevOk = InCompass(prev);
		float prevCx = tr.points[start].x;
		float prevCy = tr.points[start].y;
		for (size_t i = start + step; i < tr.points.size(); i += step)
		{
			if (!std::isfinite(tr.points[i].x) || !std::isfinite(tr.points[i].y))
			{
				/* TacO section break - do not stitch to the next segment. */
				prevOk = false;
				continue;
			}
			/* Section break / bad stitch - TacO (0,0,0) gaps become huge jumps. */
			const float cdx = tr.points[i].x - prevCx;
			const float cdy = tr.points[i].y - prevCy;
			ImVec2 cur = ToScreen(tr.points[i].x, tr.points[i].y);
			if (!prevOk || (cdx * cdx + cdy * cdy > (2500.f * 2500.f)))
			{
				prev = cur;
				prevOk = true;
				prevCx = tr.points[i].x;
				prevCy = tr.points[i].y;
				continue;
			}
			dl->AddLine(prev, cur, col, thickness);
			prev = cur;
			prevOk = true;
			prevCx = tr.points[i].x;
			prevCy = tr.points[i].y;
		}
	}

	/* Trail Tools draft - WYSIWYG Looks (texture/tint/scale). */
	if (TrailToolsDetail::AnyAuthoringPadOpen() && TrailToolsDetail::gDraft.previewEnabled)
	{
		PathingDetail::Rects rects{};
		bool haveRects = false;
		{
			std::lock_guard<std::mutex> lock(PathingDetail::gMutex);
			auto it = PathingDetail::gRects.find(ctx->mapId);
			if (it != PathingDetail::gRects.end() && it->second.valid)
			{
				rects = it->second;
				haveRects = true;
			}
		}
		if (!haveRects)
		{
			/* Pathing may not have loaded this map yet - fetch rects once async. */
			static std::atomic<uint32_t> sRectFetchMap{0};
			const uint32_t want = ctx->mapId;
			uint32_t expected = 0;
			if (sRectFetchMap.compare_exchange_strong(expected, want))
			{
				std::thread([want]() {
					PathingDetail::Rects r{};
					if (PathingDetail::FetchMapRects(want, r) && r.valid)
					{
						std::lock_guard<std::mutex> lock(PathingDetail::gMutex);
						PathingDetail::gRects[want] = r;
					}
					sRectFetchMap.store(0, std::memory_order_release);
				}).detach();
			}
		}
		else
		{
			TrailToolsPreviewCompass::Draw(ctx->mapId, dl,
				[&](float wx, float wz, float& cx, float& cy) -> bool {
					if (!std::isfinite(wx) || !std::isfinite(wz))
						return false;
					PathingDetail::WorldToContinent(rects, wx, wz, cx, cy);
					return true;
				},
				[&](float cx, float cy) { return ToScreen(cx, cy); },
				[&](ImVec2 p) { return InCompass(p); },
				mapScale);
		}
	}

	/* Marker culling in continent units ~ half compass * scale. */
	float scale = mapScale * 0.897f;
	if (!(scale > 1e-6f))
		scale = 1.f;
	const float halfW = (lay.max.x - lay.min.x) * 0.5f * scale;
	const float halfH = (lay.max.y - lay.min.y) * 0.5f * scale;
	const std::vector<PathingTrails::Marker> marks = PathingTrails::CurrentMarkersInBounds(
		centerX - halfW * 1.4f, centerY - halfH * 1.4f,
		centerX + halfW * 1.4f, centerY + halfH * 1.4f);

	int drawn = 0;
	constexpr int kMaxMarkers = 140;
	for (const PathingTrails::Marker& m : marks)
	{
		if (drawn >= kMaxMarkers)
			break;
		/* MarkerShownOnCompass already forced Barefoot/Mount shortcuts on. */
		if (!m.minimapVisible)
			continue;
		ImVec2 p = ToScreen(m.pos.x, m.pos.y);
		if (!InCompass(p))
			continue;

		const uint32_t argb = m.color;
		int a = static_cast<int>((argb >> 24) & 0xFFu);
		int r = static_cast<int>((argb >> 16) & 0xFFu);
		int g = static_cast<int>((argb >> 8) & 0xFFu);
		int b = static_cast<int>(argb & 0xFFu);
		a = std::clamp(static_cast<int>(a * m.alpha), 50, 240);

		/* Blish: drawScale = 1/scale for zoom; keep readable on small compass. */
		float sz = m.mapDisplaySize * m.iconSize / std::max(1.f, scale * 0.15f);
		sz *= std::clamp(G::CompassMarkerScale, 0.5f, 3.f);
		sz = std::clamp(sz, 4.f, 28.f);

		Texture_t* tex = nullptr;
		if (m.iconId[0] && G::API && G::API->Textures_Get)
		{
			tex = G::API->Textures_Get(m.iconId);
			if (tex && !tex->Resource)
				tex = nullptr;
		}
		if (tex)
		{
			const float h = sz * 0.5f;
			dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource),
				ImVec2(p.x - h, p.y - h), ImVec2(p.x + h, p.y + h),
				ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, a));
		}
		else
		{
			dl->AddCircleFilled(p, std::max(2.5f, sz * 0.28f), IM_COL32(r, g, b, a), 10);
		}
		++drawn;
	}

	dl->PopClipRect();
	}
	catch (...)
	{
	}
}
