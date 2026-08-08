#include "WatchCaptureWgc.h"

#include "EiRuntime.h"
#include "entryInternal.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>

#include <asyncinfo.h>
#include <d3d11.h>
#include <dxgi.h>
#include <roapi.h>
#include <shobjidl.h>
/* Mingw: boolean vs BYTE IReference clash inside foundation.h */
#define ____x_ABI_CWindows_CFoundation_CIReference_1_boolean_INTERFACE_DEFINED__
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.h>
#include <winstring.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame;
using ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool;
using ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePoolStatics2;
using ABI::Windows::Graphics::Capture::IGraphicsCaptureItem;
using ABI::Windows::Graphics::Capture::IGraphicsCaptureSession;
using ABI::Windows::Graphics::Capture::IGraphicsCaptureSessionStatics;
using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface;
using ABI::Windows::Graphics::SizeInt32;
using ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized;

namespace
{
	MIDL_INTERFACE("5a1711b3-ad79-4b4a-9336-1318fdde3539")
	IGraphicsCapturePicker : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE PickSingleItemAsync(IInspectable** operation) = 0;
	};

	MIDL_INTERFACE("00000000-0000-0000-0000-000000000000")
	IAsyncOpItem : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE get_Completed(IUnknown** handler) = 0;
		virtual HRESULT STDMETHODCALLTYPE put_Completed(IUnknown* handler) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetResults(IGraphicsCaptureItem** result) = 0;
	};

	MIDL_INTERFACE("A9B3A807-480D-4A07-A3C4-D3B5C6E8B3C0")
	IDirect3DDxgiInterfaceAccess : public IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID iid, void** p) = 0;
	};

	MIDL_INTERFACE("30d5a829-7fa4-4026-83bb-d75bae4ea99e")
	IClosableStub : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE Close() = 0;
	};

	/* Explicit IIDs — avoid __CRT_UUID_DECL inside anonymous namespace. */
	const GUID kIidPicker =
		{ 0x5a1711b3, 0xad79, 0x4b4a, { 0x93,0x36, 0x13,0x18,0xfd,0xde,0x35,0x39 } };
	const GUID kIidDxgiAccess =
		{ 0xA9B3A807, 0x480D, 0x4A07, { 0xA3,0xC4, 0xD3,0xB5,0xC6,0xE8,0xB3,0xC0 } };
	const GUID kIidClosable =
		{ 0x30d5a829, 0x7fa4, 0x4026, { 0x83,0xbb, 0xd7,0x5b,0xae,0x4e,0xa9,0x9e } };

	/* Resolve at runtime — static import of CreateDirect3D11SurfaceFromDXGISurface
	   from GraphicsCapture.dll triggers Entry Point Not Found on older Win10 builds. */
	using PFN_D3D11CreateDevice = HRESULT (WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE,
		UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*,
		ID3D11DeviceContext**);
	using PFN_CreateDirect3D11DeviceFromDXGIDevice =
		HRESULT (WINAPI*)(IUnknown* dxgiDevice, IInspectable** graphicsDevice);

	struct D3dInterop
	{
		HMODULE mod = nullptr;
		PFN_D3D11CreateDevice createDevice = nullptr;
		PFN_CreateDirect3D11DeviceFromDXGIDevice createWinrtDevice = nullptr;
		bool probed = false;
		bool ok = false;
	};

	D3dInterop& Interop()
	{
		static D3dInterop s;
		if (s.probed)
			return s;
		s.probed = true;
		s.mod = LoadLibraryW(L"d3d11.dll");
		if (!s.mod)
			return s;
		s.createDevice = reinterpret_cast<PFN_D3D11CreateDevice>(
			reinterpret_cast<void*>(GetProcAddress(s.mod, "D3D11CreateDevice")));
		s.createWinrtDevice = reinterpret_cast<PFN_CreateDirect3D11DeviceFromDXGIDevice>(
			reinterpret_cast<void*>(GetProcAddress(s.mod, "CreateDirect3D11DeviceFromDXGIDevice")));
		/* Surface helper is required by WGC frame interop on some builds; missing
		   export is exactly the Entry Point dialog users hit. */
		const bool hasSurface = GetProcAddress(s.mod, "CreateDirect3D11SurfaceFromDXGISurface") != nullptr;
		s.ok = s.createDevice && s.createWinrtDevice && hasSurface;
		return s;
	}

	constexpr wchar_t kPickerClass[] = L"Windows.Graphics.Capture.GraphicsCapturePicker";
	constexpr wchar_t kPoolClass[] = L"Windows.Graphics.Capture.Direct3D11CaptureFramePool";
	constexpr wchar_t kSessionClass[] = L"Windows.Graphics.Capture.GraphicsCaptureSession";

	std::recursive_mutex gMu;
	std::atomic<bool> gPickerOpen{ false };
	std::atomic<bool> gCapturing{ false };
	std::atomic<bool> gStop{ false };
	std::string gStatus = "Idle";

	ComPtr<IDirect3DDevice> gWinrtDevice;
	ComPtr<IDirect3D11CaptureFramePool> gPool;
	ComPtr<IGraphicsCaptureSession> gSession;
	ComPtr<IGraphicsCaptureItem> gItem;
	ComPtr<ID3D11Device> gD3d;
	ComPtr<ID3D11DeviceContext> gCtx;
	ComPtr<ID3D11Texture2D> gStaging;
	uint32_t gStagingW = 0, gStagingH = 0;

	void SetStatus(const char* s)
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		gStatus = s ? s : "";
	}

	HRESULT MakeHString(const wchar_t* s, HSTRING* out)
	{
		return WindowsCreateString(s, static_cast<UINT32>(wcslen(s)), out);
	}

	bool SessionSupported()
	{
		HSTRING hs = nullptr;
		if (FAILED(MakeHString(kSessionClass, &hs)))
			return false;
		ComPtr<IActivationFactory> factory;
		const HRESULT hr = RoGetActivationFactory(hs, IID_PPV_ARGS(&factory));
		WindowsDeleteString(hs);
		if (FAILED(hr) || !factory)
			return false;
		ComPtr<IGraphicsCaptureSessionStatics> st;
		if (FAILED(factory.As(&st)) || !st)
			return false;
		boolean ok = 0;
		return SUCCEEDED(st->IsSupported(&ok)) && ok != 0;
	}

	HRESULT EnsureD3d(IDirect3DDevice** outDev)
	{
		*outDev = nullptr;
		D3dInterop& api = Interop();
		if (!api.ok || !api.createDevice || !api.createWinrtDevice)
			return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> ctx;
		D3D_FEATURE_LEVEL fl;
		const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
		HRESULT hr = api.createDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			levels, 2, D3D11_SDK_VERSION, &device, &fl, &ctx);
		if (FAILED(hr))
			hr = api.createDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
				levels, 2, D3D11_SDK_VERSION, &device, &fl, &ctx);
		if (FAILED(hr) || !device)
			return hr;
		ComPtr<IDXGIDevice> dxgi;
		hr = device.As(&dxgi);
		if (FAILED(hr))
			return hr;
		ComPtr<IInspectable> insp;
		hr = api.createWinrtDevice(dxgi.Get(), &insp);
		if (FAILED(hr) || !insp)
			return hr;
		ComPtr<IDirect3DDevice> winrtDev;
		hr = insp.As(&winrtDev);
		if (FAILED(hr) || !winrtDev)
			return hr;
		gD3d = device;
		gCtx = ctx;
		*outDev = winrtDev.Detach();
		return S_OK;
	}

	void CloseInspectable(IInspectable* p)
	{
		IClosableStub* c = nullptr;
		if (p && SUCCEEDED(p->QueryInterface(kIidClosable, reinterpret_cast<void**>(&c))) && c)
		{
			c->Close();
			c->Release();
		}
	}

	void TearDownSession_NoLock()
	{
		CloseInspectable(gSession.Get());
		CloseInspectable(gPool.Get());
		gSession.Reset();
		gPool.Reset();
		gItem.Reset();
		gWinrtDevice.Reset();
		gStaging.Reset();
		gStagingW = gStagingH = 0;
		gCapturing = false;
	}

	HRESULT WaitAsyncItem(IInspectable* opInsp, IGraphicsCaptureItem** outItem)
	{
		*outItem = nullptr;
		ComPtr<IAsyncInfo> info;
		HRESULT hr = opInsp->QueryInterface(IID_PPV_ARGS(&info));
		if (FAILED(hr))
			return hr;
		AsyncStatus st = Started;
		for (;;)
		{
			if (gStop.load())
			{
				info->Cancel();
				return HRESULT_FROM_WIN32(ERROR_CANCELLED);
			}
			hr = info->get_Status(&st);
			if (FAILED(hr))
				return hr;
			if (st != Started)
				break;
			Sleep(16);
		}
		if (st != Completed)
			return st == Canceled ? HRESULT_FROM_WIN32(ERROR_CANCELLED) : E_FAIL;
		return reinterpret_cast<IAsyncOpItem*>(opInsp)->GetResults(outItem);
	}

	HRESULT CreatePoolAndStart(IGraphicsCaptureItem* item)
	{
		ComPtr<IDirect3DDevice> device;
		HRESULT hr = EnsureD3d(&device);
		if (FAILED(hr))
			return hr;
		SizeInt32 size{};
		hr = item->get_Size(&size);
		if (FAILED(hr) || size.Width < 2 || size.Height < 2)
			return E_FAIL;

		HSTRING hs = nullptr;
		hr = MakeHString(kPoolClass, &hs);
		if (FAILED(hr))
			return hr;
		ComPtr<IActivationFactory> factory;
		hr = RoGetActivationFactory(hs, IID_PPV_ARGS(&factory));
		WindowsDeleteString(hs);
		if (FAILED(hr))
			return hr;
		ComPtr<IDirect3D11CaptureFramePoolStatics2> st2;
		hr = factory.As(&st2);
		if (FAILED(hr) || !st2)
			return hr;

		ComPtr<IDirect3D11CaptureFramePool> pool;
		hr = st2->CreateFreeThreaded(device.Get(), DirectXPixelFormat_B8G8R8A8UIntNormalized,
			2, size, &pool);
		if (FAILED(hr) || !pool)
			return hr;
		ComPtr<IGraphicsCaptureSession> session;
		hr = pool->CreateCaptureSession(item, &session);
		if (FAILED(hr) || !session)
			return hr;
		hr = session->StartCapture();
		if (FAILED(hr))
			return hr;

		std::lock_guard<std::recursive_mutex> lock(gMu);
		TearDownSession_NoLock();
		gWinrtDevice = device;
		gPool = pool;
		gSession = session;
		gItem = item;
		gCapturing = true;
		return S_OK;
	}

	DWORD WINAPI PickerThread(LPVOID)
	{
		gPickerOpen = true;
		HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
		const bool needUninit = SUCCEEDED(hr);
		if (hr == RPC_E_CHANGED_MODE)
			hr = S_OK;
		if (FAILED(hr))
		{
			SetStatus("WGC init failed.");
			gPickerOpen = false;
			return 0;
		}

		HWND host = EntryDetail::sGameHwnd ? EntryDetail::sGameHwnd : GetForegroundWindow();
		HSTRING hs = nullptr;
		hr = MakeHString(kPickerClass, &hs);
		ComPtr<IInspectable> insp;
		if (SUCCEEDED(hr))
			hr = RoActivateInstance(hs, &insp);
		WindowsDeleteString(hs);

		ComPtr<IGraphicsCapturePicker> picker;
		if (SUCCEEDED(hr) && insp)
		{
			IGraphicsCapturePicker* raw = nullptr;
			hr = insp->QueryInterface(kIidPicker, reinterpret_cast<void**>(&raw));
			picker.Attach(raw);
		}
		if (SUCCEEDED(hr) && picker && host)
		{
			ComPtr<IInitializeWithWindow> init;
			if (SUCCEEDED(picker.As(&init)) && init)
				hr = init->Initialize(host);
		}

		ComPtr<IInspectable> op;
		ComPtr<IGraphicsCaptureItem> item;
		if (SUCCEEDED(hr) && picker)
		{
			SetStatus("Choose a window in the system picker…");
			hr = picker->PickSingleItemAsync(&op);
			if (SUCCEEDED(hr) && op)
				hr = WaitAsyncItem(op.Get(), &item);
		}

		if (SUCCEEDED(hr) && item)
		{
			if (gStop.load())
			{
				SetStatus("Stopped.");
				gCapturing = false;
			}
			else
			{
				SetStatus("Starting capture…");
				if (!Interop().ok)
				{
					hr = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
					SetStatus("WGC interop missing — use Classic list.");
					gCapturing = false;
				}
				else
				{
					hr = CreatePoolAndStart(item.Get());
					if (gStop.load())
					{
						std::lock_guard<std::recursive_mutex> lock(gMu);
						TearDownSession_NoLock();
						SetStatus("Stopped.");
						gCapturing = false;
					}
					else
					{
						SetStatus(SUCCEEDED(hr) ? "Capturing (Windows Graphics Capture)."
							: "WGC session failed — use Classic list.");
						if (FAILED(hr))
							gCapturing = false;
					}
				}
			}
		}
		else
		{
			SetStatus((hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) || !item)
				? "Picker cancelled."
				: "System picker unavailable — use Classic list.");
			gCapturing = false;
		}

		if (needUninit)
			RoUninitialize();
		gPickerOpen = false;
		return 0;
	}

	bool EnsureStaging(uint32_t w, uint32_t h)
	{
		if (gStaging && gStagingW == w && gStagingH == h)
			return true;
		gStaging.Reset();
		if (!gD3d)
			return false;
		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_STAGING;
		td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		if (FAILED(gD3d->CreateTexture2D(&td, nullptr, &gStaging)) || !gStaging)
			return false;
		gStagingW = w;
		gStagingH = h;
		return true;
	}
}

