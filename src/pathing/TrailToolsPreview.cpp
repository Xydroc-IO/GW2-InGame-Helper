#include "TrailToolsPreview.h"

#include "TrailToolsDraftStyle.h"
#include "TrailToolsShared.h"
#include "Globals.h"
#include "WorldGpsD3d.h"
#include "WorldGpsImgui.h"
#include "WorldGpsMath.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

	TrailToolsDraftStyle::BeginFrame();

	using WorldGpsMath::Mat4;
	using WorldGpsMath::Vec3;
	const ImGuiIO& io = ImGui::GetIO();
	const float screenW = io.DisplaySize.x;
	const float screenH = io.DisplaySize.y;
	Mat4 viewProj{};
	Vec3 cam{};
	if (!WorldGpsMath::BuildViewProj(screenW, screenH, viewProj, cam))
		return;

	const Vec3 avatar{
		G::Mumble->fAvatarPosition[0],
		G::Mumble->fAvatarPosition[1],
		G::Mumble->fAvatarPosition[2]
	};
	const float thickness = std::clamp(G::WorldTrailWidth, 0.5f, 4.f);
	const float maxDist = std::max(80.f, G::WorldTrailMaxDist);

	if (gDraft.active.points.size() >= 2 && gDraft.active.mapId == ctx->mapId)
	{
		PathingTrails::WorldSnippet snip = TrailToolsDraftStyle::BuildActiveSnippet();
		if (snip.points.size() >= 2 && WorldGpsD3d::Available())
		{
			std::vector<PathingTrails::WorldSnippet> one{ snip };
			WorldGpsD3d::DrawTrails(viewProj, cam, avatar, maxDist, thickness, one, nullptr);
		}
		else if (snip.points.size() >= 2)
		{
			/* ImGui fallback when D3D GPS unavailable. */
			ImDrawList* dl = ImGui::GetBackgroundDrawList();
			if (dl)
			{
				const uint32_t argb = snip.color ? snip.color : 0xFFFF40DCu;
				const ImU32 col = IM_COL32(
					(argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF,
					std::clamp(int((argb >> 24) & 0xFF), 80, 240));
				bool havePrev = false;
				float psx = 0.f, psy = 0.f;
				for (const auto& w : snip.points)
				{
					if (w.x == 0.f && w.y == 0.f && w.z == 0.f)
					{
						havePrev = false;
						continue;
					}
					float sx = 0.f, sy = 0.f;
					if (!WorldGpsMath::WorldToScreen(
						Vec3{ w.x, w.y, w.z }, viewProj, screenW, screenH, sx, sy))
					{
						havePrev = false;
						continue;
					}
					if (havePrev)
						dl->AddLine(ImVec2(psx, psy), ImVec2(sx, sy), col, 2.8f);
					psx = sx;
					psy = sy;
					havePrev = true;
				}
			}
		}
	}

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;
	std::vector<PathingTrails::Marker> marks;
	marks.reserve(gDraft.pois.size());
	for (const DraftPoi& p : gDraft.pois)
	{
		if (p.mapId != ctx->mapId)
			continue;
		marks.push_back(TrailToolsDraftStyle::BuildDraftMarker(p));
	}
	if (!marks.empty())
		WorldGpsImgui::DrawMarkers(dl, viewProj, screenW, screenH, avatar, marks);
}
