#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

/* Native Windows.Graphics.Capture (system thumbnail picker). Wine uses portal. */
namespace WatchCaptureWgc
{
	bool Available();
	bool IsCapturing();
	bool IsPickerOpen();

	/* Opens GraphicsCapturePicker (STA thread). Returns true if start kicked off. */
	bool StartPicker(std::string& status);
	void Stop();

	/* Pull latest frame into BGRA (scaled ≤ maxW/maxH). */
	bool TakeFrame(std::vector<uint8_t>& bgra, uint32_t& w, uint32_t& h, uint32_t& stride,
		uint32_t maxW, uint32_t maxH);

	void GetStatus(std::string& status);
}
