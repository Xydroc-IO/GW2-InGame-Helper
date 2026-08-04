#include "WorldOverlay.h"

#include "Globals.h"
#include "PathingTrails.h"
#include "WorldGpsD3d.h"
#include "WorldGpsImgui.h"
#include "WorldGpsMath.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <d3d11.h>
#include <dxgi.h>

/* Orchestrator: cache nearby GPS → D3D world ribbons only (Blish-style).
   Markers stay on ImGui. No ImGui trail billboards. */

namespace
{
	const MumbleContext* Ctx()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return nullptr;
		return reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	}

	bool BackbufferSize(float& outW, float& outH)
	{
		if (!G::API || !G::API->SwapChain)
			return false;
		auto* swap = static_cast<IDXGISwapChain*>(G::API->SwapChain);
		ID3D11Texture2D* back = nullptr;
		if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D),
				reinterpret_cast<void**>(&back))) || !back)
			return false;
		D3D11_TEXTURE2D_DESC td{};
		back->GetDesc(&td);
		back->Release();
		if (td.Width < 64 || td.Height < 64)
			return false;
		outW = static_cast<float>(td.Width);
		outH = static_cast<float>(td.Height);
		return true;
	}
}

void WorldOverlay::Shutdown()
{
	WorldGpsD3d::Shutdown();
}

