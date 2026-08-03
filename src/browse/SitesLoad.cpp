#include "SitesInternal.h"

#include "AddonPaths.h"
#include "Globals.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

extern "C" {
	extern const unsigned char _binary_build_sites_json_start[];
	extern const unsigned char _binary_build_sites_json_end[];
}

namespace SitesDetail
{
	SiteDef* gSites = nullptr;
	int gSiteCount = 0;

	namespace
	{
		constexpr const char* kSitesStamp = "s2210";
		constexpr int kMaxBrowsePath = 8;
		constexpr int kMaxCategories = 32;

		struct SiteOwned
		{
			std::string id;
			std::string category;
			std::string label;
			std::string title;
			std::string homeUrl;
			std::string searchPrefix;
			std::string searchSuffix;
			bool hasSearchPrefix = false;
			bool hasSearchSuffix = false;
			std::string browsePath[kMaxBrowsePath];
			int browsePathCount = 0;
			const char* browsePtrs[kMaxBrowsePath]{};
		};

		std::vector<SiteOwned> gOwned;
		std::vector<SiteDef> gDefs;

		struct SectionList
		{
			std::vector<std::string> names;
			std::vector<const char*> ptrs;
		};
		std::unordered_map<std::string, SectionList> gSections;

		/* Minimal fallback so the addon still loads. */
		SiteOwned gFallbackOwned;
		SiteDef gFallbackDef{};
		const char* gFallbackBrowse[1]{};

		void SkipWs(const char*& p, const char* end)
		{
			while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
				++p;
		}

		bool ParseString(const char*& p, const char* end, std::string& out)
		{
			SkipWs(p, end);
			if (p >= end || *p != '"')
				return false;
			++p;
			out.clear();
			while (p < end)
			{
				const char c = *p++;
				if (c == '"')
					return true;
				if (c == '\\' && p < end)
				{
					const char e = *p++;
					switch (e)
					{
					case '"':
					case '\\':
					case '/':
						out.push_back(e);
						break;
					case 'n':
						out.push_back('\n');
						break;
					case 'r':
						out.push_back('\r');
						break;
					case 't':
						out.push_back('\t');
						break;
					case 'u':
						/* Skip \uXXXX — keep as '?' for catalog text. */
						for (int i = 0; i < 4 && p < end; ++i)
							++p;
						out.push_back('?');
						break;
					default:
						out.push_back(e);
						break;
					}
				}
				else
					out.push_back(c);
			}
			return false;
		}

		bool Expect(const char*& p, const char* end, char ch)
		{
			SkipWs(p, end);
			if (p >= end || *p != ch)
				return false;
			++p;
			return true;
		}

