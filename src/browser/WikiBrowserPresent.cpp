#include "WikiBrowser.h"
#include "WikiBrowserShared.h"

#include "CrashTrail.h"
#include "Globals.h"
#include "WikiIpc.h"
#include "WinePadOpen.h"

#include <cstdint>
#include <cstring>
#include <cstdio>

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

namespace WikiBrowserDetail
{
	void ReleaseGpu()
	{
		if (gSrv) { gSrv->Release(); gSrv = nullptr; }
		if (gTex) { gTex->Release(); gTex = nullptr; }
		if (gStagingTex) { gStagingTex->Release(); gStagingTex = nullptr; }
		gTexW = gTexH = 0;
		gContentW = gContentH = 0;
		gLastFrameSeq = 0;
		gTexHasContent = false;
		gLastMapHr = S_OK;
		gMapFailCount = 0;
		gLastTexHr = S_OK;
	}

	void ReleaseDevice()
	{
		ReleaseGpu();
		if (gContext) { gContext->Release(); gContext = nullptr; }
		if (gDevice) { gDevice->Release(); gDevice = nullptr; }
	}

	bool EnsureDevice()
	{
		if (gDevice && gContext)
			return true;
		if (!G::API || !G::API->SwapChain)
			return false;
		auto* swap = static_cast<IDXGISwapChain*>(G::API->SwapChain);
		ID3D11Device* dev = nullptr;
		if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev))) || !dev)
			return false;
		ID3D11DeviceContext* ctx = nullptr;
		dev->GetImmediateContext(&ctx);
		if (!ctx)
		{
			dev->Release();
			return false;
		}
		gDevice = dev;
		gContext = ctx;
		return true;
	}

	bool EnsureTexture(uint32_t w, uint32_t h)
	{
		if (!EnsureDevice() || w == 0 || h == 0)
			return false;
		/* Allocate once at max OSR size — window drag used to CreateTexture2D
		   every pixel and hitch the game. Content is uploaded into the top-left. */
		if (gTex && gStagingTex && gSrv &&
			gTexW == kWikiFrameMaxW && gTexH == kWikiFrameMaxH)
			return true;

		ReleaseGpu();

		D3D11_TEXTURE2D_DESC td{};
		td.Width = kWikiFrameMaxW;
		td.Height = kWikiFrameMaxH;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		/* DEFAULT display texture — never Map'd. DYNAMIC+Map under GW2's busy
		   device was the Windows-only "Ready but Waiting for first paint" stall. */
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;

		gLastTexHr = gDevice->CreateTexture2D(&td, nullptr, &gTex);
		if (FAILED(gLastTexHr) || !gTex)
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = td.Format;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = 1;
		gLastTexHr = gDevice->CreateShaderResourceView(gTex, &sd, &gSrv);
		if (FAILED(gLastTexHr) || !gSrv)
		{
			ReleaseGpu();
			return false;
		}

		td.Usage = D3D11_USAGE_STAGING;
		td.BindFlags = 0;
		td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		gLastTexHr = gDevice->CreateTexture2D(&td, nullptr, &gStagingTex);
		if (FAILED(gLastTexHr) || !gStagingTex)
		{
			ReleaseGpu();
			return false;
		}

		gTexW = kWikiFrameMaxW;
		gTexH = kWikiFrameMaxH;
		return true;
	}
}

using namespace WikiBrowserDetail;

