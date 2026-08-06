#include "WorldGpsD3d.h"
#include "WorldGpsD3dInternal.h"

#include "Globals.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <dxgi.h>

using namespace WorldGpsD3dInternal;

bool WorldGpsD3dInternal::EnsureVB(UINT vertexCount)
{
	if (!gDev)
		return false;
	if (gVB && gVBCapacity >= vertexCount)
		return true;
	if (gVB)
	{
		gVB->Release();
		gVB = nullptr;
		gVBCapacity = 0;
	}
	const UINT cap = std::max(vertexCount, 8192u);
	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = cap * static_cast<UINT>(sizeof(Vertex));
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(gDev->CreateBuffer(&bd, nullptr, &gVB)))
		return false;
	gVBCapacity = cap;
	return true;
}

namespace
{
	using WorldGpsMath::Vec3;

	bool IsHeartTrail(const PathingTrails::WorldSnippet& snip)
	{
		if (snip.label[0] && std::strstr(snip.label, "heartpath"))
			return true;
		if (!snip.textureId[0])
			return false;
		return std::strstr(snip.textureId, "Line___Heart") != nullptr ||
			std::strstr(snip.textureId, "Heart_png") != nullptr;
	}

	bool ResamplePath(const std::vector<Vec3>& rawIn, std::vector<Vec3>& pts,
		std::vector<Vec3>& sides)
	{
		pts.clear();
		sides.clear();
		if (rawIn.size() < 2)
			return false;
		constexpr float kResample = 10.f;
		pts.reserve(rawIn.size() * 2);
		pts.push_back(rawIn[0]);
		for (size_t i = 0; i + 1 < rawIn.size(); ++i)
		{
			Vec3 a = rawIn[i], b = rawIn[i + 1], d = b - a;
			float len = std::sqrt(d.LengthSq());
			if (!(len > 0.08f) || !std::isfinite(len) || len > 160.f)
			{
				pts.push_back(b);
				continue;
			}
			const int steps = std::max(1, static_cast<int>(std::ceil(len / kResample)));
			for (int s = 1; s <= steps; ++s)
				pts.push_back(WorldGpsMath::Lerp3(a, b, static_cast<float>(s) / steps));
		}
		if (pts.size() < 2)
			return false;
		sides.resize(pts.size());
		float flip = 1.f;
		Vec3 lastSide{};
		for (size_t i = 0; i < pts.size(); ++i)
		{
			Vec3 pathDir = (i + 1 < pts.size())
				? (pts[i + 1] - pts[i]).Normalised()
				: (pts[i] - pts[i - 1]).Normalised();
			if (pathDir.LengthSq() < 0.5f)
				pathDir = {1.f, 0.f, 0.f};
			Vec3 side = pathDir.Cross(Vec3{0.f, 1.f, 0.f}).Normalised();
			if (side.LengthSq() < 0.5f)
			{
				side = pathDir.Cross(Vec3{0.f, 0.f, -1.f}).Normalised();
				if (side.LengthSq() < 0.5f)
					side = {1.f, 0.f, 0.f};
			}
			if (lastSide.LengthSq() > 0.5f && side.Dot(lastSide) < 0.f)
				flip = -flip;
			lastSide = side;
			sides[i] = side * flip;
		}
		for (size_t i = 1; i + 1 < sides.size(); ++i)
		{
			Vec3 s = (sides[i - 1] + sides[i] + sides[i + 1]).Normalised();
			if (s.LengthSq() > 0.5f)
				sides[i] = (s.Dot(sides[i]) < 0.f) ? s * -1.f : s;
		}
		return true;
	}

	void EmitRibbonRun(std::vector<Vertex>& out, const std::vector<Vec3>& rawIn,
		float halfW, float uvPeriod, float flowScroll,
		float cr, float cg, float cb, float baseA, float along0)
	{
		std::vector<Vec3> pts, sides;
		if (!ResamplePath(rawIn, pts, sides))
			return;
		float along = along0;
		for (size_t i = 0; i + 1 < pts.size(); ++i)
		{
			const float segLen = std::sqrt((pts[i + 1] - pts[i]).LengthSq());
			if (!(segLen > 0.05f))
				continue;
			const float v0 = -(along / uvPeriod) + flowScroll;
			const float v1 = -((along + segLen) / uvPeriod) + flowScroll;
			const Vec3 l0 = pts[i] - sides[i] * halfW;
			const Vec3 r0 = pts[i] + sides[i] * halfW;
			const Vec3 l1 = pts[i + 1] - sides[i + 1] * halfW;
			const Vec3 r1 = pts[i + 1] + sides[i + 1] * halfW;
			const Vertex verts[6] = {
				{l0.x, l0.y, l0.z, 0.f, v0, cr, cg, cb, baseA},
				{r0.x, r0.y, r0.z, 1.f, v0, cr, cg, cb, baseA},
				{l1.x, l1.y, l1.z, 0.f, v1, cr, cg, cb, baseA},
				{r0.x, r0.y, r0.z, 1.f, v0, cr, cg, cb, baseA},
				{r1.x, r1.y, r1.z, 1.f, v1, cr, cg, cb, baseA},
				{l1.x, l1.y, l1.z, 0.f, v1, cr, cg, cb, baseA},
			};
			out.insert(out.end(), verts, verts + 6);
			along += segLen;
			if (out.size() > 120000)
				return;
		}
	}

