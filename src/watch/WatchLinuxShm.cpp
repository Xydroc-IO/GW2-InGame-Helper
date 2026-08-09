#include "WatchLinuxInternal.h"

#include "AddonPaths.h"

#include <cstring>

using namespace WatchLinuxDetail;

namespace WatchLinuxDetail
{
	void UnmapShmUnlocked()
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

	void UnmapShm()
	{
		EnsureCs();
		EnterCriticalSection(&gCs);
		/* Never tear the map while the game thread is presenting a slot. */
		if (gPresentSlot != WatchProto::kNoSlot)
		{
			LeaveCriticalSection(&gCs);
			return;
		}
		UnmapShmUnlocked();
		LeaveCriticalSection(&gCs);
	}

	bool MapShmPathUnlocked(const wchar_t* path)
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
			UnmapShmUnlocked();
			return false;
		}
		gShmMap = CreateFileMappingW(gShmFile, nullptr, PAGE_READWRITE, 0,
			static_cast<DWORD>(WatchProto::kShmBytes), nullptr);
		if (!gShmMap)
		{
			UnmapShmUnlocked();
			return false;
		}
		gShmView = static_cast<uint8_t*>(MapViewOfFile(gShmMap, FILE_MAP_ALL_ACCESS, 0, 0,
			WatchProto::kShmBytes));
		if (!gShmView)
		{
			UnmapShmUnlocked();
			return false;
		}
		return true;
	}

	bool EnsureShmMappedUnlocked()
	{
		if (gShmView)
			return true;

		static const wchar_t kUnixShm[] = L"\\\\?\\unix\\/dev/shm/gw2igh-watch-frame";
		if (!MapShmPathUnlocked(kUnixShm))
		{
			const std::wstring fallback = AddonPaths::DataDir() + L"\\gw2igh-watch-frame";
			if (!MapShmPathUnlocked(fallback.c_str()))
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

	bool EnsureShmMapped()
	{
		EnsureCs();
		EnterCriticalSection(&gCs);
		const bool ok = EnsureShmMappedUnlocked();
		LeaveCriticalSection(&gCs);
		return ok;
	}

	/* Status only — pixels are presented directly from shm (CEF-style).
	   Also drains queued portal Start so Soft Start never CreateThreads. */
	DWORD WINAPI PumpThread(LPVOID)
	{
		while (gPumpRun.load())
		{
			if (gStartRequested.exchange(false))
			{
				const uint32_t epoch = gQueuedStartEpoch.load();
				RunQueuedStart(epoch);
			}

			if (!gCapturing.load())
			{
				Sleep(50);
				continue;
			}
			EnsureShmMapped();
			if (gShmView)
			{
				EnsureCs();
				EnterCriticalSection(&gCs);
				if (gShmView)
				{
					const auto* h = reinterpret_cast<const WatchProto::ShmHeader*>(gShmView);
					if (h->magic == WatchProto::kShmMagic && h->capturing)
						gPumpStatus = "Capturing (OOP watchd).";
				}
				LeaveCriticalSection(&gCs);
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
