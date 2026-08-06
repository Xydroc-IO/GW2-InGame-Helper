#include "UiChrome.h"

#include "AddonPaths.h"
#include "Globals.h"

#include "miniz.h"
#include "nexus/Nexus.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

/* ld -r -b binary build/ui_chrome.zip */
extern "C" const unsigned char _binary_build_ui_chrome_zip_start[];
extern "C" const unsigned char _binary_build_ui_chrome_zip_end[];
/* GNU ld: value of this symbol IS the byte size (not a pointer to size). */
extern "C" const unsigned char _binary_build_ui_chrome_zip_size[];

namespace
{
	constexpr const char* kPackStamp = "uc14";
	constexpr int kChromeIds[] = {
		155985, 155981, 156022, 156008, 156009, 156010, 155967, 156260, 155014
	};
	/* Named pack files (not numeric DAT ids). */
	constexpr const char* kChromeNamed[] = {
		"button-exit.png", "button-exit-active.png", "crest-hero.png",
		"panel-wash.png", "title-bar.png", "panel-edge.png", "ink-edge.png"
	};

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

	std::wstring ChromeDir(const std::wstring& addonDir)
	{
		return addonDir + L"\\ui-chrome";
	}

	bool WriteBytes(const std::wstring& path, const void* data, DWORD len)
	{
		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(out, data, len, &written, nullptr);
		CloseHandle(out);
		return ok && written == len;
	}

	bool StampMatches(const std::wstring& verPath)
	{
		HANDLE vin = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (vin == INVALID_HANDLE_VALUE)
			return false;
		char buf[32]{};
		DWORD got = 0;
		bool ok = false;
		if (ReadFile(vin, buf, sizeof(buf) - 1, &got, nullptr) && got > 0)
			ok = (std::strncmp(buf, kPackStamp, std::strlen(kPackStamp)) == 0);
		CloseHandle(vin);
		return ok;
	}

	bool FileExists(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		CloseHandle(h);
		return true;
	}

	bool ExtractPack(const std::wstring& addonDir)
	{
		/* Prefer size symbol (GNU ld) — more reliable than end-start across PE refptrs. */
		const unsigned char* begin = _binary_build_ui_chrome_zip_start;
		size_t size = static_cast<size_t>(
			_binary_build_ui_chrome_zip_end - _binary_build_ui_chrome_zip_start);
		const size_t sizeSym = reinterpret_cast<size_t>(&_binary_build_ui_chrome_zip_size);
		if (sizeSym > 64 && sizeSym < (64ull * 1024ull * 1024ull))
			size = sizeSym;
		if (!begin || size < 64)
		{
			CreateDirectoryW(addonDir.c_str(), nullptr);
			CreateDirectoryW(ChromeDir(addonDir).c_str(), nullptr);
			WriteBytes(ChromeDir(addonDir) + L"\\extract.err", "bad blob", 8);
			return false;
		}

		const std::wstring dir = ChromeDir(addonDir);
		const std::wstring verPath = dir + L"\\ui-chrome.ver";
		const std::wstring probe = dir + L"\\button-exit.png";

		if (StampMatches(verPath) && FileExists(probe))
			return true;

		CreateDirectoryW(addonDir.c_str(), nullptr);
		CreateDirectoryW(dir.c_str(), nullptr);

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_mem(&zip, begin, size, 0))
		{
			WriteBytes(dir + L"\\extract.err", "zip init", 8);
			return false;
		}