void WorldOverlay::Render()
{
	try
	{
	if (!G::ShowPathingTrails)
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

	if (!G::ShowWorldTrails && !PathingTrails::HasSearchGuideActive())
		return;
	if (G::HideWhenMapOpen && (ctx->uiState & static_cast<uint32_t>(UiStateBits::MapOpen)))
		return;

	const float ax = G::Mumble->fAvatarPosition[0];
	const float ay = G::Mumble->fAvatarPosition[1];
	const float az = G::Mumble->fAvatarPosition[2];
	if (!WorldGpsMath::ReasonablePos(ax, ay, az))
		return;
	if (ax * ax + ay * ay + az * az < 0.25f)
		return;

	/* Match D3D viewport aspect to projection — ImGui DisplaySize can differ
	   under Wine/DPI and shove ribbons off the real path. */
	float screenW = 0.f, screenH = 0.f;
	if (!BackbufferSize(screenW, screenH))
	{
		const ImGuiIO& io = ImGui::GetIO();
		screenW = io.DisplaySize.x;
		screenH = io.DisplaySize.y;
	}
	if (screenW < 64.f || screenH < 64.f || !std::isfinite(screenW) || !std::isfinite(screenH))
		return;

	using WorldGpsMath::Mat4;
	using WorldGpsMath::Vec3;

	Mat4 viewProj{};
	Vec3 cam{};
	static Mat4 sLastViewProj{};
	static bool sHaveViewProj = false;
	if (!WorldGpsMath::BuildViewProj(screenW, screenH, viewProj, cam))
	{
		if (!sHaveViewProj)
			return;
		viewProj = sLastViewProj;
	}
	else
	{
		sLastViewProj = viewProj;
		sHaveViewProj = true;
	}

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	PathingTrails::BeginFrame();

	const float maxDist = G::LadyWpOnly
		? std::clamp(std::max(G::WorldTrailMaxDist, 200.f), 160.f, 320.f)
		: std::clamp(G::WorldTrailMaxDist, 40.f, 200.f);
	const float thickness = std::clamp(G::WorldTrailWidth, 0.5f, 4.0f);
	const Vec3 avatar{ax, ay, az};

	static std::vector<PathingTrails::WorldSnippet> sNearCache;
	static std::vector<PathingTrails::Marker> sMarkerCache;
	static PathingTrails::WorldSnippet sGuideCache;
	static float sCacheAx = 0.f, sCacheAy = 0.f, sCacheAz = 0.f;
	static uint32_t sGpsMap = 0;
	static uint64_t sGpsContent = 0;

	const uint64_t content = PathingTrails::ContentRevision();
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
	const float refreshM = G::LadyWpOnly ? 3.5f : 5.5f;
	const bool movedFar = (mdx * mdx + mdy * mdy + mdz * mdz) > (refreshM * refreshM);
	const bool needRefresh = (sGpsContent != content) || sNearCache.empty() || movedFar;

	if (G::ShowWorldTrails && needRefresh)
	{
		std::vector<PathingTrails::WorldSnippet> snips;
		std::vector<PathingTrails::Marker> marks;
		if (PathingTrails::TryNearbyWorldGps(ax, ay, az, maxDist, snips, marks))
		{
			const bool got = !snips.empty() || !marks.empty();
			if (got)
			{
				/* Sticky merge — keep prior ribbons that briefly missed a sample
				   so trails do not blink disappear/reappear at range edges. */
				const float stickM = std::max(maxDist * 3.6f, 480.f);
				const float stick2 = stickM * stickM;
				auto nearD2 = [&](const PathingTrails::WorldSnippet& s) -> float {
					float best = 1.0e30f;
					const size_t n = s.points.size();
					const size_t step = std::max<size_t>(1, n / 48);
					for (size_t i = 0; i < n; i += step)
					{
						const auto& p = s.points[i];
						if (!std::isfinite(p.x))
							continue;
						const float dx = ax - p.x;
						const float dy = ay - p.y;
						const float dz = az - p.z;
						const float d = dx * dx + dy * dy * 0.25f + dz * dz;
						if (d < best)
							best = d;
					}
					return best;
				};
				auto sameKey = [](const PathingTrails::WorldSnippet& a,
					const PathingTrails::WorldSnippet& b) -> bool {
					if (a.label[0] && b.label[0])
						return std::strncmp(a.label, b.label, sizeof(a.label)) == 0;
					return a.color == b.color &&
						std::strncmp(a.textureId, b.textureId, sizeof(a.textureId)) == 0;
				};

				std::vector<PathingTrails::WorldSnippet> merged = std::move(snips);
				for (const auto& old : sNearCache)
				{
					bool replaced = false;
					for (const auto& n : merged)
					{
						if (sameKey(old, n))
						{
							replaced = true;
							break;
						}
					}
					if (replaced)
						continue;
					if (nearD2(old) <= stick2)
						merged.push_back(old);
					if (merged.size() >= 20)
						break;
				}
				sNearCache = std::move(merged);
				sMarkerCache = std::move(marks);
				sCacheAx = ax;
				sCacheAy = ay;
				sCacheAz = az;
				sGpsContent = content;
			}
			else if (!PathingTrails::HasDrawableWorldGps())
			{
				sNearCache.clear();
				sMarkerCache.clear();
				sCacheAx = ax;
				sCacheAy = ay;
				sCacheAz = az;
				sGpsContent = content;
			}
			else if (!sNearCache.empty())
			{
				sGpsContent = content;
			}
		}
	}

	if (PathingTrails::HasSearchGuideActive())
	{
		if (PathingTrails::HasSearchGuide())
		{
			const PathingTrails::WorldSnippet guide = PathingTrails::SearchGuideWorldSnippet();
			if (guide.points.size() >= 2)
				sGuideCache = guide;
		}
	}
	else
		sGuideCache = {};

	float drawDist = std::max(maxDist * 3.2f, 420.f);
	if (G::LadyWpOnly)
		drawDist = std::max(maxDist * 3.5f, 650.f);

	const PathingTrails::WorldSnippet* guidePtr =
		(sGuideCache.points.size() >= 2) ? &sGuideCache : nullptr;

	/* D3D world ribbons only — no ImGui trail billboards. */
	if (WorldGpsD3d::Available())
	{
		static const std::vector<PathingTrails::WorldSnippet> kEmpty;
		if (guidePtr)
			WorldGpsD3d::DrawTrails(viewProj, cam, avatar, 2500.f, thickness,
				kEmpty, guidePtr);
		if (G::ShowWorldTrails && !sNearCache.empty())
			WorldGpsD3d::DrawTrails(viewProj, cam, avatar, drawDist, thickness,
				sNearCache, nullptr);
	}

	if (G::ShowWorldTrails && dl)
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float markW = io.DisplaySize.x;
		const float markH = io.DisplaySize.y;
		Mat4 markVp = viewProj;
		Vec3 markCam{};
		if (markW >= 64.f && markH >= 64.f)
			WorldGpsMath::BuildViewProj(markW, markH, markVp, markCam);
		WorldGpsImgui::DrawMarkers(dl, markVp, markW, markH, avatar, sMarkerCache);
	}
	}
	catch (...)
	{
	}
}
