#include "EiRuntime.h"
#include "EiRuntimeInternal.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>
#include <shellapi.h>

using namespace EiRuntimeDetail;
using EiRuntime::kFallbackStamp;
using EiRuntime::kFallbackDownloadUrl;
using EiRuntime::kFallbackSha256Hex;

bool EiRuntime::IsInstalled(const wchar_t* addonDirWide)
{
	if (!addonDirWide || !addonDirWide[0])
		return false;
	return AlreadyInstalled(Join(addonDirWide, L"ei"));
}

bool EiRuntime::GetInstalledStamp(const wchar_t* addonDirWide, char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return false;
	out[0] = 0;
	if (!addonDirWide || !addonDirWide[0])
		return false;
	std::string stamp;
	if (!ReadStamp(Join(Join(addonDirWide, L"ei"), L"ei.ver"), stamp) || stamp.empty())
		return false;
	if (stamp.size() >= outLen)
		return false;
	std::memcpy(out, stamp.c_str(), stamp.size() + 1);
	return true;
}

bool EiRuntime::GetCliPathUtf8(const wchar_t* addonDirWide, char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return false;
	out[0] = 0;
	if (!addonDirWide || !addonDirWide[0])
		return false;
	const std::wstring eiDir = Join(addonDirWide, L"ei");
	std::wstring cli;
	if (!FindCliRecursive(eiDir, cli, 0))
		return false;
	const std::string utf8 = WideToUtf8(cli);
	if (utf8.empty() || utf8.size() >= outLen)
		return false;
	std::memcpy(out, utf8.c_str(), utf8.size() + 1);
	return true;
}

