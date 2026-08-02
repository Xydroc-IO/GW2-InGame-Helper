#include "CheatSheets.h"

#include "CheatSheets_Data.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
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

} // namespace

const CheatSheets::Sheet* CheatSheets::All(size_t* outCount)
{
	size_t n = 0;
	const CheatSheetsData::PageSpec* pages = CheatSheetsData::Pages(&n);
	/* Expose meta only — caller iterates All via Find or we return parallel. */
	static Sheet kMeta[40];
	static bool init = false;
	if (!init)
	{
		for (size_t i = 0; i < n && i < 40; ++i)
			kMeta[i] = pages[i].meta;
		init = true;
	}
	if (outCount)
		*outCount = n;
	return kMeta;
}

const CheatSheets::Sheet* CheatSheets::FindByAbout(const char* aboutUrl)
{
	if (!aboutUrl || !aboutUrl[0])
		return nullptr;
	size_t n = 0;
	const CheatSheetsData::PageSpec* pages = CheatSheetsData::Pages(&n);
	for (size_t i = 0; i < n; ++i)
	{
		if (std::strcmp(pages[i].meta.about, aboutUrl) == 0)
			return &All(nullptr)[i];
	}
	return nullptr;
}

std::string CheatSheets::EnsureFileUrl(const std::wstring& addonDir, const Sheet& sheet)
{
	if (addonDir.empty() || !sheet.fileStem || !sheet.version)
		return {};

	size_t n = 0;
	const CheatSheetsData::PageSpec* pages = CheatSheetsData::Pages(&n);
	const CheatSheetsData::PageSpec* spec = nullptr;
	for (size_t i = 0; i < n; ++i)
	{
		if (std::strcmp(pages[i].meta.fileStem, sheet.fileStem) == 0)
		{
			spec = &pages[i];
			break;
		}
	}
	if (!spec || !spec->build)
		return {};

	const std::wstring path = addonDir + L"\\" + std::wstring(sheet.fileStem, sheet.fileStem + std::strlen(sheet.fileStem)) + L".html";
	const std::wstring verPath = addonDir + L"\\" + std::wstring(sheet.fileStem, sheet.fileStem + std::strlen(sheet.fileStem)) + L".ver";

	char buf[64] = {};
	HANDLE vin = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (vin != INVALID_HANDLE_VALUE)
	{
		DWORD read = 0;
		if (ReadFile(vin, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
		{
			buf[read] = '\0';
			/* Trim CR/LF so "10\r\n" still matches version "10". */
			while (read > 0 && (buf[read - 1] == '\r' || buf[read - 1] == '\n' || buf[read - 1] == ' '))
				buf[--read] = '\0';
			if (std::strcmp(buf, sheet.version) == 0)
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

	const std::string html = spec->build();
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
		WriteFile(vout, sheet.version, static_cast<DWORD>(std::strlen(sheet.version)), &vw, nullptr);
		CloseHandle(vout);
	}

	return PathToFileUrl(path);
}

std::string CheatSheets::ResolveAboutUrl(const std::wstring& addonDir, const std::string& url)
{
	const Sheet* sheet = FindByAbout(url.c_str());
	if (!sheet)
		return {};
	return EnsureFileUrl(addonDir, *sheet);
}
