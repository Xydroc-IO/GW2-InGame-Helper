#include "TrailToolsPreview.h"

#include "TrailToolsShared.h"
#include "Globals.h"
#include "WorldGpsMath.h"

#include "imgui/imgui.h"

#include <cmath>

/* Compass draft is drawn inside CompassOverlay (shared layout math).
   World draft: trails + marker dots via WorldGpsMath. */

void TrailToolsPreview::RenderCompass()
{
	/* no-op — see CompassOverlay */
}

void TrailToolsPreview::RenderWorld()
{
	using namespace TrailToolsDetail;
	if (!G::ShowTrailTools || !gDraft.previewEnabled)
		return;
	if (!G::Mumble || G::Mumble->uiTick == 0)
		return;
	const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	if (!ctx || ctx->mapId == 0)
		return;
	if (G::HideWhenMapOpen && (ctx->uiState & static_cast<uint32_t>(UiStateBits::MapOpen)))
		return;
	if (G::HideOutOfGameplay && G::NexusLink && !G::NexusLink->IsGameplay)
		return;

	using WorldGpsMath::Mat4;
	using WorldGpsMath::Vec3;
	const ImGuiIO& io = ImGui::GetIO();
	const float screenW = io.DisplaySize.x;
	const float screenH = io.DisplaySize.y;
	Mat4 viewProj{};
	Vec3 cam{};
	if (!WorldGpsMath::BuildViewProj(screenW, screenH, viewProj, cam))
		return;
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;

	constexpr ImU32 kDraftCol = IM_COL32(255, 64, 220, 220);
	constexpr ImU32 kMarkerCol = IM_COL32(255, 200, 40, 240);

	if (gDraft.active.points.size() >= 2 && gDraft.active.mapId == ctx->mapId)
	{
		bool havePrev = false;
		float psx = 0.f, psy = 0.f;
		for (const auto& w : gDraft.active.points)
		{
			if (w.x == 0.f && w.y == 0.f && w.z == 0.f)
			{
				havePrev = false;
				continue;
			}
			Vec3 world{ w.x, w.y, w.z };
			float sx = 0.f, sy = 0.f;
			if (!WorldGpsMath::WorldToScreen(world, viewProj, screenW, screenH, sx, sy))
			{
				havePrev = false;
				continue;
			}
			if (havePrev)
				dl->AddLine(ImVec2(psx, psy), ImVec2(sx, sy), kDraftCol, 2.8f);
			psx = sx;
			psy = sy;
			havePrev = true;
		}
	}

	for (const DraftPoi& p : gDraft.pois)
	{
		if (p.mapId != ctx->mapId)
			continue;
		Vec3 world{ p.x, p.y + 1.5f, p.z };
		float sx = 0.f, sy = 0.f;
		if (!WorldGpsMath::WorldToScreen(world, viewProj, screenW, screenH, sx, sy))
			continue;
		dl->AddCircleFilled(ImVec2(sx, sy), 7.f, kMarkerCol, 12);
		dl->AddCircle(ImVec2(sx, sy), 7.f, IM_COL32(40, 20, 0, 220), 12, 1.5f);
	}
}
