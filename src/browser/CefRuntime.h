#pragma once

/* Private CEF Stable 150 runtime for GW2-InGame-Helper.
   The DLL owns full setup on first helper open:
     1) Prefer a local cef-runtime zip (next to the DLL or in the addon folder)
     2) Else download from kDownloadUrl
     3) SHA-256 verify → extract to addons/.../cef/ → write cef.ver
   Never writes into game bin64/cef. */

namespace CefRuntime
{
	inline constexpr const char* kStamp = "150.0.14";
	inline constexpr const char* kChromium = "150.0.7871.129";
	inline constexpr const char* kZipFileName = "cef-runtime-150-windows64.zip";

	/* Temporary host while testing first-run download (tag 1.0.0.0).
	   Move back to cef-runtime-150/ when that dedicated release is published. */
	inline constexpr const char* kDownloadUrl =
		"https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/"
		"1.0.0.0/cef-runtime-150-windows64.zip";

	inline constexpr const char* kSha256Hex =
		"d08859aa99266566f5ba51be4cacc7ec57265bcc4b84436151410553c7d82943";

	/* Ensures addons/<name>/cef/ is ready (local zip or download). */
	bool EnsureInstalled(const wchar_t* addonDirWide,
		void (*statusFn)(const char* msg));
}