bool WatchCaptureWgc::Available()
{
	if (EiRuntime::IsWine())
		return false;
	static int s = -1;
	if (s < 0)
	{
		if (!Interop().ok)
		{
			s = 0;
			return false;
		}
		const HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
		s = SessionSupported() ? 1 : 0;
		if (SUCCEEDED(hr))
			RoUninitialize();
	}
	return s != 0;
}

bool WatchCaptureWgc::IsCapturing() { return gCapturing.load(); }
bool WatchCaptureWgc::IsPickerOpen() { return gPickerOpen.load(); }

bool WatchCaptureWgc::StartPicker(std::string& status)
{
	if (!Available())
	{
		status = "Windows Graphics Capture not supported.";
		return false;
	}
	if (gPickerOpen.load() || gCapturing.load())
	{
		status = gPickerOpen.load() ? "Picker already open…" : "Already capturing.";
		return gCapturing.load();
	}
	gStop = false;
	SetStatus("Opening system picker…");
	HANDLE th = CreateThread(nullptr, 0, PickerThread, nullptr, 0, nullptr);
	if (!th)
	{
		status = "Failed to start picker thread.";
		return false;
	}
	CloseHandle(th);
	status = "Opening system picker…";
	return true;
}

void WatchCaptureWgc::Stop()
{
	gStop = true;
	gCapturing = false;
	std::lock_guard<std::recursive_mutex> lock(gMu);
	TearDownSession_NoLock();
	gStatus = "Stopped.";
}

