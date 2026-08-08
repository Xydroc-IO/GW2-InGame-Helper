#include "HomePage.h"

#include "AddonPaths.h"
#include "HelperThemeCss.h"
#include "UiChrome.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

/* Embedded via ld -r -b binary (see Makefile). */
extern "C" {
	extern const unsigned char _binary_home_logo_png_start[];
	extern const unsigned char _binary_home_logo_png_end[];
	extern const unsigned char _binary_home_cover_jpg_start[];
	extern const unsigned char _binary_home_cover_jpg_end[];
}

namespace
{
	static constexpr const char* kHomePageVersion = "2232";

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
		if (n > 0)
			WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	std::string PathToFileUrl(const std::wstring& path)
	{
		std::string utf8 = WideToUtf8(path);
		for (char& c : utf8)
		{
			if (c == '\\')
				c = '/';
		}
		if (utf8.size() >= 2 && utf8[1] == ':')
			return std::string("file:///") + utf8;
		return std::string("file://") + utf8;
	}

	bool WriteBytes(const std::wstring& path, const void* data, DWORD len)
	{
		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(out, data, len, &written, nullptr);
		CloseHandle(out);
		return ok && written == len;
	}
}

std::string HomePage::EnsureFileUrl(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return {};

	CreateDirectoryW(addonDir.c_str(), nullptr);
	const std::wstring pages = AddonPaths::EnsureUnder(addonDir, L"pages");
	const std::wstring path = pages + L"\\helper-home.html";
	const std::wstring verPath = pages + L"\\helper-home.ver";
	const std::wstring logoPath = pages + L"\\home-logo.png";
	const std::wstring coverPath = pages + L"\\home-cover.jpg";

	bool assetsCurrent = false;
	HANDLE verFile = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (verFile != INVALID_HANDLE_VALUE)
	{
		char buf[32]{};
		DWORD read = 0;
		if (ReadFile(verFile, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
		{
			buf[read] = 0;
			while (read > 0 && (buf[read - 1] == '\n' || buf[read - 1] == '\r'))
				buf[--read] = 0;
			if (std::strcmp(buf, kHomePageVersion) == 0)
				assetsCurrent = true;
		}
		CloseHandle(verFile);
	}

	const bool missingAssets =
		GetFileAttributesW(logoPath.c_str()) == INVALID_FILE_ATTRIBUTES ||
		GetFileAttributesW(coverPath.c_str()) == INVALID_FILE_ATTRIBUTES;

	/* Always rewrite HTML — version stamps alone left Wine/Proton installs on
	   stale helper-home (Browse/Favorites pills). Assets only when needed. */
	{
		std::string html = Html();
		const std::string fill = UiChrome::FillFileUrl(addonDir);
		std::string themeCss = HelperThemeCss::FillBackgroundCss(fill.c_str());
		themeCss += UiChrome::DecorCss(addonDir);
		HelperThemeCss::AppendUserRoot(themeCss);
		if (!themeCss.empty())
		{
			const std::string inject = std::string("<style>\n") + themeCss + "</style>\n</head>";
			const size_t pos = html.find("</head>");
			if (pos != std::string::npos)
				html.replace(pos, 7, inject);
		}
		if (!WriteBytes(path, html.data(), static_cast<DWORD>(html.size())))
			return {};
	}

	if (!assetsCurrent || missingAssets)
	{
		const DWORD logoLen = static_cast<DWORD>(_binary_home_logo_png_end - _binary_home_logo_png_start);
		const DWORD coverLen = static_cast<DWORD>(_binary_home_cover_jpg_end - _binary_home_cover_jpg_start);
		if (!WriteBytes(logoPath, _binary_home_logo_png_start, logoLen))
			return {};
		if (!WriteBytes(coverPath, _binary_home_cover_jpg_start, coverLen))
			return {};
		WriteBytes(verPath, kHomePageVersion, static_cast<DWORD>(std::strlen(kHomePageVersion)));
	}

	if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		return {};
	/* Cache-bust so CEF does not keep serving an old file:// document. */
	return PathToFileUrl(path) + "?v=" + kHomePageVersion;
}
