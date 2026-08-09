#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;

/* Window mirror — Win32 GDI on native Windows; Linux X11 via gw2igh-watchd on Wine. */
namespace WatchCapture
{
	struct WindowEntry
	{
		uint64_t    id = 0; /* HWND cast or X11 Window id */
		std::string title;
	};

	void RefreshWindowList();
	const std::vector<WindowEntry>& Windows();
	int RawEnumCount();

	bool Start(uint64_t id); /* id=0 → WGC system picker on native Windows when available */
	bool StartWgcPicker(); /* explicit system picker */
	bool WgcAvailable();
	void Stop(); /* full stop + deferred GPU park/release */
	/* Wine Soft-stop: end capture/watchd only — keep D3D textures alive so Soft-open
	   after a long Mirror session does not tip Wine on SRV Release. */
	void SoftStopCapture();
	bool IsCapturing();
	/* True only while frames can flow (excludes system-picker wait). */
	bool IsStreaming();
	/* True after at least one GPU upload this session. */
	bool HasContent();
	uint64_t TargetId();
	bool ClassicListMode(); /* true when showing/using GDI HWND list */
	void SetClassicListMode(bool on);

	void Tick();

	ID3D11ShaderResourceView* Srv();
	uint32_t ContentW();
	uint32_t ContentH();
	/* UV max for AddImage — texture is max-sized; content is top-left. */
	float ContentU();
	float ContentV();
	/* True while Stop park / deferred GPU free still holds an SRV (Wine reopen). */
	bool GpuParkBusy();
	/* Soft-stop click: stop AddImage immediately without parking yet. */
	void HideContent();
	/* Wine: true when Mirror uploads on a dedicated D3D device (shared SRV on game). */
	bool DedicatedMirrorDevice();
	/* "dedicated" / "fallback: …" — Watch UI so we can see if SHARED stuck. */
	const char* MirrorGpuPathText();

	const char* StatusText();
	bool LastFrameLookedBlank();

	void Shutdown();
}
