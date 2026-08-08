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

	bool Start(uint64_t id);
	void Stop();
	bool IsCapturing();
	uint64_t TargetId();

	void Tick();

	ID3D11ShaderResourceView* Srv();
	uint32_t ContentW();
	uint32_t ContentH();

	const char* StatusText();
	bool LastFrameLookedBlank();

	void Shutdown();
}
