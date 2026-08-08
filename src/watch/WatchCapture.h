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
	void Stop();
	bool IsCapturing();
	/* True only while frames can flow (excludes system-picker wait). */
	bool IsStreaming();
	uint64_t TargetId();
	bool ClassicListMode(); /* true when showing/using GDI HWND list */
	void SetClassicListMode(bool on);

	void Tick();

	ID3D11ShaderResourceView* Srv();
	uint32_t ContentW();
	uint32_t ContentH();

	const char* StatusText();
	bool LastFrameLookedBlank();

	void Shutdown();
}