	void AppendRibbon(std::vector<Vertex>& out, const PathingTrails::WorldSnippet& snip,
		float thickness, bool bright, float flowScroll)
	{
		if (snip.points.size() < 2)
			return;
		const bool hearts = IsHeartTrail(snip);
		/* Hearts keep pack yellow tint and flow with the same ribbon path as arrows. */
		float halfW = WorldGpsMath::TrailHalfWidthM(snip.trailScale, thickness);
		if (hearts)
			halfW *= 1.5f;
		const float baseA = (bright ? 0.98f : 0.92f) *
			std::clamp(snip.alpha > 0.05f ? snip.alpha : 1.f, 0.f, 1.f);
		float cr = ((snip.color >> 16) & 0xFFu) / 255.f;
		float cg = ((snip.color >> 8) & 0xFFu) / 255.f;
		float cb = (snip.color & 0xFFu) / 255.f;
		if (cr > 0.96f && cg > 0.96f && cb > 0.96f)
		{
			cr = 1.f; cg = 1.f; cb = 1.f;
		}
		std::vector<Vec3> raw;
		raw.reserve(snip.points.size());
		float along = snip.uvAlong0;
		auto flush = [&]() {
			if (raw.size() < 2)
			{
				raw.clear();
				return;
			}
			EmitRibbonRun(out, raw, halfW, WorldGpsMath::kBlishUvPeriodM, flowScroll,
				cr, cg, cb, baseA, along);
			for (size_t i = 0; i + 1 < raw.size(); ++i)
				along += std::sqrt((raw[i + 1] - raw[i]).LengthSq());
			raw.clear();
		};
		for (const auto& wp : snip.points)
		{
			if (!WorldGpsMath::ReasonablePos(wp.x, wp.y, wp.z))
			{
				flush();
				continue;
			}
			raw.push_back({wp.x, wp.y + WorldGpsMath::kHeightBias, wp.z});
		}
		flush();
	}

	ID3D11ShaderResourceView* ResolveSrv(const PathingTrails::WorldSnippet& snip)
	{
		if (!snip.textureId[0] || !G::API || !G::API->Textures_Get)
			return nullptr;
		Texture_t* tex = G::API->Textures_Get(snip.textureId);
		if (!tex || !tex->Resource)
			return nullptr;
		return reinterpret_cast<ID3D11ShaderResourceView*>(tex->Resource);
	}

