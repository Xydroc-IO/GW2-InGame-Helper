#include "SitesLoadInternal.h"

#include "AddonPaths.h"
#include "Globals.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

extern "C" {
	extern const unsigned char _binary_build_sites_json_start[];
	extern const unsigned char _binary_build_sites_json_end[];
}

namespace SitesDetail
{
	SiteDef* gSites = nullptr;
	int gSiteCount = 0;

	void InstallFallback()
	{
		ClearCatalog();
		gFallbackOwned = SiteOwned{};
		gFallbackOwned.id = "home";
		gFallbackOwned.category = "Help";
		gFallbackOwned.label = "How to use";
		gFallbackOwned.title = "How to use";
		gFallbackOwned.homeUrl = "about:helper-home";
		gFallbackOwned.browsePath[0] = "Getting Started";
		gFallbackOwned.browsePathCount = 1;
		gFallbackBrowse[0] = gFallbackOwned.browsePath[0].c_str();

		gFallbackDef.id = gFallbackOwned.id.c_str();
		gFallbackDef.category = gFallbackOwned.category.c_str();
		gFallbackDef.label = gFallbackOwned.label.c_str();
		gFallbackDef.title = gFallbackOwned.title.c_str();
		gFallbackDef.homeUrl = gFallbackOwned.homeUrl.c_str();
		gFallbackDef.searchUrlPrefix = nullptr;
		gFallbackDef.searchUrlSuffix = nullptr;
		gFallbackDef.browsePath = gFallbackBrowse;
		gFallbackDef.browsePathCount = 1;

		gSites = &gFallbackDef;
		gSiteCount = 1;

		SectionList help;
		help.names = { "Getting Started", "ArenaNet", "Other" };
		for (const std::string& n : help.names)
			help.ptrs.push_back(n.c_str());
		gSections.emplace("Help", std::move(help));
	}

	std::wstring SitesPathW()
	{
		return AddonPaths::DataDir() + L"\\sites.json";
	}

	bool ExtractEmbeddedSites()
	{
		const unsigned char* begin = _binary_build_sites_json_start;
		const unsigned char* end = _binary_build_sites_json_end;
		if (end <= begin)
			return false;
		const size_t size = static_cast<size_t>(end - begin);
		const std::wstring path = SitesPathW();
		const std::wstring verPath = path + L".ver";

		bool stampOk = false;
		HANDLE verIn = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (verIn != INVALID_HANDLE_VALUE)
		{
			char buf[32]{};
			DWORD got = 0;
			if (ReadFile(verIn, buf, sizeof(buf) - 1, &got, nullptr) && got > 0)
				stampOk = (std::strncmp(buf, kSitesStamp, std::strlen(kSitesStamp)) == 0);
			CloseHandle(verIn);
		}

		HANDLE existing = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (existing != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER li{};
			const bool same = stampOk && GetFileSizeEx(existing, &li) &&
				static_cast<size_t>(li.QuadPart) == size;
			CloseHandle(existing);
			if (same)
				return true;
		}

		const std::wstring dir = AddonPaths::DataDir();
		CreateDirectoryW(dir.c_str(), nullptr);

		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(out, begin, static_cast<DWORD>(size), &written, nullptr);
		CloseHandle(out);
		if (!ok || written != size)
			return false;

		HANDLE verOut = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (verOut != INVALID_HANDLE_VALUE)
		{
			DWORD vw = 0;
			WriteFile(verOut, kSitesStamp, static_cast<DWORD>(std::strlen(kSitesStamp)), &vw, nullptr);
			CloseHandle(verOut);
		}
		return true;
	}

	bool ReadFileUtf8(const std::wstring& path, std::string& out)
	{
		HANDLE in = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (in == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER li{};
		if (!GetFileSizeEx(in, &li) || li.QuadPart <= 0 || li.QuadPart > 32 * 1024 * 1024)
		{
			CloseHandle(in);
			return false;
		}
		out.assign(static_cast<size_t>(li.QuadPart), '\0');
		DWORD got = 0;
		const BOOL ok = ReadFile(in, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
		CloseHandle(in);
		if (!ok || got != out.size())
			return false;
		return true;
	}

	void ClearCatalog()
	{
		gDefs.clear();
		gOwned.clear();
		gSections.clear();
		gSites = nullptr;
		gSiteCount = 0;
	}

	bool LoadCatalog()
	{
		ClearCatalog();
		(void)AddonPaths::DataDir();
		if (!ExtractEmbeddedSites())
		{
			if (G::API && G::API->Log)
				G::API->Log(LOGL_WARNING, ADDON_NAME, "sites.json extract failed — using fallback catalog");
			InstallFallback();
			return false;
		}

		std::string json;
		if (!ReadFileUtf8(SitesPathW(), json) || !ParseRoot(json))
		{
			if (G::API && G::API->Log)
				G::API->Log(LOGL_WARNING, ADDON_NAME, "sites.json parse failed — using fallback catalog");
			InstallFallback();
			return false;
		}

		if (G::API && G::API->Log)
		{
			char buf[96];
			std::snprintf(buf, sizeof(buf), "Loaded sites.json (%d entries)", gSiteCount);
			G::API->Log(LOGL_INFO, ADDON_NAME, buf);
		}
		return true;
	}

	const char* const* BrowseSectionsFor(const char* category, size_t* outCount)
	{
		if (outCount)
			*outCount = 0;
		if (!category || !category[0])
			return nullptr;
		const auto it = gSections.find(category);
		if (it == gSections.end() || it->second.ptrs.empty())
			return nullptr;
		if (outCount)
			*outCount = it->second.ptrs.size();
		return it->second.ptrs.data();
	}
}

const char* const* Sites::BrowseSections(const char* category, size_t* outCount)
{
	return SitesDetail::BrowseSectionsFor(category, outCount);
}