bool EiRuntime::EnsureInstalled(const wchar_t* addonDirWide, void (*statusFn)(const char* msg))
{
	if (!addonDirWide || !addonDirWide[0])
	{
		Status(statusFn, "EI addon dir missing");
		return false;
	}

	const std::wstring addonDir = addonDirWide;
	const std::wstring eiDir = Join(addonDir, L"ei");

	if (!MkDirDeep(addonDir))
	{
		Status(statusFn, "EI addon mkdir failed");
		return false;
	}

	LatestRelease latest;
	const bool haveLatest = QueryLatestRelease(latest, statusFn);
	if (!haveLatest)
	{
		/* Offline / API down: keep current install, else fallback pin. */
		if (AlreadyInstalled(eiDir))
		{
			CleanupStrayZipInEi(eiDir);
			Status(statusFn, "Elite Insights ready (offline — kept current)");
			return true;
		}

		latest.stamp = kFallbackStamp;
		latest.url = kFallbackDownloadUrl;
		latest.sha256 = kFallbackSha256Hex;
		Status(statusFn, "Using fallback Elite Insights package…");
	}
	else if (MatchesStamp(eiDir, latest.stamp.c_str()))
	{
		CleanupStrayZipInEi(eiDir);
		char msg[96];
		std::snprintf(msg, sizeof(msg), "Elite Insights %s up to date", latest.stamp.c_str());
		Status(statusFn, msg);
		return true;
	}

	std::wstring localZip;
	bool deleteAfter = false;
	if (FindLocalZip(addonDir, localZip, deleteAfter))
	{
		Status(statusFn, "Installing Elite Insights from local zip…");
		if (InstallFromZip(localZip, eiDir, latest.stamp.c_str(), latest.sha256.c_str(),
				statusFn, deleteAfter))
			return true;
		Status(statusFn, "Local EI zip did not match latest hash — downloading…");
	}

	if (latest.url.empty())
	{
		Status(statusFn, "EI zip missing — place GW2EICLI.zip next to the DLL");
		return false;
	}

	int n = MultiByteToWideChar(CP_UTF8, 0, latest.url.c_str(), -1, nullptr, 0);
	if (n <= 0)
	{
		Status(statusFn, "EI URL widen failed");
		return false;
	}
	std::wstring urlW(static_cast<size_t>(n - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, latest.url.c_str(), -1, urlW.data(), n);

	const std::wstring tmpZip = Join(addonDir, L"ei-download.tmp");
	DeleteFileW(tmpZip.c_str());

	if (!DownloadToFile(urlW.c_str(), tmpZip, statusFn))
		return false;

	const bool ok = InstallFromZip(tmpZip, eiDir, latest.stamp.c_str(), latest.sha256.c_str(),
		statusFn, true);
	if (!ok)
		DeleteFileW(tmpZip.c_str());
	return ok;
}

namespace
{
	bool gDotNetCacheValid = false;
	bool gDotNetCacheValue = false;
	DWORD gDotNetCacheMs = 0;

	bool SharedFxHasV8(const std::wstring& fxDir)
	{
		if (fxDir.empty())
			return false;
		WIN32_FIND_DATAW fd{};
		const std::wstring pat = fxDir + L"\\8.*";
		HANDLE h = FindFirstFileW(pat.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		bool ok = false;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				ok = true;
				break;
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
		return ok;
	}

	bool DotNetRootHasV8(const std::wstring& root)
	{
		if (root.empty())
			return false;
		if (SharedFxHasV8(root + L"\\shared\\Microsoft.NETCore.App"))
			return true;
		if (SharedFxHasV8(root + L"\\shared\\Microsoft.WindowsDesktop.App"))
			return true;
		return false;
	}

	bool RegistryHasDotNet8()
	{
		const wchar_t* keys[] = {
			L"SOFTWARE\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.NETCore.App",
			L"SOFTWARE\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.WindowsDesktop.App",
			L"SOFTWARE\\WOW6432Node\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.NETCore.App",
			L"SOFTWARE\\WOW6432Node\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.WindowsDesktop.App",
		};
		for (const wchar_t* sub : keys)
		{
			HKEY hk = nullptr;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub, 0, KEY_READ, &hk) != ERROR_SUCCESS)
				continue;
			DWORD index = 0;
			wchar_t name[128];
			DWORD nameLen = 128;
			bool found = false;
			while (RegEnumValueW(hk, index++, name, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			{
				if (name[0] == L'8' && name[1] == L'.')
				{
					found = true;
					break;
				}
				nameLen = 128;
			}
			RegCloseKey(hk);
			if (found)
				return true;
		}
		return false;
	}

	bool ProbeDotNet8()
	{
		wchar_t pf[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH) > 0 &&
			DotNetRootHasV8(std::wstring(pf) + L"\\dotnet"))
			return true;

		wchar_t pf86[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"ProgramFiles(x86)", pf86, MAX_PATH) > 0 &&
			DotNetRootHasV8(std::wstring(pf86) + L"\\dotnet"))
			return true;

		wchar_t root[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"DOTNET_ROOT", root, MAX_PATH) > 0 && DotNetRootHasV8(root))
			return true;

		wchar_t local[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) > 0 &&
			DotNetRootHasV8(std::wstring(local) + L"\\Microsoft\\dotnet"))
			return true;

		if (RegistryHasDotNet8())
			return true;

		return false;
	}
}

bool EiRuntime::IsWine()
{
	static int sCached = -1;
	if (sCached >= 0)
		return sCached != 0;
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	sCached = (ntdll && GetProcAddress(ntdll, "wine_get_version")) ? 1 : 0;
	return sCached != 0;
}

void EiRuntime::InvalidateDotNet8Cache()
{
	gDotNetCacheValid = false;
	gDotNetCacheMs = 0;
}

bool EiRuntime::HasDotNet8Runtime()
{
	const DWORD now = GetTickCount();
	if (gDotNetCacheValid && (now - gDotNetCacheMs) < 5000u)
		return gDotNetCacheValue;
	gDotNetCacheValue = ProbeDotNet8();
	gDotNetCacheMs = now;
	gDotNetCacheValid = true;
	return gDotNetCacheValue;
}

void EiRuntime::OpenDotNet8Installer()
{
	/* Direct x64 Desktop Runtime installer — works on Windows and in Wine/Proton prefixes. */
	ShellExecuteA(nullptr, "open",
		"https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe",
		nullptr, nullptr, SW_SHOWNORMAL);
}