	void DrawBatch(ID3D11DeviceContext* ctx, const Vertex* data, UINT count,
		ID3D11ShaderResourceView* srv)
	{
		if (!count || !gVB)
			return;
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(ctx->Map(gVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return;
		std::memcpy(mapped.pData, data, count * sizeof(Vertex));
		ctx->Unmap(gVB, 0);
		if (srv)
		{
			ctx->PSSetShader(gPSTex, nullptr, 0);
			ctx->PSSetShaderResources(0, 1, &srv);
		}
		else
			ctx->PSSetShader(gPS, nullptr, 0);
		ctx->Draw(count, 0);
		if (srv)
		{
			ID3D11ShaderResourceView* nullSrv = nullptr;
			ctx->PSSetShaderResources(0, 1, &nullSrv);
		}
	}
}

bool WorldGpsD3d::DrawTrails(
	const WorldGpsMath::Mat4& viewProj,
	const WorldGpsMath::Vec3& /*cam*/,
	const WorldGpsMath::Vec3& avatar,
	float maxDist,
	float thickness,
	const std::vector<PathingTrails::WorldSnippet>& trails,
	const PathingTrails::WorldSnippet* guideOrNull)
{
	if (!EnsureDevice() || !gCtx || !G::API || !G::API->SwapChain)
		return false;

	struct Batch
	{
		std::vector<Vertex> verts;
		ID3D11ShaderResourceView* srv = nullptr;
	};
	std::vector<Batch> batches;
	batches.reserve(trails.size() + 1);

	/* Bake Blish flow into ribbon UVs (same scroll for arrows + hearts). */
	static LARGE_INTEGER sQpcFreq = {};
	static LARGE_INTEGER sQpcStart = {};
	static bool sQpcInit = false;
	if (!sQpcInit)
	{
		QueryPerformanceFrequency(&sQpcFreq);
		QueryPerformanceCounter(&sQpcStart);
		sQpcInit = true;
	}
	LARGE_INTEGER qpcNow{};
	QueryPerformanceCounter(&qpcNow);
	const double sec = (sQpcFreq.QuadPart > 0)
		? static_cast<double>(qpcNow.QuadPart - sQpcStart.QuadPart) /
			static_cast<double>(sQpcFreq.QuadPart)
		: 0.0;
	const float flow = static_cast<float>(std::fmod(sec * 0.45, 1024.0));

	auto addSnip = [&](const PathingTrails::WorldSnippet& snip, bool bright, float thick) {
		Batch b;
		b.srv = ResolveSrv(snip);
		/* Hearts without texture would fall back to a solid yellow ribbon - skip. */
		if (IsHeartTrail(snip) && !b.srv)
			return;
		AppendRibbon(b.verts, snip, thick, bright, flow);
		if (!b.verts.empty())
			batches.push_back(std::move(b));
	};

	if (guideOrNull && guideOrNull->points.size() >= 2)
		addSnip(*guideOrNull, true, thickness + 0.15f);
	for (const auto& snip : trails)
		addSnip(snip, false, thickness);

	UINT total = 0;
	for (const auto& b : batches)
		total += static_cast<UINT>(b.verts.size());
	if (total == 0)
		return true;
	if (!EnsureVB(total))
		return false;

	auto* swap = static_cast<IDXGISwapChain*>(G::API->SwapChain);
	ID3D11Texture2D* back = nullptr;
	if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))) || !back)
		return false;
	ID3D11RenderTargetView* rtv = nullptr;
	const HRESULT rtvHr = gDev->CreateRenderTargetView(back, nullptr, &rtv);
	D3D11_TEXTURE2D_DESC td{};
	back->GetDesc(&td);
	back->Release();
	if (FAILED(rtvHr) || !rtv)
		return false;

	float fadeStart = 0.f, fadeEnd = 0.f;
	WorldGpsMath::TrailFadeRange(maxDist, fadeStart, fadeEnd);
	const float clearMul = std::clamp(G::WorldTrailPlayerClear, 0.f, 3.f);
	const float hideM = WorldGpsMath::kAvatarTrailHideAt1 * clearMul;
	const float fadeM = hideM + WorldGpsMath::kAvatarTrailFadeExtraAt1 * clearMul;

	Constants cb{};
	std::memcpy(cb.viewProj, viewProj.m, sizeof(cb.viewProj));
	cb.avatar[0] = avatar.x; cb.avatar[1] = avatar.y; cb.avatar[2] = avatar.z;
	cb.avatar[3] = clearMul;
	cb.camPos[0] = avatar.x; cb.camPos[1] = avatar.y; cb.camPos[2] = avatar.z;
	cb.camPos[3] = 0.f; /* flow baked into ribbon verts */
	cb.fade[0] = fadeStart; cb.fade[1] = fadeEnd; cb.fade[2] = hideM; cb.fade[3] = fadeM;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(gCtx->Map(gCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		std::memcpy(mapped.pData, &cb, sizeof(cb));
		gCtx->Unmap(gCB, 0);
	}

	ID3D11RenderTargetView* prevRtv = nullptr;
	ID3D11DepthStencilView* prevDsv = nullptr;
	gCtx->OMGetRenderTargets(1, &prevRtv, &prevDsv);
	UINT numVp = 1;
	D3D11_VIEWPORT prevVp{};
	gCtx->RSGetViewports(&numVp, &prevVp);

	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(td.Width);
	vp.Height = static_cast<float>(td.Height);
	vp.MaxDepth = 1.f;
	gCtx->RSSetViewports(1, &vp);
	gCtx->OMSetRenderTargets(1, &rtv, nullptr);
	gCtx->OMSetBlendState(gBlend, nullptr, 0xFFFFFFFFu);
	gCtx->OMSetDepthStencilState(gDepth, 0);
	gCtx->RSSetState(gRaster);
	gCtx->IASetInputLayout(gLayout);
	gCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = sizeof(Vertex), offset = 0;
	gCtx->IASetVertexBuffers(0, 1, &gVB, &stride, &offset);
	gCtx->VSSetShader(gVS, nullptr, 0);
	gCtx->VSSetConstantBuffers(0, 1, &gCB);
	gCtx->PSSetConstantBuffers(0, 1, &gCB);
	gCtx->PSSetSamplers(0, 1, &gSamp);

	for (const auto& b : batches)
		DrawBatch(gCtx, b.verts.data(), static_cast<UINT>(b.verts.size()), b.srv);

	gCtx->OMSetRenderTargets(1, &prevRtv, prevDsv);
	if (numVp > 0)
		gCtx->RSSetViewports(numVp, &prevVp);
	if (prevRtv) prevRtv->Release();
	if (prevDsv) prevDsv->Release();
	rtv->Release();
	return true;
}
