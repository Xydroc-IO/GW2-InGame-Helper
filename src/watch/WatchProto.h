#pragma once

#include <cstdint>
#include <cstddef>

/* Wine DLL ↔ host gw2igh-watchd.
 * Linux: xdg-desktop-portal ScreenCast → PipeWire → /dev/shm (not X11).
 * DLL only presents (CEF-style). Target ~60 FPS. */
namespace WatchProto
{
	constexpr uint32_t kMagic = 0x31484357u; /* 'WCH1' */
	constexpr uint16_t kPort = 27865;
	constexpr uint32_t kVersion = 6;

	enum : uint32_t
	{
		CmdList  = 1, /* Linux: unused (portal picker owns selection) */
		CmdStart = 2, /* Linux: open portal picker (id ignored); Win: window id */
		CmdStop  = 3,
		CmdPing  = 4,

		MsgHello   = 10,
		MsgWindows = 11,
		MsgStatus  = 13,
		MsgErr     = 14,
		MsgPong    = 15,
	};

	struct Header
	{
		uint32_t magic;
		uint32_t type;
		uint32_t nbytes;
	};

	constexpr const char* kShmUnixPath = "/dev/shm/gw2igh-watch-frame";
	constexpr uint32_t kShmMagic = 0x4D485357u; /* 'WSHM' */
	/* Match ~720p so the Mirror window is not upscaling mush. */
	constexpr uint32_t kMaxW = 1280;
	constexpr uint32_t kMaxH = 720;
	constexpr uint32_t kNoSlot = 0xFFFFFFFFu;
	constexpr size_t   kShmHeaderBytes = 4096;
	constexpr size_t   kSlotBytes = static_cast<size_t>(kMaxW) * kMaxH * 4;
	constexpr size_t   kShmBytes = kShmHeaderBytes + 2 * kSlotBytes;

	struct ShmHeader
	{
		uint32_t magic;
		uint32_t version;
		uint32_t seq;
		uint32_t w;
		uint32_t h;
		uint32_t stride;
		uint32_t slot;
		uint32_t capturing;
		uint32_t reading;
	};

	inline uint8_t* ShmSlotPixels(uint8_t* base, uint32_t slot)
	{
		return base + kShmHeaderBytes + (slot & 1u) * kSlotBytes;
	}
	inline const uint8_t* ShmSlotPixels(const uint8_t* base, uint32_t slot)
	{
		return base + kShmHeaderBytes + (slot & 1u) * kSlotBytes;
	}
}
