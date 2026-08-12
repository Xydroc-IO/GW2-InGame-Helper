#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <d3d11.h>
#include <windows.h>

#include "WatchCapture.h"

namespace WatchCaptureDetail
{
	constexpr uint32_t kMaxCaptureW = 1280;
	constexpr uint32_t kMaxCaptureH = 720;
	/* Native ~60 FPS; Wine: ~30 FPS — long Soft-stop tips were worse at 60, but
	   20 FPS is too choppy for watching. Soft-stop still pauses uploads. */
	constexpr DWORD    kMinFrameMsNative = 16;
	constexpr DWORD    kMinFrameMsWine = 33; /* ~30 FPS present */

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

	extern ID3D11Device*           gDevice;
	extern ID3D11DeviceContext*    gContext;
	extern ID3D11Texture2D*        gTex;
	extern ID3D11Texture2D*        gStagingTex;
	extern ID3D11ShaderResourceView* gSrv;
	extern uint32_t                gTexW;
	extern uint32_t                gTexH;
	extern uint32_t                gContentW;
	extern uint32_t                gContentH;
	/* Stop/close must not Release() while this frame's ImGui draw list still
	 * holds the SRV — that use-after-free can take down the game process.
	 * Resize parks the prior set in gDead* for the same reason (Wine). */
	extern bool                    gDeferGpuRelease;
	extern ID3D11Texture2D*        gDeadTex;
	extern ID3D11Texture2D*        gDeadStagingTex;
	extern ID3D11ShaderResourceView* gDeadSrv;
	extern ID3D11Texture2D*        gDeadMirrorTex;
	extern bool                    gDedicatedMirror;
	extern char                    gMirrorGpuPath[96];

	extern int                      gRawEnumCount;
	extern uint64_t                gTarget;
	extern bool                    gCapturing;
	extern DWORD                   gLastCaptureMs;
	extern bool                    gLastBlank;
	extern std::string             gStatus;
	extern std::vector<WatchCapture::WindowEntry> gWindows;
	extern bool                    gClassicList;

	bool EnsureDevice();
	bool EnsureTexture(uint32_t w, uint32_t h);
	bool UploadBgra(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t srcStride);
	void ReleaseGpu();
	void FlushDeferredGpuRelease();
	void RequestGpuRelease();
	void HideContent(); /* Soft-stop: drop AddImage dims now; park on deferred Stop */
	void ReleaseDevice();

	bool ApplyChromeCrop(const uint8_t*& ptr, uint32_t& w, uint32_t& h, uint32_t stride);
	bool SampleLooksBlank(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t stride);
	bool CaptureOnce(HWND hwnd, std::vector<uint8_t>& outBgra, uint32_t& outW, uint32_t& outH,
		uint32_t& outStride);
	void EnsureWinPump();
	void StopWinPump();
	void ResetWinReady();
	bool TakeWinFrame(std::vector<uint8_t>& bgra, uint32_t& w, uint32_t& h, uint32_t& stride);
}
