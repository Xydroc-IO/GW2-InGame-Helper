#include "WatchLinuxInternal.h"

#include "AddonPaths.h"

#include <cstring>

using namespace WatchLinuxDetail;

namespace WatchLinuxDetail
{
	void UnmapShm()
	{
		if (gShmView)
		{
			UnmapViewOfFile(gShmView);
			gShmView = nullptr;
		}
		if (gShmMap)
		{
			CloseHandle(gShmMap);
			gShmMap = nullptr;
		}
		if (gShmFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(gShmFile);
			gShmFile = INVALID_HANDLE_VALUE;
		}
	}

	bool MapShmPath(const wchar_t* path)
	{
		gShmFile = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (gShmFile == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER need{};
		need.QuadPart = static_cast<LONGLONG>(WatchProto::kShmBytes);
		if (!SetFilePointerEx(gShmFile, need, nullptr, FILE_BEGIN) || !SetEndOfFile(gShmFile))
		{
			UnmapShm();
			return false;
		}
		gShmMap = CreateFileMappingW(gShmFile, nullptr, PAGE_READWRITE, 0,
			static_cast<DWORD>(WatchProto::kShmBytes), nullptr);
		if (!gShmMap)
		{
			UnmapShm();
			return false;
		}
		gShmView = static_cast<uint8_t*>(MapViewOfFile(gShmMap, FILE_MAP_ALL_ACCESS, 0, 0,
			WatchProto::kShmBytes));
		if (!gShmView)
		{
			UnmapShm();
			return false;
		}
		return true;
	}

	bool EnsureShmMapped()
	{
		if (gShmView)
			return true;

		static const wchar_t kUnixShm[] = L"\\\\?\\unix\\/dev/shm/gw2igh-watch-frame";
		if (!MapShmPath(kUnixShm))
		{
			const std::wstring fallback = AddonPaths::DataDir() + L"\\gw2igh-watch-frame";
			if (!MapShmPath(fallback.c_str()))
				return false;
		}

		auto* h = reinterpret_cast<WatchProto::ShmHeader*>(gShmView);
		if (h->magic != WatchProto::kShmMagic)
		{
			std::memset(gShmView, 0, WatchProto::kShmHeaderBytes);
			h->magic = WatchProto::kShmMagic;
			h->version = WatchProto::kVersion;
			h->reading = WatchProto::kNoSlot;
		}
		return true;
	}

	/* Status only — pixels are presented directly from shm (CEF-style). */
	DWORD WINAPI PumpThread(LPVOID)
	{
		while (gPumpRun.load())
		{
			if (!gCapturing.load())
			{
				Sleep(50);
				continue;
			}
			if (!gShmView)
				EnsureShmMapped();
			if (gShmView)
			{
				const auto* h = reinterpret_cast<const WatchProto::ShmHeader*>(gShmView);
				if (h->magic == WatchProto::kShmMagic && h->capturing)
					gPumpStatus = "Capturing (OOP watchd ~60 FPS).";
			}
			Sleep(200);
		}
		return 0;
	}

	void EnsurePump()
	{
		EnsureCs();
		if (gPumpRun.load())
			return;
		gPumpRun = true;
		gPumpThread = CreateThread(nullptr, 0, PumpThread, nullptr, 0, nullptr);
		if (gPumpThread)
			SetThreadPriority(gPumpThread, THREAD_PRIORITY_LOWEST);
	}

	void StopPump()
	{
		gPumpRun = false;
		if (gPumpThread)
		{
			WaitForSingleObject(gPumpThread, 2000);
			CloseHandle(gPumpThread);
			gPumpThread = nullptr;
		}
	}
}
