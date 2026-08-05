#include "TrailToolsPreviewCompass.h"

#include "TrailToolsDraftStyle.h"
#include "TrailToolsShared.h"
#include "Globals.h"

#include <algorithm>
#include <cmath>

void TrailToolsPreviewCompass::Draw(
	uint32_t mapId,
	ImDrawList* dl,
	const std::function<bool(float wx, float wz, float& cx, float& cy)>& worldToCont,
	const std::function<ImVec2(float cx, float cy)>& toScreen,
	const std::function<bool(ImVec2)>& inCompass,
	float mapScale)
{
	using namespace TrailToolsDetail;
	if (!TrailToolsDetail::AnyAuthoringPadOpen() || !gDraft.previewEnabled || !dl)
		return;

	TrailToolsDraftStyle::BeginFrame();

	if (gDraft.active.points.size() >= 2 && gDraft.active.mapId == mapId)
	{
		const auto sty = TrailToolsDraftStyle::ResolveTrail();
		const uint32_t argb = sty.color ? sty.color : 0xFFFF40DCu;
		int a = static_cast<int>((argb >> 24) & 0xFFu);
		a = std::clamp(static_cast<int>(a * sty.alpha), 80, 240);
		const ImU32 col = IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, a);
		float thickness = std::clamp(2.6f * sty.trailScale * G::WorldTrailWidth, 1.6f, 6.f);

		bool prevOk = false;
		ImVec2 prev{};
		for (const auto& w : gDraft.active.points)
		{
			if (w.x == 0.f && w.y == 0.f && w.z == 0.f)
			{
				prevOk = false;
				continue;
			}
			float cx = 0.f, cy = 0.f;
			if (!worldToCont(w.x, w.z, cx, cy))
			{
				prevOk = false;
				continue;
			}
			ImVec2 cur = toScreen(cx, cy);
			if (prevOk && inCompass(prev) && inCompass(cur))
				dl->AddLine(prev, cur, col, thickness);
			prev = cur;
			prevOk = true;
		}
	}

	float scale = mapScale * 0.897f;
	if (!(scale > 1e-6f))
		scale = 1.f;

	for (const DraftPoi& poi : gDraft.pois)
	{
		if (poi.mapId != mapId)
			continue;
		float cx = 0.f, cy = 0.f;
		if (!worldToCont(poi.x, poi.z, cx, cy))
			continue;
		ImVec2 p = toScreen(cx, cy);
		if (!inCompass(p))
			continue;

		PathingTrails::Marker m = TrailToolsDraftStyle::BuildDraftMarker(poi);
		const uint32_t argb = m.color;
		int a = static_cast<int>((argb >> 24) & 0xFFu);
		int r = static_cast<int>((argb >> 16) & 0xFFu);
		int g = static_cast<int>((argb >> 8) & 0xFFu);
		int b = static_cast<int>(argb & 0xFFu);
		a = std::clamp(static_cast<int>(a * m.alpha), 50, 240);

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
			dl->AddCircleFilled(p, std::max(2.5f, sz * 0.28f), IM_COL32(r, g, b, a), 10);
	}
}