void WikiBrowser::PresentFrame()
{
	if (!gWantVisible.load() || !gIpc || !gFramePixels)
		return;

	/* Helper died under Wine (often 0x80000003) — do not touch shm / upload. */
	if (!HelperAlive())
	{
		gContentW = gContentH = 0;
		gTexHasContent = false;
		return;
	}

	/* Soft rail/companion work: do not Map/upload the same frame as Navigate/Begin.
	   After Soft clears, keep skipping a few frames so idle→click Map pressure drains. */
	static int sPresentCooldown = 0;
	if (WinePadOpen::SoftWorkBusy())
	{
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("cef:Present skip SoftWorkBusy");
		sPresentCooldown = 10;
		gStagingReady = false;
		gPartialCopySeq = 0;
		gPartialCopyY = 0;
		if (!gStagingFrame.empty())
		{
			gStagingFrame.clear();
			gStagingFrame.shrink_to_fit();
		}
		return;
	}
	CrashTrail::Scope presentScope("cef:Present enter", "cef:Present leave");
	if (sPresentCooldown > 0)
	{
		--sPresentCooldown;
		return;
	}

	FlushPendingCmds();
	FlushPendingNavigate();

	/* Ready but CEF never called OnPaint — kick was_resized via SET_BOUNDS.
	   Distinct from GPU Map stalls (frame_seq > 0). */
	if (gIpc->ready && gIpc->frame_seq == 0)
	{
		const DWORD now = GetTickCount();
		if (gLastPaintKickMs == 0 || (now - gLastPaintKickMs) >= 500u)
		{
			gLastPaintKickMs = now;
			PostCmd(WIKI_CMD_SET_BOUNDS);
		}
		return;
	}

	if (!gIpc->ready || gIpc->frame_seq == 0)
		return;

	/* Acquire ordering paired with helper MemoryBarrier before frame_seq++. */
	MemoryBarrier();
	const uint32_t seq = gIpc->frame_seq;
	if (seq == gLastFrameSeq)
		return;

	/* Cap GPU upload — display-rate while scrolling/interacting, ~30 FPS idle.
	   Wheel gets a longer smooth window so coasting after a flick stays fluid. */
	const DWORD now = GetTickCount();
	const bool wheelSmooth =
		gLastWheelMs != 0 && (now - gLastWheelMs) < 400u;
	const bool interactive =
		wheelSmooth ||
		(gLastUserInputMs != 0 && (now - gLastUserInputMs) < 750u);
	/* ~120 Hz while wheel-scrolling; ~60 Hz for other input; ~30 Hz idle. */
	const DWORD budgetMs = wheelSmooth ? 8u : (interactive ? 16u : 33u);
	if (gLastPresentMs != 0 && (now - gLastPresentMs) < budgetMs)
		return;

	MemoryBarrier();
	const uint32_t front = gIpc->frame_front & 1u;
	uint32_t w = gIpc->frame_w;
	uint32_t h = gIpc->frame_h;
	if (w == 0 || h == 0 || w > kWikiFrameMaxW || h > kWikiFrameMaxH)
		return;
	if (gIpc->frame_seq != seq)
		return;

	uint32_t dx = gIpc->dirty_x;
	uint32_t dy = gIpc->dirty_y;
	uint32_t dw = gIpc->dirty_w;
	uint32_t dh = gIpc->dirty_h;
	if (dw == 0 || dh == 0 || dx >= w || dy >= h)
	{
		dx = 0;
		dy = 0;
		dw = w;
		dh = h;
	}
	if (dx + dw > w) dw = w - dx;
	if (dy + dh > h) dh = h - dy;
	const bool dirtyIsFull = (dx == 0 && dy == 0 && dw == w && dh == h);
	bool uploadFull = dirtyIsFull || !gTexHasContent || gContentW != w || gContentH != h;

	if (!EnsureTexture(w, h) || !gContext || !gTex || !gStagingTex)
		return;

	const uint32_t rowBudget = interactive ? kMaxCopyRowsInteractive : kMaxCopyRowsPerFrame;
	/* Never chunk while interacting/scrolling — multi-frame staging lags behind
	   CEF paints and feels like stuttering scroll. First paint still full copy. */
	if (interactive)
	{
		gStagingReady = false;
		gPartialCopySeq = 0;
		gPartialCopyY = 0;
	}
	const bool continuingPartial =
		!interactive && gTexHasContent && uploadFull && gPartialCopySeq == seq &&
		gPartialCopyFront == front && gPartialCopyY > 0 && gStagingReady;
	const bool wantChunk = !interactive && gTexHasContent && uploadFull &&
		(continuingPartial || h > rowBudget);

	if (wantChunk && (!gStagingReady || gStagingSeq != seq || gStagingW != w || gStagingH != h))
	{
		gIpc->frame_reading = front;
		MemoryBarrier();
		if (gIpc->frame_seq != seq)
		{
			gIpc->frame_reading = 0xFFFFFFFFu;
			return;
		}
		const uint8_t* srcSnap = gFramePixels + static_cast<size_t>(front) * kWikiFrameBytes;
		const size_t need = static_cast<size_t>(h) * kWikiFrameStride;
		gStagingFrame.resize(need);
		if (gStagingFrame.size() < need)
		{
			gIpc->frame_reading = 0xFFFFFFFFu;
			return;
		}
		for (uint32_t y = 0; y < h; ++y)
		{
			std::memcpy(
				gStagingFrame.data() + static_cast<size_t>(y) * kWikiFrameStride,
				srcSnap + static_cast<size_t>(y) * kWikiFrameStride,
				static_cast<size_t>(w) * 4);
		}
		MemoryBarrier();
		gIpc->frame_reading = 0xFFFFFFFFu;
		gStagingSeq = seq;
		gStagingW = w;
		gStagingH = h;
		gStagingReady = true;
		gPartialCopySeq = seq;
		gPartialCopyFront = front;
		gPartialCopyY = 0;
		gLastPresentMs = now;
		return;
	}

	/* STAGING Map — never Map the ImGui-bound DEFAULT texture. WRITE_DISCARD is
	   invalid on STAGING; block on first paint so a busy game device cannot
	   leave us on "Waiting for first paint…" forever. */
	D3D11_MAPPED_SUBRESOURCE mapped{};
	const bool firstPaint = !gTexHasContent;
	UINT mapFlags = firstPaint ? 0u : static_cast<UINT>(D3D11_MAP_FLAG_DO_NOT_WAIT);
	HRESULT hr = gContext->Map(gStagingTex, 0, D3D11_MAP_WRITE, mapFlags, &mapped);
	/* Wine: never block on Map after DO_NOT_WAIT — idle→interact spikes were
	   stalling Present under a busy GW2 device and tip-over on first click. */
	if (FAILED(hr) && mapFlags != 0u && !WinePadOpen::Soft())
		hr = gContext->Map(gStagingTex, 0, D3D11_MAP_WRITE, 0u, &mapped);
	gLastMapHr = hr;
	if (FAILED(hr))
	{
		++gMapFailCount;
		/* Wine: after repeated Map fails, cool Present so side-nav SoftOpen
		   does not land on a hot D3D device (builds over 10–20 min idle). */
		if (WinePadOpen::Soft() && gMapFailCount >= 3u)
			sPresentCooldown = 10;
		return;
	}
	if (!mapped.pData)
	{
		gContext->Unmap(gStagingTex, 0);
		gLastMapHr = E_POINTER;
		++gMapFailCount;
		return;
	}
	gMapFailCount = 0;

	uint32_t copyY0 = 0;
	uint32_t copyY1 = h;
	uint32_t copyX0 = 0;
	uint32_t copyX1 = w;
	bool copyComplete = true;
	if (wantChunk && gStagingReady)
	{
		const uint32_t y0 = gPartialCopyY;
		const uint32_t endY = gPartialCopyY + rowBudget;
		const uint32_t y1 = endY < h ? endY : h;
		const size_t rowBytes = static_cast<size_t>(w) * 4;
		for (uint32_t y = y0; y < y1; ++y)
		{
			std::memcpy(
				static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch,
				gStagingFrame.data() + static_cast<size_t>(y) * kWikiFrameStride,
				rowBytes);
		}
		gPartialCopyY = y1;
		copyComplete = (gPartialCopyY >= h);
		copyY0 = y0;
		copyY1 = y1;
		if (copyComplete)
		{
			gPartialCopySeq = 0;
			gPartialCopyY = 0;
			gStagingReady = false;
		}
	}
	else if (uploadFull)
	{
		gPartialCopySeq = 0;
		gPartialCopyY = 0;
		gStagingReady = false;
		gIpc->frame_reading = front;
		MemoryBarrier();
		const uint8_t* src = gFramePixels + static_cast<size_t>(front) * kWikiFrameBytes;
		uint8_t* dst = static_cast<uint8_t*>(mapped.pData);
		const size_t rowBytes = static_cast<size_t>(w) * 4;
		for (uint32_t y = 0; y < h; ++y)
			std::memcpy(dst + y * mapped.RowPitch, src + y * kWikiFrameStride, rowBytes);
		MemoryBarrier();
		gIpc->frame_reading = 0xFFFFFFFFu;
		copyY0 = 0;
		copyY1 = h;
		copyX0 = 0;
		copyX1 = w;
	}
	else
	{
		gPartialCopySeq = 0;
		gPartialCopyY = 0;
		gStagingReady = false;
		gIpc->frame_reading = front;
		MemoryBarrier();
		const uint8_t* src = gFramePixels + static_cast<size_t>(front) * kWikiFrameBytes;
		uint8_t* dst = static_cast<uint8_t*>(mapped.pData);
		const size_t rowBytes = static_cast<size_t>(dw) * 4;
		const size_t xOff = static_cast<size_t>(dx) * 4;
		for (uint32_t y = 0; y < dh; ++y)
		{
			const uint32_t row = dy + y;
			std::memcpy(
				dst + row * mapped.RowPitch + xOff,
				src + row * kWikiFrameStride + xOff,
				rowBytes);
		}
		MemoryBarrier();
		gIpc->frame_reading = 0xFFFFFFFFu;
		copyX0 = dx;
		copyY0 = dy;
		copyX1 = dx + dw;
		copyY1 = dy + dh;
	}
	gContext->Unmap(gStagingTex, 0);

	D3D11_BOX box{};
	box.left = copyX0;
	box.top = copyY0;
	box.front = 0;
	box.right = copyX1;
	box.bottom = copyY1;
	box.back = 1;
	gContext->CopySubresourceRegion(
		gTex, 0, copyX0, copyY0, 0, gStagingTex, 0, &box);

	if (!copyComplete)
	{
		gLastPresentMs = now;
		return;
	}
	gContentW = w;
	gContentH = h;
	gTexHasContent = true;
	gLastFrameSeq = seq;
	gLastPresentMs = now;
}