		void SkipValue(const char*& p, const char* end)
		{
			SkipWs(p, end);
			if (p >= end)
				return;
			if (*p == '"')
			{
				std::string tmp;
				ParseString(p, end, tmp);
				return;
			}
			if (*p == '{')
			{
				++p;
				int depth = 1;
				while (p < end && depth > 0)
				{
					if (*p == '"')
					{
						std::string tmp;
						ParseString(p, end, tmp);
						continue;
					}
					if (*p == '{')
						++depth;
					else if (*p == '}')
						--depth;
					++p;
				}
				return;
			}
			if (*p == '[')
			{
				++p;
				int depth = 1;
				while (p < end && depth > 0)
				{
					if (*p == '"')
					{
						std::string tmp;
						ParseString(p, end, tmp);
						continue;
					}
					if (*p == '[')
						++depth;
					else if (*p == ']')
						--depth;
					++p;
				}
				return;
			}
			/* null / true / false / number */
			while (p < end && *p != ',' && *p != '}' && *p != ']' &&
				*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t')
				++p;
		}

		bool ParseStringOrNull(const char*& p, const char* end, std::string& out, bool& isNull)
		{
			SkipWs(p, end);
			isNull = false;
			if (p + 4 <= end && std::strncmp(p, "null", 4) == 0)
			{
				p += 4;
				isNull = true;
				out.clear();
				return true;
			}
			return ParseString(p, end, out);
		}

		bool ParseBrowsePath(const char*& p, const char* end, SiteOwned& site)
		{
			SkipWs(p, end);
			if (p >= end || *p != '[')
				return false;
			++p;
			site.browsePathCount = 0;
			while (true)
			{
				SkipWs(p, end);
				if (p < end && *p == ']')
				{
					++p;
					return true;
				}
				std::string part;
				if (!ParseString(p, end, part))
					return false;
				if (site.browsePathCount < kMaxBrowsePath && !part.empty())
				{
					site.browsePath[site.browsePathCount] = std::move(part);
					++site.browsePathCount;
				}
				SkipWs(p, end);
				if (p < end && *p == ',')
				{
					++p;
					continue;
				}
				if (p < end && *p == ']')
				{
					++p;
					return true;
				}
				return false;
			}
		}

		bool ParseSiteObject(const char*& p, const char* end, SiteOwned& site)
		{
			if (!Expect(p, end, '{'))
				return false;
			site = SiteOwned{};
			while (true)
			{
				SkipWs(p, end);
				if (p < end && *p == '}')
				{
					++p;
					return !site.id.empty() && !site.category.empty() &&
						!site.label.empty() && !site.title.empty() && !site.homeUrl.empty();
				}
				std::string key;
				if (!ParseString(p, end, key) || !Expect(p, end, ':'))
					return false;
				if (key == "id")
				{
					if (!ParseString(p, end, site.id))
						return false;
				}
				else if (key == "category")
				{
					if (!ParseString(p, end, site.category))
						return false;
				}
				else if (key == "label")
				{
					if (!ParseString(p, end, site.label))
						return false;
				}
				else if (key == "title")
				{
					if (!ParseString(p, end, site.title))
						return false;
				}
				else if (key == "homeUrl")
				{
					if (!ParseString(p, end, site.homeUrl))
						return false;
				}
				else if (key == "searchUrlPrefix")
				{
					bool isNull = false;
					if (!ParseStringOrNull(p, end, site.searchPrefix, isNull))
						return false;
					site.hasSearchPrefix = !isNull && !site.searchPrefix.empty();
				}
				else if (key == "searchUrlSuffix")
				{
					bool isNull = false;
					if (!ParseStringOrNull(p, end, site.searchSuffix, isNull))
						return false;
					site.hasSearchSuffix = !isNull && !site.searchSuffix.empty();
				}
				else if (key == "browsePath")
				{
					if (!ParseBrowsePath(p, end, site))
						return false;
				}
				else
					SkipValue(p, end);

				SkipWs(p, end);
				if (p < end && *p == ',')
				{
					++p;
					continue;
				}
				if (p < end && *p == '}')
				{
					++p;
					return !site.id.empty() && !site.category.empty() &&
						!site.label.empty() && !site.title.empty() && !site.homeUrl.empty();
				}
				return false;
			}
		}

		bool ParseStringArray(const char*& p, const char* end, std::vector<std::string>& out)
		{
			out.clear();
			if (!Expect(p, end, '['))
				return false;
			while (true)
			{
				SkipWs(p, end);
				if (p < end && *p == ']')
				{
					++p;
					return true;
				}
				std::string s;
				if (!ParseString(p, end, s))
					return false;
				out.push_back(std::move(s));
				SkipWs(p, end);
				if (p < end && *p == ',')
				{
					++p;
					continue;
				}
				if (p < end && *p == ']')
				{
					++p;
					return true;
				}
				return false;
			}
		}

		bool ParseBrowseSections(const char*& p, const char* end)
		{
			if (!Expect(p, end, '{'))
				return false;
			gSections.clear();
			while (true)
			{
				SkipWs(p, end);
				if (p < end && *p == '}')
				{
					++p;
					return true;
				}
				std::string cat;
				if (!ParseString(p, end, cat) || !Expect(p, end, ':'))
					return false;
				SectionList list;
				if (!ParseStringArray(p, end, list.names))
					return false;
				list.ptrs.reserve(list.names.size());
				for (const std::string& n : list.names)
					list.ptrs.push_back(n.c_str());
				gSections.emplace(std::move(cat), std::move(list));
				/* Fix pointers after map move — rebuild below. */
				SkipWs(p, end);
				if (p < end && *p == ',')
				{
					++p;
					continue;
				}
				if (p < end && *p == '}')
				{
					++p;
					/* Rebuild ptrs after all insertions (string addresses stable in map values). */
					for (auto& kv : gSections)
					{
						kv.second.ptrs.clear();
						kv.second.ptrs.reserve(kv.second.names.size());
						for (const std::string& n : kv.second.names)
							kv.second.ptrs.push_back(n.c_str());
					}
					return true;
				}
				return false;
			}
		}

		bool ParseRoot(const std::string& json)
		{
			const char* p = json.data();
			const char* end = json.data() + json.size();
			if (!Expect(p, end, '{'))
				return false;

			gOwned.clear();
			gOwned.reserve(2800);
			gSections.clear();
			bool sawSites = false;

			while (true)
			{
				SkipWs(p, end);
				if (p < end && *p == '}')
				{
					++p;
					break;
				}
				std::string key;
				if (!ParseString(p, end, key) || !Expect(p, end, ':'))
					return false;
				if (key == "browseSections")
				{
					if (!ParseBrowseSections(p, end))
						return false;
				}
				else if (key == "sites")
				{
					if (!Expect(p, end, '['))
						return false;
					sawSites = true;
					while (true)
					{
						SkipWs(p, end);
						if (p < end && *p == ']')
						{
							++p;
							break;
						}
						SiteOwned site;
						if (!ParseSiteObject(p, end, site))
							return false;
						gOwned.push_back(std::move(site));
						SkipWs(p, end);
						if (p < end && *p == ',')
						{
							++p;
							continue;
						}
						if (p < end && *p == ']')
						{
							++p;
							break;
						}
						return false;
					}
				}
				else
					SkipValue(p, end);

				SkipWs(p, end);
				if (p < end && *p == ',')
				{
					++p;
					continue;
				}
				if (p < end && *p == '}')
				{
					++p;
					break;
				}
				return false;
			}

			if (!sawSites || gOwned.empty())
				return false;

			gDefs.clear();
			gDefs.resize(gOwned.size());
			for (size_t i = 0; i < gOwned.size(); ++i)
			{
				SiteOwned& o = gOwned[i];
				SiteDef& d = gDefs[i];
				d.id = o.id.c_str();
				d.category = o.category.c_str();
				d.label = o.label.c_str();
				d.title = o.title.c_str();
				d.homeUrl = o.homeUrl.c_str();
				d.searchUrlPrefix = o.hasSearchPrefix ? o.searchPrefix.c_str() : nullptr;
				d.searchUrlSuffix = o.hasSearchSuffix ? o.searchSuffix.c_str() : nullptr;
				for (int j = 0; j < o.browsePathCount; ++j)
					o.browsePtrs[j] = o.browsePath[j].c_str();
				d.browsePath = o.browsePathCount > 0 ? o.browsePtrs : nullptr;
				d.browsePathCount = o.browsePathCount;
			}
			gSites = gDefs.data();
			gSiteCount = static_cast<int>(gDefs.size());
			return true;
		}

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
