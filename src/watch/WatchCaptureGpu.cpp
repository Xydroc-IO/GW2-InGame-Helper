#include "WatchCaptureInternal.h"

#include "Globals.h"

#include <cstring>

#include <dxgi.h>

namespace WatchCaptureDetail
{
	ID3D11Device*            gDevice = nullptr;
	ID3D11DeviceContext*     gContext = nullptr;
	ID3D11Texture2D*         gTex = nullptr;
	ID3D11Texture2D*         gStagingTex = nullptr;
	ID3D11ShaderResourceView* gSrv = nullptr;
	/* Prior live set after resize — ImGui/Wine may still sample until next present. */
	ID3D11Texture2D*         gDeadTex = nullptr;
	ID3D11Texture2D*         gDeadStagingTex = nullptr;
	ID3D11ShaderResourceView* gDeadSrv = nullptr;
	uint32_t                 gTexW = 0;
	uint32_t                 gTexH = 0;
	uint32_t                 gContentW = 0;
	uint32_t                 gContentH = 0;
	bool                     gDeferGpuRelease = false;
	uint32_t                 gGpuTick = 0;
	uint32_t                 gDeadParkedAt = 0;

	uint64_t                 gTarget = 0;
	int                      gRawEnumCount = 0;
	bool                     gCapturing = false;
	DWORD                    gLastCaptureMs = 0;
	bool                     gLastBlank = false;
	std::string              gStatus = "Idle — pick a window and Start.";
	std::vector<WatchCapture::WindowEntry> gWindows;

	void ReleaseDeadGpu()
	{
		if (gDeadSrv) { gDeadSrv->Release(); gDeadSrv = nullptr; }
		if (gDeadTex) { gDeadTex->Release(); gDeadTex = nullptr; }
		if (gDeadStagingTex) { gDeadStagingTex->Release(); gDeadStagingTex = nullptr; }
	}

	void ReleaseGpu()
	{
		if (gSrv) { gSrv->Release(); gSrv = nullptr; }
		if (gTex) { gTex->Release(); gTex = nullptr; }
		if (gStagingTex) { gStagingTex->Release(); gStagingTex = nullptr; }
		gTexW = gTexH = 0;
		gContentW = gContentH = 0;
	}

	void FlushDeferredGpuRelease()
	{
		++gGpuTick;
		/* Keep parked SRVs at least 2 Ticks — Wine Present often lags ImGui NewFrame. */
		if (gDeadSrv && gGpuTick - gDeadParkedAt >= 2u)
			ReleaseDeadGpu();
		if (!gDeferGpuRelease)
			return;
		gDeferGpuRelease = false;
		ReleaseGpu();
	}

	void RequestGpuRelease()
	{
		/* Hide content immediately so this frame won't AddImage after Stop,
		 * but keep the SRV alive until the next Tick (after ImGui presents). */
		gContentW = gContentH = 0;
		gDeferGpuRelease = true;
	}

	/* Move the live SRV/textures aside instead of Release() — Wine can still
	   be sampling last frame's ImGui draw list when portal size changes. */
	void ParkLiveGpu()
	{
		if (gDeadSrv)
		{
			/* Only one parked generation — free the older set if we must park again. */
			ReleaseDeadGpu();
		}
		gDeadSrv = gSrv;
		gDeadTex = gTex;
		gDeadStagingTex = gStagingTex;
		gSrv = nullptr;
		gTex = nullptr;
		gStagingTex = nullptr;
		gTexW = gTexH = 0;
		gDeadParkedAt = gGpuTick;
	}

	void ReleaseDevice()
	{
		gDeferGpuRelease = false;
		ReleaseDeadGpu();
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
		/* Do not FlushDeferred here — Tick owns lifetime so ImGui can Present first. */
		if (!EnsureDevice() || w == 0 || h == 0)
			return false;
		if (gTex && gStagingTex && gSrv && gTexW == w && gTexH == h)
			return true;

		/* Do not Release() the live SRV here — park it for later Ticks. */
		if (gSrv || gTex || gStagingTex)
			ParkLiveGpu();
		gContentW = gContentH = 0;

		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;

		if (FAILED(gDevice->CreateTexture2D(&td, nullptr, &gTex)) || !gTex)
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = td.Format;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = 1;
		if (FAILED(gDevice->CreateShaderResourceView(gTex, &sd, &gSrv)) || !gSrv)
		{
			ReleaseGpu();
			return false;
		}

		td.Usage = D3D11_USAGE_STAGING;
		td.BindFlags = 0;
		td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(gDevice->CreateTexture2D(&td, nullptr, &gStagingTex)) || !gStagingTex)
		{
			ReleaseGpu();
			return false;
		}

		gTexW = w;
		gTexH = h;
		return true;
	}

	bool UploadBgra(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t srcStride)
	{
		if (!bgra || w == 0 || h == 0 || srcStride < w * 4)
			return false;
		if (!EnsureTexture(w, h) || !gContext || !gTex)
			return false;

		/* Prefer UpdateSubresource — avoids Map stalls on the game thread. */
		D3D11_BOX box{};
		box.left = 0;
		box.top = 0;
		box.front = 0;
		box.right = w;
		box.bottom = h;
		box.back = 1;
		gContext->UpdateSubresource(gTex, 0, &box, bgra, srcStride, 0);
		gContentW = w;
		gContentH = h;
		return true;
	}
}