void WatchCaptureWgc::GetStatus(std::string& status)
{
	std::lock_guard<std::recursive_mutex> lock(gMu);
	status = gStatus;
}

bool WatchCaptureWgc::TakeFrame(std::vector<uint8_t>& bgra, uint32_t& w, uint32_t& h,
	uint32_t& stride, uint32_t maxW, uint32_t maxH)
{
	w = h = stride = 0;
	ComPtr<IDirect3D11CaptureFramePool> pool;
	ComPtr<ID3D11DeviceContext> ctx;
	{
		std::lock_guard<std::recursive_mutex> lock(gMu);
		if (!gCapturing.load() || !gPool || !gCtx)
			return false;
		pool = gPool;
		ctx = gCtx;
	}

	ComPtr<IDirect3D11CaptureFrame> frame;
	if (FAILED(pool->TryGetNextFrame(&frame)) || !frame)
		return false;

	ComPtr<IDirect3DSurface> surface;
	if (FAILED(frame->get_Surface(&surface)) || !surface)
		return false;
	IDirect3DDxgiInterfaceAccess* accessRaw = nullptr;
	if (FAILED(surface->QueryInterface(kIidDxgiAccess, reinterpret_cast<void**>(&accessRaw))) || !accessRaw)
		return false;
	ComPtr<IDirect3DDxgiInterfaceAccess> access;
	access.Attach(accessRaw);
	ComPtr<ID3D11Texture2D> tex;
	if (FAILED(access->GetInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(tex.GetAddressOf())))
		|| !tex)
		return false;

	D3D11_TEXTURE2D_DESC desc{};
	tex->GetDesc(&desc);
	const uint32_t srcW = desc.Width, srcH = desc.Height;
	if (srcW < 2 || srcH < 2)
		return false;

	uint32_t dstW = srcW, dstH = srcH;
	if (dstW > maxW || dstH > maxH)
	{
		const float s = (std::min)(static_cast<float>(maxW) / dstW, static_cast<float>(maxH) / dstH);
		dstW = (std::max)(2u, static_cast<uint32_t>(std::lround(dstW * s)));
		dstH = (std::max)(2u, static_cast<uint32_t>(std::lround(dstH * s)));
	}

	if (!EnsureStaging(srcW, srcH))
		return false;
	ctx->CopyResource(gStaging.Get(), tex.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(ctx->Map(gStaging.Get(), 0, D3D11_MAP_READ, 0, &mapped)) || !mapped.pData)
		return false;

	const uint32_t outStride = dstW * 4u;
	bgra.resize(static_cast<size_t>(outStride) * dstH);
	const auto* srcBase = static_cast<const uint8_t*>(mapped.pData);
	for (uint32_t y = 0; y < dstH; ++y)
	{
		const uint32_t sy = (dstH == srcH) ? y : (y * srcH / dstH);
		const uint8_t* srow = srcBase + static_cast<size_t>(sy) * mapped.RowPitch;
		uint8_t* drow = bgra.data() + static_cast<size_t>(y) * outStride;
		for (uint32_t x = 0; x < dstW; ++x)
		{
			const uint32_t sx = (dstW == srcW) ? x : (x * srcW / dstW);
			const uint8_t* sp = srow + static_cast<size_t>(sx) * 4u;
			uint8_t* dp = drow + static_cast<size_t>(x) * 4u;
			dp[0] = sp[0];
			dp[1] = sp[1];
			dp[2] = sp[2];
			dp[3] = 255;
		}
	}
	ctx->Unmap(gStaging.Get(), 0);
	CloseInspectable(frame.Get());
	w = dstW;
	h = dstH;
	stride = outStride;
	return true;
}
