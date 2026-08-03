#pragma once

#include <cstddef>

/* Private Elite Insights CLI for Combat Logs (same pattern as CefRuntime):
     1) Prefer a local GW2EICLI.zip (next to the DLL or in the addon folder)
     2) Else query GitHub releases/latest for GW2EICLI.zip (+ asset SHA-256)
     3) If local ei.ver != latest tag → download → verify → extract to ei/
   Elite Insights is MIT (baaron4). Not bundled inside the DLL.
   Fallback constants used only when the GitHub API is unreachable. */

namespace EiRuntime
{
	inline constexpr const char* kZipFileName = "GW2EICLI.zip";
	inline constexpr const char* kCliExeName = "GuildWars2EliteInsights-CLI.exe";

	inline constexpr const char* kLatestApiUrl =
		"https://api.github.com/repos/baaron4/GW2-Elite-Insights-Parser/releases/latest";

	/* Offline fallback if GitHub API is unreachable. */
	inline constexpr const char* kFallbackStamp = "3.26.0.0";
	inline constexpr const char* kFallbackDownloadUrl =
		"https://github.com/baaron4/GW2-Elite-Insights-Parser/releases/download/"
		"v3.26.0.0/GW2EICLI.zip";
	inline constexpr const char* kFallbackSha256Hex =
		"19fb297e7268f3d4078ce12605382e015768c0be7786b1b0adc0960eb9a11b63";

	/* True when addons/<name>/ei/ has a usable CLI (any stamped version). */
	bool IsInstalled(const wchar_t* addonDirWide);

	/* Installed stamp from ei.ver (e.g. "3.26.0.0"), or empty. */
	bool GetInstalledStamp(const wchar_t* addonDirWide, char* out, size_t outLen);

	/* Ensures ei/ matches the newest GitHub release (updates if needed). BLOCKING. */
	bool EnsureInstalled(const wchar_t* addonDirWide,
		void (*statusFn)(const char* msg));

	/* Full path to GuildWars2EliteInsights-CLI.exe under ei/, or empty. */
	bool GetCliPathUtf8(const wchar_t* addonDirWide, char* out, size_t outLen);

	/* Windows/.Wine-prefix .NET 8 shared framework (Core or Desktop). Cached briefly. */
	bool HasDotNet8Runtime();
	/* Force the next HasDotNet8Runtime() call to rescan. */
	void InvalidateDotNet8Cache();

	/* Opens Microsoft’s .NET 8 Desktop Runtime x64 installer (aka.ms). */
	void OpenDotNet8Installer();

	/* True when running under Wine/Proton (ntdll wine export). */
	bool IsWine();
}
