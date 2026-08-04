#include "CefRuntime.h"
#include "CefRuntimeInternal.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace CefRuntimeDetail
{
bool InstallFromZip(const std::wstring& zipPath, const std::wstring& cefDir,
	void (*statusFn)(const char*), bool deleteZipAfter)
{
	if (!VerifySha256(zipPath, statusFn))
		return false;
	if (!ExtractZip(zipPath, cefDir, statusFn))
		return false;
	if (!WriteStamp(Join(cefDir, L"cef.ver"), CefRuntime::kStamp))
	{
		Status(statusFn, "CEF stamp write failed");
		return false;
	}
	CleanupStrayZipInCef(cefDir);
	if (deleteZipAfter)
		DeleteFileW(zipPath.c_str());
	if (!AlreadyInstalled(cefDir))
	{
		Status(statusFn, "CEF install incomplete");
		return false;
	}
	Status(statusFn, "CEF ready");
	return true;
}

bool FindLocalZip(const std::wstring& addonDir, std::wstring& outPath, bool& deleteAfter)
{
	outPath.clear();
	deleteAfter = false;
	const wchar_t* name = L"cef-runtime-150-windows64.zip";

	const std::wstring dllSide = Join(DllDir(), name);
	const std::wstring addonSide = Join(addonDir, name);
	const std::wstring insideCef = Join(Join(addonDir, L"cef"), name);
	const std::wstring candidates[] = { addonSide, dllSide, insideCef };

	for (const std::wstring& c : candidates)
	{
		if (c.empty())
			continue;
		if (GetFileAttributesW(c.c_str()) == INVALID_FILE_ATTRIBUTES)
			continue;
		outPath = c;
		/* Remove zip only when it was dropped inside cef/ (wrong place). */
		deleteAfter = (c == insideCef);
		return true;
	}
	return false;
}
}

using namespace CefRuntimeDetail;

bool CefRuntime::EnsureInstalled(const wchar_t* addonDirWide, void (*statusFn)(const char* msg))
{
	if (!addonDirWide || !addonDirWide[0])
	{
		Status(statusFn, "CEF addon dir missing");
		return false;
	}

	const std::wstring addonDir = addonDirWide;
	const std::wstring cefDir = Join(addonDir, L"cef");

	if (!MkDirDeep(addonDir))
	{
		Status(statusFn, "CEF addon mkdir failed");
		return false;
	}

	if (AlreadyInstalled(cefDir))
	{
		CleanupStrayZipInCef(cefDir);
		return true;
	}

	/* Never re-hash / re-extract the ~170MB zip just because stamp is missing. */
	if (TreeLooksComplete(cefDir))
	{
		Status(statusFn, "Writing CEF stamp…");
		if (WriteStamp(Join(cefDir, L"cef.ver"), kStamp))
		{
			CleanupStrayZipInCef(cefDir);
			if (AlreadyInstalled(cefDir))
			{
				Status(statusFn, "CEF ready");
				return true;
			}
		}
	}

	std::wstring localZip;
	bool deleteAfter = false;
	if (FindLocalZip(addonDir, localZip, deleteAfter))
	{
		Status(statusFn, "Installing CEF from local zip…");
		return InstallFromZip(localZip, cefDir, statusFn, deleteAfter);
	}

	if (!kDownloadUrl || !kDownloadUrl[0])
	{
		Status(statusFn, "CEF zip missing — place cef-runtime-150-windows64.zip next to the DLL");
		return false;
	}

	int n = MultiByteToWideChar(CP_UTF8, 0, kDownloadUrl, -1, nullptr, 0);
	if (n <= 0)
	{
		Status(statusFn, "CEF URL widen failed");
		return false;
	}
	std::wstring urlW(static_cast<size_t>(n - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, kDownloadUrl, -1, urlW.data(), n);

	const std::wstring tmpZip = Join(addonDir, L"cef-download.tmp");
	DeleteFileW(tmpZip.c_str());

	if (!DownloadToFile(urlW.c_str(), tmpZip, statusFn))
		return false;

	const bool ok = InstallFromZip(tmpZip, cefDir, statusFn, true);
	if (!ok)
		DeleteFileW(tmpZip.c_str());
	return ok;
}

