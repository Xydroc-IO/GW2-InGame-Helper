#include "WatchCaptureInternal.h"

#include "CrashTrail.h"
#include "EiRuntime.h"
#include "Globals.h"

#include <cstdio>
#include <cstring>

#include <d3d11.h>
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

	/* Wine: dedicated Mirror device — UpdateSubresource off the game/ImGui context.
	   Game device only OpenSharedResource + SRV for AddImage. */
	ID3D11Device*            gMirrorDevice = nullptr;
	ID3D11DeviceContext*     gMirrorContext = nullptr;
	ID3D11Texture2D*         gMirrorTex = nullptr;
	ID3D11Texture2D*         gDeadMirrorTex = nullptr;
	bool                     gDedicatedTried = false;
	bool                     gDedicatedMirror = false;
	char                     gMirrorGpuPath[96] = "idle";

	uint64_t                 gTarget = 0;
	int                      gRawEnumCount = 0;
	bool                     gCapturing = false;
	DWORD                    gLastCaptureMs = 0;
	bool                     gLastBlank = false;
	std::string              gStatus = "Idle — pick a window and Start.";
	std::vector<WatchCapture::WindowEntry> gWindows;

	void ReleaseDeadGpu()
	{
		if (CrashTrail::DetailArmed()
			&& (gDeadSrv || gDeadTex || gDeadStagingTex || gDeadMirrorTex))
			CrashTrail::Note("gpu:ReleaseDeadGpu");
		if (gDeadSrv) { gDeadSrv->Release(); gDeadSrv = nullptr; }
		if (gDeadTex) { gDeadTex->Release(); gDeadTex = nullptr; }
		if (gDeadStagingTex) { gDeadStagingTex->Release(); gDeadStagingTex = nullptr; }
		if (gDeadMirrorTex) { gDeadMirrorTex->Release(); gDeadMirrorTex = nullptr; }
	}

	void ReleaseGpu()
	{
		if (gSrv) { gSrv->Release(); gSrv = nullptr; }
		if (gTex) { gTex->Release(); gTex = nullptr; }
		if (gStagingTex) { gStagingTex->Release(); gStagingTex = nullptr; }
		if (gMirrorTex) { gMirrorTex->Release(); gMirrorTex = nullptr; }
		gTexW = gTexH = 0;
		gContentW = gContentH = 0;
	}

	/* Move the live SRV/textures aside instead of Release() — Wine can still
	   be sampling last frame's ImGui draw list when portal size changes. */
	void ParkLiveGpu()
	{
		if (gDeadSrv || gDeadMirrorTex)
		{
			/* Only one parked generation — free the older set if we must park again. */
			ReleaseDeadGpu();
		}
		gDeadSrv = gSrv;
		gDeadTex = gTex;
		gDeadStagingTex = gStagingTex;
		gDeadMirrorTex = gMirrorTex;
		gSrv = nullptr;
		gTex = nullptr;
		gStagingTex = nullptr;
		gMirrorTex = nullptr;
		gTexW = gTexH = 0;
		gDeadParkedAt = gGpuTick;
	}

	void RequestGpuRelease()
	{
		/* Hide content immediately so this frame won't AddImage after Stop. */
		gContentW = gContentH = 0;
		if (!(gSrv || gTex || gStagingTex || gMirrorTex || gDeadSrv || gDeadMirrorTex))
			return;
		gDeferGpuRelease = true;
	}

	void HideContent()
	{
		gContentW = gContentH = 0;
	}

	void FlushDeferredGpuRelease()
	{
		++gGpuTick;
		/* Wine Present lags ImGui; long Mirror Soft-stop needs a longer park. */
		const uint32_t parkHold = EiRuntime::IsWine() ? 48u : 3u;
		if ((gDeadSrv || gDeadMirrorTex) && gGpuTick - gDeadParkedAt >= parkHold)
		{
			if (CrashTrail::DetailArmed())
				CrashTrail::NoteF("gpu:park expire tick=%u", gGpuTick);
			ReleaseDeadGpu();
		}
		if (!gDeferGpuRelease)
			return;
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("gpu:FlushDeferred park/release");
		gDeferGpuRelease = false;
		if (gSrv || gTex || gStagingTex || gMirrorTex)
			ParkLiveGpu();
		else
			ReleaseGpu();
	}

	void ReleaseMirrorDevice()
	{
		if (gMirrorContext) { gMirrorContext->Release(); gMirrorContext = nullptr; }
		if (gMirrorDevice) { gMirrorDevice->Release(); gMirrorDevice = nullptr; }
		gDedicatedMirror = false;
	}

	void ReleaseDevice()
	{
		gDeferGpuRelease = false;
		ReleaseDeadGpu();
		ReleaseGpu();
		ReleaseMirrorDevice();
		gDedicatedTried = false;
		if (gContext) { gContext->Release(); gContext = nullptr; }
		if (gDevice) { gDevice->Release(); gDevice = nullptr; }
	}

	bool EnsureGameDevice()
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

	bool EnsureDevice()
	{
		return EnsureGameDevice();
	}

	bool EnsureMirrorDevice()
	{
		if (gMirrorDevice && gMirrorContext)
			return true;
		if (!EnsureGameDevice())
			return false;

		IDXGIDevice* dxgiDev = nullptr;
		if (FAILED(gDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDev))) || !dxgiDev)
			return false;
		IDXGIAdapter* adapter = nullptr;
		const HRESULT adaptHr = dxgiDev->GetAdapter(&adapter);
		dxgiDev->Release();
		if (FAILED(adaptHr) || !adapter)
			return false;

		const D3D_FEATURE_LEVEL levels[] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
		ID3D11Device* mirDev = nullptr;
		ID3D11DeviceContext* mirCtx = nullptr;
		/* Same adapter as the game swapchain — required for SHARED textures. */
		const HRESULT hr = D3D11CreateDevice(
			adapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			nullptr,
			0,
			levels,
			static_cast<UINT>(sizeof(levels) / sizeof(levels[0])),
			D3D11_SDK_VERSION,
			&mirDev,
			&got,
			&mirCtx);
		adapter->Release();
		if (FAILED(hr) || !mirDev || !mirCtx)
		{
			if (mirDev) mirDev->Release();
			if (mirCtx) mirCtx->Release();
			return false;
		}
		gMirrorDevice = mirDev;
		gMirrorContext = mirCtx;
		return true;
	}

	bool WantDedicatedMirror()
	{
		/* Wine tip: soft Begin beside UpdateSubresource on the game context.
		   Native keeps the single-device path (shared works too, but unneeded). */
		return EiRuntime::IsWine();
	}

	void SetMirrorGpuPath(const char* note)
	{
		std::snprintf(gMirrorGpuPath, sizeof(gMirrorGpuPath), "%s", note ? note : "");
	}

	bool OpenSharedOnto(ID3D11Device* openDev, HANDLE shared, ID3D11Texture2D** outTex)
	{
		if (!openDev || !shared || !outTex)
			return false;
		*outTex = nullptr;
		return SUCCEEDED(openDev->OpenSharedResource(shared, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(outTex))) && *outTex;
	}

	bool CreateSharedPair(ID3D11Device* createDev, ID3D11Device* openDev,
		ID3D11Texture2D** outCreateTex, ID3D11Texture2D** outOpenTex)
	{
		if (!createDev || !openDev || !outCreateTex || !outOpenTex)
			return false;
		*outCreateTex = nullptr;
		*outOpenTex = nullptr;

		D3D11_TEXTURE2D_DESC td{};
		td.Width = kMaxCaptureW;
		td.Height = kMaxCaptureH;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;
		td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

		ID3D11Texture2D* created = nullptr;
		if (FAILED(createDev->CreateTexture2D(&td, nullptr, &created)) || !created)
			return false;

		IDXGIResource* dxgiRes = nullptr;
		HANDLE shared = nullptr;
		if (FAILED(created->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&dxgiRes))) || !dxgiRes)
		{
			created->Release();
			return false;
		}
		const HRESULT shareHr = dxgiRes->GetSharedHandle(&shared);
		dxgiRes->Release();
		if (FAILED(shareHr) || !shared)
		{
			created->Release();
			return false;
		}

		ID3D11Texture2D* opened = nullptr;
		if (!OpenSharedOnto(openDev, shared, &opened))
		{
			created->Release();
			return false;
		}

		*outCreateTex = created;
		*outOpenTex = opened;
		return true;
	}

	bool CreateSharedMirrorTextures()
	{
		if (!gMirrorDevice || !gDevice)
			return false;

		ID3D11Texture2D* gameTex = nullptr;
		ID3D11Texture2D* mirTex = nullptr;

		/* Prefer game-owned SHARED (ImGui samples native resource; Mirror opens it). */
		if (CreateSharedPair(gDevice, gMirrorDevice, &gameTex, &mirTex))
			SetMirrorGpuPath("dedicated (game-shared)");
		else if (CreateSharedPair(gMirrorDevice, gDevice, &mirTex, &gameTex))
			SetMirrorGpuPath("dedicated (mirror-shared)");
		else
		{
			SetMirrorGpuPath("fallback: SHARED open failed");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = 1;
		ID3D11ShaderResourceView* srv = nullptr;
		if (FAILED(gDevice->CreateShaderResourceView(gameTex, &sd, &srv)) || !srv)
		{
			gameTex->Release();
			mirTex->Release();
			SetMirrorGpuPath("fallback: SRV failed");
			return false;
		}

		gMirrorTex = mirTex;
		gTex = gameTex;
		gSrv = srv;
		gTexW = kMaxCaptureW;
		gTexH = kMaxCaptureH;
		gDedicatedMirror = true;
		CrashTrail::Note(gMirrorGpuPath);
		return true;
	}

	bool CreateLocalMirrorTextures()
	{
		if (!EnsureGameDevice())
			return false;

		D3D11_TEXTURE2D_DESC td{};
		td.Width = kMaxCaptureW;
		td.Height = kMaxCaptureH;
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

		gTexW = kMaxCaptureW;
		gTexH = kMaxCaptureH;
		gDedicatedMirror = false;
		if (WantDedicatedMirror())
			SetMirrorGpuPath("fallback: game-device upload");
		else
			SetMirrorGpuPath("native game-device");
		CrashTrail::Note(gMirrorGpuPath);
		return true;
	}

	bool EnsureTexture(uint32_t w, uint32_t h)
	{
		/* Do not FlushDeferred here — Tick owns lifetime so ImGui can Present first. */
		if (!EnsureGameDevice() || w == 0 || h == 0)
			return false;
		if (w > kMaxCaptureW || h > kMaxCaptureH)
			return false;
		/* Allocate once at max — portal resize used to CreateTexture2D + SRV every
		   wobble and tip Wine over after long Watch sessions. */
		if (gTex && gSrv && gTexW == kMaxCaptureW && gTexH == kMaxCaptureH
			&& (!gDedicatedMirror || gMirrorTex))
			return true;

		/* Do not Release() the live SRV here — park it for later Ticks. */
		if (gSrv || gTex || gStagingTex || gMirrorTex)
			ParkLiveGpu();
		gContentW = gContentH = 0;

		if (WantDedicatedMirror())
		{
			if (!gDedicatedTried)
			{
				gDedicatedTried = true;
				if (!EnsureMirrorDevice())
				{
					SetMirrorGpuPath("fallback: no Mirror device");
					ReleaseMirrorDevice();
					gDedicatedMirror = false;
				}
				else if (CreateSharedMirrorTextures())
					return true;
				else
				{
					ReleaseMirrorDevice();
					gDedicatedMirror = false;
				}
			}
			else if (gDedicatedMirror && EnsureMirrorDevice() && CreateSharedMirrorTextures())
				return true;
		}

		return CreateLocalMirrorTextures();
	}

	bool UploadBgra(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t srcStride)
	{
		if (!bgra || w == 0 || h == 0 || srcStride < w * 4)
			return false;
		if (!EnsureTexture(w, h) || !gSrv)
			return false;

		D3D11_BOX box{};
		box.left = 0;
		box.top = 0;
		box.front = 0;
		box.right = w;
		box.bottom = h;
		box.back = 1;

		if (gDedicatedMirror && gMirrorContext && gMirrorTex)
		{
			if (CrashTrail::DetailArmed())
				CrashTrail::NoteF("gpu:Upload dedicated %ux%u", w, h);
			/* Upload on Mirror device only — game context never sees UpdateSubresource. */
			gMirrorContext->UpdateSubresource(gMirrorTex, 0, &box, bgra, srcStride, 0);
			gMirrorContext->Flush();
			gContentW = w;
			gContentH = h;
			return true;
		}

		if (!gContext || !gTex)
			return false;
		if (CrashTrail::DetailArmed())
			CrashTrail::NoteF("gpu:Upload gamectx %ux%u", w, h);
		gContext->UpdateSubresource(gTex, 0, &box, bgra, srcStride, 0);
		gContentW = w;
		gContentH = h;
		return true;
	}
}
