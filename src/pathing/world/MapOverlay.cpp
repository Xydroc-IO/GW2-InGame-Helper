#include "MapOverlay.h"

#include "Globals.h"
#include "PathingTrails.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <d3d11.h>
#include <dxgi.h>

/* Fullscreen map trails when MapOpen — same continent→pixel math as Blish
   FlatMap / our working CompassOverlay (mapScale * 0.897, screen mid,
   mapCenter, no rotation, no avatar align). */

namespace
{
	const MumbleContext* Ctx()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return nullptr;
		return reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	}

	/* Prefer D3D backbuffer size when it matches ImGui space; otherwise DisplaySize.
	   Wine/DPI can leave DisplaySize briefly wrong while the swapchain is right. */
	void OverlaySize(float& w, float& h)
	{
		const ImGuiIO& io = ImGui::GetIO();
		w = io.DisplaySize.x;
		h = io.DisplaySize.y;
		if (!G::API || !G::API->SwapChain)
			return;
		auto* swap = static_cast<IDXGISwapChain*>(G::API->SwapChain);
		ID3D11Texture2D* back = nullptr;
		if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D),
				reinterpret_cast<void**>(&back))) || !back)
			return;
		D3D11_TEXTURE2D_DESC td{};
		back->GetDesc(&td);
		back->Release();
		if (td.Width < 64 || td.Height < 64)
			return;
		const float bw = static_cast<float>(td.Width);
		const float bh = static_cast<float>(td.Height);
		/* Only adopt backbuffer when aspect matches — avoids letterbox skew. */
		if (w > 64.f && h > 64.f)
		{
			const float aDisp = w / h;
			const float aBack = bw / bh;
			if (std::fabs(aDisp - aBack) > 0.02f)
				return;
		}
		w = bw;
		h = bh;
	}

	/* Same scale factor as CompassOverlay::ContinentToCompass / Blish FlatMap. */
	ImVec2 ContinentToMap(float continentX, float continentY,
		float mapCenterX, float mapCenterY, float mapScale, ImVec2 mid)
	{
		float scale = mapScale * 0.897f;
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

	PathingTrails::Update(ctx->mapId);

	float screenW = 0.f, screenH = 0.f;
	OverlaySize(screenW, screenH);
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

	auto ToScreen = [&](float cx, float cy) -> ImVec2
	{
		return ContinentToMap(cx, cy, centerX, centerY, mapScale, mid);
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
		/* Continent units — same soft-bridge budget as CompassOverlay. */
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

	float scale = mapScale * 0.897f;
	if (!(scale > 1e-6f))
		scale = 1.f;
	const float halfW = (clipMax.x - clipMin.x) * 0.5f * scale;
	const float halfH = (clipMax.y - clipMin.y) * 0.5f * scale;
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

		float sz = m.mapDisplaySize * m.iconSize / std::max(1.f, scale * 0.15f);
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
