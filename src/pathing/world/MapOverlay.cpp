#include "MapOverlay.h"

#include "Globals.h"
#include "PathingTrails.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

/* Fullscreen map trails when MapOpen.
   Continent points match the compass (same WorldToContinent). Screen scale is
   TacO big-map: mapScale / identity uisz — not Blish's *0.897 (HUD quirk) and
   not a live mapCenter↔avatar align (that slides on pan). */

namespace
{
	const MumbleContext* Ctx()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return nullptr;
		return reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	}

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
	float GameUiScale(int uiSize)
	{
		switch (uiSize)
		{
		case 0:  return 0.9f;
		case 2:  return 1.111f;
		case 3:  return 1.224f;
		default: return 1.f;
		}
	}

	ImVec2 ContinentToMap(float continentX, float continentY,
		float mapCenterX, float mapCenterY, float pixelScale, ImVec2 mid)
	{
		float scale = pixelScale;
		if (!(scale > 1e-6f) || !std::isfinite(scale))
			scale = 1.f;
		const float dx = (continentX - mapCenterX) / scale;
		const float dy = (continentY - mapCenterY) / scale;
		return ImVec2(mid.x + dx, mid.y + dy);
	}
}

void MapOverlay::Render()
{
	try
	{
	if (!G::ShowPathingTrails || !G::ShowMapTrails)
		return;
	if (G::HideOutOfGameplay && G::NexusLink && !G::NexusLink->IsGameplay)
		return;
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return;

	const MumbleContext* ctx = Ctx();
	if (!ctx || ctx->mapId == 0)
		return;

	const bool mapOpen = (ctx->uiState & static_cast<uint32_t>(UiStateBits::MapOpen)) != 0;
	if (!mapOpen)
		return;

	/* Rebuild polylines if a prior bad WorldToContinent Y build was cached. */
	static bool sReloaded = false;
	if (!sReloaded)
	{
		sReloaded = true;
		PathingTrails::NotifyVisibilityFilterChanged();
	}

	PathingTrails::Update(ctx->mapId);

	const ImGuiIO& io = ImGui::GetIO();
	const float screenW = io.DisplaySize.x;
	const float screenH = io.DisplaySize.y;
	if (screenW < 64.f || screenH < 64.f)
		return;

	const ImVec2 mid(screenW * 0.5f, screenH * 0.5f);
	const ImVec2 clipMin(4.f, 4.f);
	const ImVec2 clipMax(screenW - 4.f, screenH - 4.f);

	const float centerX = ctx->mapCenterX;
	const float centerY = ctx->mapCenterY;
	const float mapScale = ctx->mapScale;
	if (!std::isfinite(centerX) || !std::isfinite(centerY) || !std::isfinite(mapScale))
		return;

	/* TacO BuildTransformationMatrix: /mapScale * GetUIScale()
	   ⇒ pixel divisor = mapScale / uisz. Keeps zoom locked to terrain. */
	const float pixelScale = mapScale / std::max(0.1f, GameUiScale(ParseUiSize()));

	auto ToScreen = [&](float cx, float cy) -> ImVec2
	{
		return ContinentToMap(cx, cy, centerX, centerY, pixelScale, mid);
	};

	auto InView = [&](ImVec2 p) -> bool
	{
		return p.x >= clipMin.x - 8.f && p.x <= clipMax.x + 8.f &&
			p.y >= clipMin.y - 8.f && p.y <= clipMax.y + 8.f;
	};

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;

	PathingTrails::BeginFrame();
	dl->PushClipRect(clipMin, clipMax, true);

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
		const float thickness = std::clamp(2.6f * tr.trailScale * G::WorldTrailWidth, 1.6f, 6.0f);

		const size_t step = (tr.points.size() > 160) ? 2u : 1u;
		size_t start = 0;
		while (start < tr.points.size() &&
			(!std::isfinite(tr.points[start].x) || !std::isfinite(tr.points[start].y)))
			++start;
		if (start >= tr.points.size())
			continue;
		ImVec2 prev = ToScreen(tr.points[start].x, tr.points[start].y);
		bool prevOk = InView(prev);
		float prevCx = tr.points[start].x;
		float prevCy = tr.points[start].y;
		constexpr float kSoftBridgeC2 = 45.f * 45.f;
		for (size_t i = start + step; i < tr.points.size(); i += step)
		{
			if (!std::isfinite(tr.points[i].x) || !std::isfinite(tr.points[i].y))
			{
				size_t j = i + 1;
				while (j < tr.points.size() &&
					(!std::isfinite(tr.points[j].x) || !std::isfinite(tr.points[j].y)))
					++j;
				if (j >= tr.points.size() || !prevOk)
				{
					prevOk = false;
					continue;
				}
				const float bdx = tr.points[j].x - prevCx;
				const float bdy = tr.points[j].y - prevCy;
				if (bdx * bdx + bdy * bdy > kSoftBridgeC2)
				{
					prevOk = false;
					continue;
				}
				i = j;
			}
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

	const float halfW = (clipMax.x - clipMin.x) * 0.5f * pixelScale;
	const float halfH = (clipMax.y - clipMin.y) * 0.5f * pixelScale;
	const std::vector<PathingTrails::Marker> marks = PathingTrails::CurrentMarkersInBounds(
		centerX - halfW * 1.1f, centerY - halfH * 1.1f,
		centerX + halfW * 1.1f, centerY + halfH * 1.1f);

	int drawn = 0;
	constexpr int kMaxMarkers = 220;
	for (const PathingTrails::Marker& m : marks)
	{
		if (drawn >= kMaxMarkers)
			break;
		if (!m.minimapVisible)
			continue;
		ImVec2 p = ToScreen(m.pos.x, m.pos.y);
		if (!InView(p))
			continue;

		const uint32_t argb = m.color;
		int a = static_cast<int>((argb >> 24) & 0xFFu);
		int r = static_cast<int>((argb >> 16) & 0xFFu);
		int g = static_cast<int>((argb >> 8) & 0xFFu);
		int b = static_cast<int>(argb & 0xFFu);
		a = std::clamp(static_cast<int>(a * m.alpha), 50, 240);

		float sz = m.mapDisplaySize * m.iconSize / std::max(1.f, pixelScale * 0.15f);
		sz *= std::clamp(G::CompassMarkerScale, 0.5f, 3.f);
		sz = std::clamp(sz, 4.f, 36.f);

		Texture_t* tex = nullptr;
		if (m.iconId[0] && G::API && G::API->Textures_Get)
		{
			tex = G::API->Textures_Get(m.iconId);
			if (tex && !tex->Resource)
				tex = nullptr;
		}
		if (tex)
		{
			const float half = sz * 0.5f;
			dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource),
				ImVec2(p.x - half, p.y - half), ImVec2(p.x + half, p.y + half),
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
