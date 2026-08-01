#include "CompassOverlay.h"

#include "Globals.h"
#include "TekkitTrails.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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

	/* Blish FlatMap GetOffset — soft padding around the compass widget. */
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
		(void)padW;
		(void)padH;

		out.min = ImVec2(x1, y1);
		out.max = ImVec2(x2, y2);
		out.mid = ImVec2((x1 + x2) * 0.5f, (y1 + y2) * 0.5f);
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
	if (!G::ShowTekkitTrails || !G::ShowCompassOverlay)
		return;
	if (G::HideOutOfGameplay && G::NexusLink && !G::NexusLink->IsGameplay)
		return;
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return;

	const MumbleContext* ctx = Ctx();
	if (!ctx || ctx->mapId == 0)
		return;

	TekkitTrails::Update(ctx->mapId);

	const bool mapOpen = (ctx->uiState & static_cast<uint32_t>(UiStateBits::MapOpen)) != 0;
	if (G::HideWhenMapOpen && mapOpen)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	float screenW = io.DisplaySize.x;
	float screenH = io.DisplaySize.y;
	/* Prefer Nexus viewport when it matches the game client (avoids DPI drift). */
	if (G::NexusLink && G::NexusLink->Width > 64 && G::NexusLink->Height > 64)
	{
		const float nw = static_cast<float>(G::NexusLink->Width);
		const float nh = static_cast<float>(G::NexusLink->Height);
		/* Only trust Nexus sizes close to ImGui display (Wine/DPI safe). */
		if (std::fabs(nw - screenW) < screenW * 0.08f &&
			std::fabs(nh - screenH) < screenH * 0.08f)
		{
			screenW = nw;
			screenH = nh;
		}
	}
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

	TekkitTrails::BeginFrame();
	dl->PushClipRect(lay.min, lay.max, true);

	const std::vector<TekkitTrails::Trail> trails = TekkitTrails::CurrentTrails();
	for (const TekkitTrails::Trail& tr : trails)
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
		const float thickness = std::clamp(2.0f * tr.trailScale, 1.2f, 4.0f);

		const size_t step = (tr.points.size() > 160) ? 2u : 1u;
		ImVec2 prev = ToScreen(tr.points[0].x, tr.points[0].y);
		bool prevOk = InCompass(prev);
		for (size_t i = step; i < tr.points.size(); i += step)
		{
			ImVec2 cur = ToScreen(tr.points[i].x, tr.points[i].y);
			const bool curOk = InCompass(cur);
			if (prevOk || curOk)
				dl->AddLine(prev, cur, col, thickness);
			prev = cur;
			prevOk = curOk;
		}
	}

	/* Marker culling in continent units ≈ half compass * scale. */
	float scale = mapScale * 0.897f;
	if (!(scale > 1e-6f))
		scale = 1.f;
	const float halfW = (lay.max.x - lay.min.x) * 0.5f * scale;
	const float halfH = (lay.max.y - lay.min.y) * 0.5f * scale;
	const std::vector<TekkitTrails::Marker> marks = TekkitTrails::CurrentMarkersInBounds(
		centerX - halfW * 1.4f, centerY - halfH * 1.4f,
		centerX + halfW * 1.4f, centerY + halfH * 1.4f);

	int drawn = 0;
	constexpr int kMaxMarkers = 140;
	for (const TekkitTrails::Marker& m : marks)
	{
		if (drawn >= kMaxMarkers)
			break;
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
		sz = std::clamp(sz, 5.f, 18.f);

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