bool WikiBrowser::HasFrame()
{
	return HelperAlive() && gSrv && gIpc && gIpc->frame_seq > 0 && gContentW > 0 && gContentH > 0;
}

const char* WikiBrowser::PaintWaitReasonCStr()
{
	if (!gIpc)
		return "no IPC";
	if (!gIpc->ready)
		return "helper not ready";
	if (gIpc->frame_seq == 0)
	{
		std::snprintf(gPaintWaitReason, sizeof(gPaintWaitReason),
			"CEF has not painted yet (frame_seq=0) — kicking resize");
		return gPaintWaitReason;
	}
	if (FAILED(gLastTexHr) && !gSrv)
	{
		std::snprintf(gPaintWaitReason, sizeof(gPaintWaitReason),
			"D3D texture create failed hr=0x%08lX",
			static_cast<unsigned long>(gLastTexHr));
		return gPaintWaitReason;
	}
	if (!gSrv || !gStagingTex)
		return "D3D textures not ready";
	if (gContentW == 0 || gContentH == 0)
	{
		if (FAILED(gLastMapHr))
		{
			std::snprintf(gPaintWaitReason, sizeof(gPaintWaitReason),
				"CEF painted (seq=%u) but staging Map failed hr=0x%08lX x%u",
				static_cast<unsigned>(gIpc->frame_seq),
				static_cast<unsigned long>(gLastMapHr),
				static_cast<unsigned>(gMapFailCount));
			return gPaintWaitReason;
		}
		std::snprintf(gPaintWaitReason, sizeof(gPaintWaitReason),
			"CEF painted (seq=%u %ux%u) — GPU upload pending",
			static_cast<unsigned>(gIpc->frame_seq),
			static_cast<unsigned>(gIpc->frame_w),
			static_cast<unsigned>(gIpc->frame_h));
		return gPaintWaitReason;
	}
	return "";
}

ID3D11ShaderResourceView* WikiBrowser::FrameSrv()
{
	return gSrv;
}

int WikiBrowser::FrameWidth() { return static_cast<int>(gContentW); }
int WikiBrowser::FrameHeight() { return static_cast<int>(gContentH); }

void WikiBrowser::FrameUvMax(float* outU, float* outV)
{
	if (outU)
		*outU = (gTexW > 0 && gContentW > 0) ? static_cast<float>(gContentW) / static_cast<float>(gTexW) : 1.f;
	if (outV)
		*outV = (gTexH > 0 && gContentH > 0) ? static_cast<float>(gContentH) / static_cast<float>(gTexH) : 1.f;
}

