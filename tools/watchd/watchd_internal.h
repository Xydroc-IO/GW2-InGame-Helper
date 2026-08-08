#pragma once

#include "../../src/watch/WatchProto.h"

#include <atomic>
#include <cstdint>
#include <string>

/* Host watchd — portal/PipeWire capture into WatchProto shm. */
namespace WatchdDetail
{
	extern uint8_t* gShm;
	extern int      gShmFd;
	extern std::string gDataDir;

	extern std::atomic<bool> gWantCapture;
	extern std::atomic<bool> gCapturing;
	extern std::atomic<bool> gPortalBusy;
	extern std::string gLastErr;

	WatchProto::ShmHeader* Hdr();
	bool EnsureShm();
	void SetCapturing(bool on);
	void PublishBgra(uint32_t dstW, uint32_t dstH, const uint8_t* bgra, uint32_t srcStride);

	/* Scale + convert SPA video formats into BGRA shm (pace ~60 FPS). */
	void PublishSpaFrame(uint32_t srcW, uint32_t srcH, uint32_t srcStride,
		const uint8_t* src, int spaVideoFormat);

	/* Blocks for portal UI, then runs PipeWire until gWantCapture clears. */
	bool RunPortalCaptureLoop(std::string& errOut);
}
