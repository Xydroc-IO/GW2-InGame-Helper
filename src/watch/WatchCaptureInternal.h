#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <d3d11.h>
#include <windows.h>

#include "WatchCapture.h"

namespace WatchCaptureDetail
{
	constexpr uint32_t kMaxCaptureW = 640;
	constexpr uint32_t kMaxCaptureH = 360;
	constexpr DWORD    kMinFrameMs = 16; /* ~60 FPS present */

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

	extern int                      gRawEnumCount;
	extern uint64_t                gTarget;
	extern bool                    gCapturing;
	extern DWORD                   gLastCaptureMs;
	extern bool                    gLastBlank;
	extern std::string             gStatus;
	extern std::vector<WatchCapture::WindowEntry> gWindows;

	bool EnsureDevice();
	bool EnsureTexture(uint32_t w, uint32_t h);
	bool UploadBgra(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t srcStride);
	void ReleaseGpu();
	void ReleaseDevice();

	bool SampleLooksBlank(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t stride);
	bool CaptureOnce(HWND hwnd, std::vector<uint8_t>& outBgra, uint32_t& outW, uint32_t& outH,
		uint32_t& outStride);
	void EnsureWinPump();
	void StopWinPump();
	void ResetWinReady();
	bool TakeWinFrame(std::vector<uint8_t>& bgra, uint32_t& w, uint32_t& h, uint32_t& stride);
}
