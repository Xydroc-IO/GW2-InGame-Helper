#include "AddonPaths.h"

#include "Globals.h"

#include <cstring>

#include <windows.h>

namespace
{
	std::wstring Utf8ToWide(const char* utf8)
	{
		if (!utf8 || !utf8[0])
			return {};
		int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring out(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
		return out;
	}

	void SlashToBack(std::wstring& w)
	{
		for (wchar_t& c : w)
		{
			if (c == L'/')
				c = L'\\';
		}
	}

	std::wstring ModuleDir()
	{
		wchar_t path[MAX_PATH]{};
		if (G::Self && GetModuleFileNameW(G::Self, path, MAX_PATH))
		{
			std::wstring full = path;
			const size_t slash = full.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
				return full.substr(0, slash);
		}
		return L"";
	}

	std::wstring EnsureDir(const std::wstring& dir)
	{
		if (!dir.empty())
			CreateDirectoryW(dir.c_str(), nullptr);
		return dir;
	}
}

std::wstring AddonPaths::DataDir()
{
	/* Nexus addon data folder — typically <GW2>/addons/GW2-InGame-Helper */
	if (G::API && G::API->Paths_GetAddonDirectory)
	{
		const char* d = G::API->Paths_GetAddonDirectory(ADDON_NAME);
		if (d && d[0])
		{
			std::wstring w = Utf8ToWide(d);
			SlashToBack(w);
			return EnsureDir(w);
		}
	}

	/* Fallback: sibling folder next to the DLL (never write into addons/ root). */
	const std::wstring mod = ModuleDir();
	if (!mod.empty())
		return EnsureDir(mod + L"\\GW2-InGame-Helper");

	wchar_t tmp[MAX_PATH]{};
	GetTempPathW(MAX_PATH, tmp);
	return EnsureDir(std::wstring(tmp) + L"GW2-InGame-Helper");
}

std::string AddonPaths::DataDirUtf8()
{
	const std::wstring w = DataDir();
	if (w.empty())
		return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (n <= 0)
		return {};
	std::string out(static_cast<size_t>(n - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
	return out;
}
