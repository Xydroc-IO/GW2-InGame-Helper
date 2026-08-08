#include "watchd_internal.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace WatchdDetail
{
	uint8_t* gShm = nullptr;
	int      gShmFd = -1;
	std::string gDataDir = "/tmp";

	std::atomic<bool> gWantCapture{ false };
	std::atomic<bool> gCapturing{ false };
	std::atomic<bool> gPortalBusy{ false };
	std::string gLastErr;

	WatchProto::ShmHeader* Hdr()
	{
		return reinterpret_cast<WatchProto::ShmHeader*>(gShm);
	}

	bool EnsureShm()
	{
		if (gShm)
			return true;

		/* Prefer /dev/shm so Wine can map via \\?\unix\/dev/shm/... */
		gShmFd = ::open(WatchProto::kShmUnixPath, O_RDWR | O_CREAT, 0600);
		if (gShmFd < 0)
		{
			const std::string fallback = gDataDir + "/gw2igh-watch-frame";
			gShmFd = ::open(fallback.c_str(), O_RDWR | O_CREAT, 0600);
		}
		if (gShmFd < 0)
		{
			std::perror("watchd open shm");
			return false;
		}
		if (ftruncate(gShmFd, static_cast<off_t>(WatchProto::kShmBytes)) != 0)
		{
			std::perror("watchd ftruncate shm");
			::close(gShmFd);
			gShmFd = -1;
			return false;
		}
		void* p = mmap(nullptr, WatchProto::kShmBytes, PROT_READ | PROT_WRITE, MAP_SHARED, gShmFd, 0);
		if (p == MAP_FAILED)
		{
			std::perror("watchd mmap shm");
			::close(gShmFd);
			gShmFd = -1;
			return false;
		}
		gShm = static_cast<uint8_t*>(p);
		auto* h = Hdr();
		if (h->magic != WatchProto::kShmMagic)
		{
			std::memset(gShm, 0, WatchProto::kShmHeaderBytes);
			h->magic = WatchProto::kShmMagic;
			h->version = WatchProto::kVersion;
			h->reading = WatchProto::kNoSlot;
		}
		return true;
	}

	void SetCapturing(bool on)
	{
		gCapturing = on;
		if (!gShm)
			return;
		Hdr()->capturing = on ? 1u : 0u;
		if (!on)
		{
			/* Keep last frame; just mark idle. */
		}
	}

	void PublishBgra(uint32_t dstW, uint32_t dstH, const uint8_t* bgra, uint32_t srcStride)
	{
		if (!gShm || !bgra || dstW == 0 || dstH == 0)
			return;
		if (dstW > WatchProto::kMaxW)
			dstW = WatchProto::kMaxW;
		if (dstH > WatchProto::kMaxH)
			dstH = WatchProto::kMaxH;

		auto* h = Hdr();
		const uint32_t reading = h->reading;
		uint32_t slot = (h->slot + 1u) & 1u;
		if (reading == slot)
			slot ^= 1u;

		const uint32_t stride = dstW * 4u;
		uint8_t* dst = WatchProto::ShmSlotPixels(gShm, slot);
		for (uint32_t y = 0; y < dstH; ++y)
		{
			std::memcpy(dst + static_cast<size_t>(y) * stride,
				bgra + static_cast<size_t>(y) * srcStride, stride);
		}
		__sync_synchronize();
		h->w = dstW;
		h->h = dstH;
		h->stride = stride;
		h->slot = slot;
		h->seq = h->seq + 1u;
		h->capturing = 1u;
		__sync_synchronize();
	}
}
