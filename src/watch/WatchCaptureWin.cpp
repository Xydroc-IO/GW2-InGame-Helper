#include "WatchCaptureInternal.h"
#include "WatchLinux.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

using namespace WatchCaptureDetail;

namespace
{
	HBITMAP MakeTopDownDib(HDC hdc, int w, int h, void** bits)
	{
		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		return CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, bits, nullptr, 0);
	}

	CRITICAL_SECTION gWinCs{};
	bool gWinCsInit = false;
	std::atomic<bool> gWinPumpRun{ false };
	HANDLE gWinPumpThread = nullptr;
	std::vector<uint8_t> gWinReady;
	uint32_t gWinReadyW = 0, gWinReadyH = 0, gWinReadyStride = 0;
	uint32_t gWinReadySeq = 0;
	uint32_t gWinConsumedSeq = 0;

	void EnsureWinCs()
	{
		if (!gWinCsInit)
		{
			InitializeCriticalSection(&gWinCs);
			gWinCsInit = true;
		}
	}
}

namespace WatchCaptureDetail
{
	bool SampleLooksBlank(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t stride)
	{
		if (!bgra || w == 0 || h == 0)
			return true;
		uint32_t lit = 0;
		uint32_t samples = 0;
		const uint32_t stepX = (std::max)(1u, w / 16u);
		const uint32_t stepY = (std::max)(1u, h / 12u);
		for (uint32_t y = 0; y < h; y += stepY)
		{
			const uint8_t* row = bgra + static_cast<size_t>(y) * stride;
			for (uint32_t x = 0; x < w; x += stepX)
			{
				const uint8_t* p = row + static_cast<size_t>(x) * 4;
				++samples;
				if (p[0] > 18 || p[1] > 18 || p[2] > 18)
					++lit;
			}
		}
		return samples > 0 && lit * 40 < samples;
	}

	bool CaptureOnce(HWND hwnd, std::vector<uint8_t>& outBgra, uint32_t& outW, uint32_t& outH,
		uint32_t& outStride)
	{
		outW = outH = outStride = 0;
		RECT rc{};
		if (!GetClientRect(hwnd, &rc))
			return false;
		int srcW = rc.right - rc.left;
		int srcH = rc.bottom - rc.top;
		if (srcW < 2 || srcH < 2)
		{
			if (!GetWindowRect(hwnd, &rc))
				return false;
			srcW = rc.right - rc.left;
			srcH = rc.bottom - rc.top;
		}
		if (srcW < 2 || srcH < 2)
			return false;

		int dstW = srcW;
		int dstH = srcH;
		if (dstW > static_cast<int>(kMaxCaptureW) || dstH > static_cast<int>(kMaxCaptureH))
		{
			const float sx = static_cast<float>(kMaxCaptureW) / static_cast<float>(dstW);
			const float sy = static_cast<float>(kMaxCaptureH) / static_cast<float>(dstH);
			const float s = (std::min)(sx, sy);
			dstW = (std::max)(2, static_cast<int>(std::lround(dstW * s)));
			dstH = (std::max)(2, static_cast<int>(std::lround(dstH * s)));
		}

		HDC hdcWin = GetDC(hwnd);
		if (!hdcWin)
			hdcWin = GetWindowDC(hwnd);
		if (!hdcWin)
			return false;

		HDC hdcMem = CreateCompatibleDC(hdcWin);
		if (!hdcMem)
		{
			ReleaseDC(hwnd, hdcWin);
			return false;
		}

		void* bits = nullptr;
		HBITMAP dib = MakeTopDownDib(hdcMem, srcW, srcH, &bits);
		if (!dib || !bits)
		{
			if (dib) DeleteObject(dib);
			DeleteDC(hdcMem);
			ReleaseDC(hwnd, hdcWin);
			return false;
		}

		HGDIOBJ old = SelectObject(hdcMem, dib);
		BOOL ok = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);
		if (!ok)
			ok = PrintWindow(hwnd, hdcMem, 0);
		if (!ok)
			ok = BitBlt(hdcMem, 0, 0, srcW, srcH, hdcWin, 0, 0, SRCCOPY);

		void* finalBits = bits;
		int finalW = srcW;
		int finalH = srcH;
		HBITMAP dibScaled = nullptr;
		HDC hdcScaled = nullptr;
		HGDIOBJ oldScaled = nullptr;

