#pragma once

#include <string>

/* Built-in how-to page shown as the addon Home / landing page. */
namespace HomePage
{
	const char* Html();

	/* Write helper-home.html under addonDir and return a short file:/// URL
	   (data: URLs are too large for the IPC command buffer). */
	std::string EnsureFileUrl(const std::wstring& addonDir);
}
