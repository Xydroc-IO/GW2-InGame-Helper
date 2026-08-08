#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include <d3d11.h>
#include <roapi.h>
/* Mingw: boolean vs BYTE IReference clash inside foundation.h */
#define ____x_ABI_CWindows_CFoundation_CIReference_1_boolean_INTERFACE_DEFINED__
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool;
using ABI::Windows::Graphics::Capture::IGraphicsCaptureItem;
using ABI::Windows::Graphics::Capture::IGraphicsCaptureSession;
using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;

/* Shared state/helpers for WatchCaptureWgc.cpp + WatchCaptureWgcSession.cpp. */
namespace WatchCaptureWgcDetail
{
	extern const GUID kIidDxgiAccess;

	extern std::recursive_mutex gMu;
	extern std::atomic<bool> gPickerOpen;
	extern std::atomic<bool> gCapturing;
	extern std::atomic<bool> gStop;
	extern std::string gStatus;

	extern ComPtr<IDirect3DDevice> gWinrtDevice;
	extern ComPtr<IDirect3D11CaptureFramePool> gPool;
	extern ComPtr<IGraphicsCaptureSession> gSession;
	extern ComPtr<IGraphicsCaptureItem> gItem;
	extern ComPtr<ID3D11Device> gD3d;
	extern ComPtr<ID3D11DeviceContext> gCtx;
	extern ComPtr<ID3D11Texture2D> gStaging;
	extern uint32_t gStagingW, gStagingH;

	bool InteropOk();
	bool SessionSupported();
	void SetStatus(const char* s);
	void CloseInspectable(IInspectable* p);
	void TearDownSession_NoLock();
	bool EnsureStaging(uint32_t w, uint32_t h);
	DWORD WINAPI PickerThread(LPVOID);
}