		if (ok && (dstW != srcW || dstH != srcH))
		{
			hdcScaled = CreateCompatibleDC(hdcWin);
			void* bits2 = nullptr;
			dibScaled = hdcScaled ? MakeTopDownDib(hdcScaled, dstW, dstH, &bits2) : nullptr;
			if (dibScaled && bits2)
			{
				oldScaled = SelectObject(hdcScaled, dibScaled);
				SetStretchBltMode(hdcScaled, HALFTONE);
				StretchBlt(hdcScaled, 0, 0, dstW, dstH, hdcMem, 0, 0, srcW, srcH, SRCCOPY);
				finalBits = bits2;
				finalW = dstW;
				finalH = dstH;
			}
		}

		if (ok && finalBits)
		{
			const size_t stride = static_cast<size_t>(((finalW * 32 + 31) / 32) * 4);
			const size_t bytes = stride * static_cast<size_t>(finalH);
			outBgra.resize(bytes);
			std::memcpy(outBgra.data(), finalBits, bytes);
			/* GDI BI_RGB DIBs leave alpha = 0; ImGui samples A → invisible Mirror. */
			uint8_t* row = outBgra.data();
			for (int y = 0; y < finalH; ++y)
			{
				uint8_t* p = row;
				for (int x = 0; x < finalW; ++x)
				{
					p[3] = 255;
					p += 4;
				}
				row += stride;
			}
			outW = static_cast<uint32_t>(finalW);
			outH = static_cast<uint32_t>(finalH);
			outStride = static_cast<uint32_t>(stride);
		}

		if (oldScaled)
			SelectObject(hdcScaled, oldScaled);
		if (dibScaled)
			DeleteObject(dibScaled);
		if (hdcScaled)
			DeleteDC(hdcScaled);
		SelectObject(hdcMem, old);
		DeleteObject(dib);
		DeleteDC(hdcMem);
		ReleaseDC(hwnd, hdcWin);
		return ok != FALSE && outW > 0 && outH > 0;
	}

	DWORD WINAPI WinPumpThread(LPVOID)
	{
		std::vector<uint8_t> local;
		while (gWinPumpRun.load())
		{
			if (!gCapturing || WatchLinux::Available())
			{
				Sleep(20);
				continue;
			}
			HWND hwnd = reinterpret_cast<HWND>(gTarget);
			if (!hwnd || !IsWindow(hwnd))
			{
				Sleep(50);
				continue;
			}
			uint32_t w = 0, h = 0, stride = 0;
			if (!CaptureOnce(hwnd, local, w, h, stride))
			{
				Sleep(30);
				continue;
			}
			EnsureWinCs();
			EnterCriticalSection(&gWinCs);
			gWinReady.swap(local);
			gWinReadyW = w;
			gWinReadyH = h;
			gWinReadyStride = stride;
			++gWinReadySeq;
			LeaveCriticalSection(&gWinCs);
			Sleep(16); /* ~60 FPS */
		}
		return 0;
	}

	void EnsureWinPump()
	{
		EnsureWinCs();
		if (gWinPumpRun.load())
			return;
		gWinPumpRun = true;
		gWinPumpThread = CreateThread(nullptr, 0, WinPumpThread, nullptr, 0, nullptr);
		if (gWinPumpThread)
			SetThreadPriority(gWinPumpThread, THREAD_PRIORITY_LOWEST);
	}

	void StopWinPump()
	{
		gWinPumpRun = false;
		if (gWinPumpThread)
		{
			WaitForSingleObject(gWinPumpThread, 2000);
			CloseHandle(gWinPumpThread);
			gWinPumpThread = nullptr;
		}
	}

	void ResetWinReady()
	{
		EnsureWinCs();
		EnterCriticalSection(&gWinCs);
		gWinReady.clear();
		gWinReadySeq = gWinConsumedSeq = 0;
		LeaveCriticalSection(&gWinCs);
	}

	bool TakeWinFrame(std::vector<uint8_t>& bgra, uint32_t& w, uint32_t& h, uint32_t& stride)
	{
		EnsureWinCs();
		EnterCriticalSection(&gWinCs);
		if (gWinReadySeq == gWinConsumedSeq || gWinReady.empty())
		{
			LeaveCriticalSection(&gWinCs);
			return false;
		}
		bgra.swap(gWinReady);
		w = gWinReadyW;
		h = gWinReadyH;
		stride = gWinReadyStride;
		gWinConsumedSeq = gWinReadySeq;
		gWinReady.clear();
		LeaveCriticalSection(&gWinCs);
		return true;
	}
}