		bool ok = true;
		const mz_uint n = mz_zip_reader_get_num_files(&zip);
		for (mz_uint i = 0; i < n; ++i)
		{
			mz_zip_archive_file_stat st{};
			if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
				continue;
			std::string name = st.m_filename;
			if (name.empty() || name.find("..") != std::string::npos ||
				name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
			{
				ok = false;
				break;
			}
			size_t outLen = 0;
			void* mem = mz_zip_reader_extract_to_heap(&zip, i, &outLen, 0);
			if (!mem)
			{
				ok = false;
				break;
			}
			const std::wstring outPath = dir + L"\\" + std::wstring(name.begin(), name.end());
			const bool wrote = WriteBytes(outPath, mem, static_cast<DWORD>(outLen));
			mz_free(mem);
			if (!wrote)
			{
				ok = false;
				break;
			}
		}
		mz_zip_reader_end(&zip);
		if (!ok)
		{
			WriteBytes(dir + L"\\extract.err", "extract", 7);
			return false;
		}

		return WriteBytes(verPath, kPackStamp, static_cast<DWORD>(std::strlen(kPackStamp)));
	}
}

void UiChrome::MakeTexId(int assetId, char* out, size_t outLen)
{
	std::snprintf(out, outLen, "GW2IGH_CHROME_%d", assetId);
}

void UiChrome::MakeNamedTexId(const char* fileStem, char* out, size_t outLen)
{
	std::snprintf(out, outLen, "GW2IGH_CHROME_%s", fileStem ? fileStem : "x");
}

bool UiChrome::Ensure(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return false;
	return ExtractPack(addonDir);
}

std::wstring UiChrome::PngPath(const std::wstring& addonDir, int assetId)
{
	if (addonDir.empty() || assetId <= 0)
		return {};
	char name[32];
	std::snprintf(name, sizeof(name), "%d.png", assetId);
	return NamedPngPath(addonDir, name);
}

std::wstring UiChrome::NamedPngPath(const std::wstring& addonDir, const char* fileName)
{
	if (addonDir.empty() || !fileName || !fileName[0])
		return {};
	std::wstring path = ChromeDir(addonDir) + L"\\";
	for (const char* p = fileName; *p; ++p)
		path.push_back(static_cast<wchar_t>(*p));
	if (!FileExists(path))
		return {};
	return path;
}

std::string UiChrome::FillFileUrl(const std::wstring& addonDir, int assetId)
{
	Ensure(addonDir);
	/* Prefer opaque rectangular wash — full 155985 has feathered alpha edges that
	   leave black gaps at the sides/bottom of CEF pages under background-size:cover. */
	const std::wstring wash = NamedPngPath(addonDir, "panel-wash.png");
	if (!wash.empty())
		return PathToFileUrl(wash);
	if (assetId <= 0)
		assetId = 155985;
	const std::wstring path = PngPath(addonDir, assetId);
	if (path.empty())
		return {};
	return PathToFileUrl(path);
}

void UiChrome::WarmTextures(const std::wstring& addonDir)
{
	/* Always extract first — do not gate on Texture APIs (they may be late-bound). */
	if (!Ensure(addonDir))
	{
		if (G::API && G::API->Log)
			G::API->Log(LOGL_WARNING, ADDON_NAME, "ui-chrome pack extract failed");
		return;
	}
	if (!G::API || !G::API->Textures_GetOrCreateFromFile)
		return;
	for (int id : kChromeIds)
	{
		const std::wstring path = PngPath(addonDir, id);
		if (path.empty())
			continue;
		char texId[48];
		MakeTexId(id, texId, sizeof(texId));
		const std::string utf8 = WideToUtf8(path);
		G::API->Textures_GetOrCreateFromFile(texId, utf8.c_str());
	}
	for (const char* name : kChromeNamed)
	{
		const std::wstring path = NamedPngPath(addonDir, name);
		if (path.empty())
			continue;
		/* Stem without .png for stable tex id. */
		char stem[64];
		std::snprintf(stem, sizeof(stem), "%s", name);
		if (char* dot = std::strrchr(stem, '.'))
			*dot = '\0';
		char texId[80];
		MakeNamedTexId(stem, texId, sizeof(texId));
		const std::string utf8 = WideToUtf8(path);
		G::API->Textures_GetOrCreateFromFile(texId, utf8.c_str());
	}
}
