#include "RaidFood.h"

#include "AddonPaths.h"
#include "HelperThemeCss.h"
#include "UiChrome.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	static constexpr const char* kRaidFoodVersion = "7";

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
}

std::string RaidFood::EnsureFileUrl(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return {};

	const std::wstring pages = AddonPaths::EnsureUnder(addonDir, L"pages");
	const std::wstring path = pages + L"\\raid-food.html";
	const std::wstring verPath = pages + L"\\raid-food.ver";

	char buf[64] = {};
	HANDLE vin = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (vin != INVALID_HANDLE_VALUE)
	{
		DWORD read = 0;
		if (ReadFile(vin, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
		{
			buf[read] = '\0';
			if (std::strcmp(buf, kRaidFoodVersion) == 0)
			{
				CloseHandle(vin);
				HANDLE probe = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (probe != INVALID_HANDLE_VALUE)
				{
					CloseHandle(probe);
					return PathToFileUrl(path);
				}
			}
		}
		CloseHandle(vin);
	}

	const char* htmlSrc = Html();
	std::string html = htmlSrc ? htmlSrc : "";
	{
		const std::string fill = UiChrome::FillFileUrl(addonDir);
		const std::string fillCss = HelperThemeCss::FillBackgroundCss(fill.c_str());
		if (!fillCss.empty())
		{
			const std::string inject = std::string("<style>\n") + fillCss + "</style>\n</head>";
			const size_t pos = html.find("</head>");
			if (pos != std::string::npos)
				html.replace(pos, 7, inject);
		}
	}
	const DWORD len = static_cast<DWORD>(html.size());
	HANDLE hout = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hout == INVALID_HANDLE_VALUE)
		return {};
	DWORD written = 0;
	const BOOL ok = WriteFile(hout, html.data(), len, &written, nullptr);
	CloseHandle(hout);
	if (!ok || written != len)
		return {};

	HANDLE vout = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (vout != INVALID_HANDLE_VALUE)
	{
		DWORD vw = 0;
		WriteFile(vout, kRaidFoodVersion, static_cast<DWORD>(std::strlen(kRaidFoodVersion)), &vw, nullptr);
		CloseHandle(vout);
	}

	return PathToFileUrl(path);
}
