#include "Sites.h"
#include "SitesInternal.h"

#include "AddonPaths.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <windows.h>

using SitesRuntimeDetail::FolderIdKnown;
using SitesRuntimeDetail::gFavoriteCount;
using SitesRuntimeDetail::gFavoriteFolderIds;
using SitesRuntimeDetail::gFavoriteFolders;
using SitesRuntimeDetail::gFavoriteFolderCount;
using SitesRuntimeDetail::gFavoriteGeneration;
using SitesRuntimeDetail::gFavoriteTitles;
using SitesRuntimeDetail::gFavoriteUrls;
using SitesRuntimeDetail::gFavoriteNextFolderId;
using SitesRuntimeDetail::kMaxFavoriteFolders;
using SitesRuntimeDetail::kMaxFavoriteTitle;
using SitesRuntimeDetail::kMaxFavoriteUrl;
using SitesRuntimeDetail::kMaxFavorites;
using SitesRuntimeDetail::kUnfiledFavoriteFolderId;

namespace
{
	std::wstring FavoritesPathW()
	{
		return AddonPaths::ConfigDir() + L"\\favorites.json";
	}

	bool WriteUtf8File(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		CloseHandle(h);
		return ok && written == data.size();
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 512 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok)
			return {};
		out.resize(read);
		return out;
	}

	std::string JsonEsc(const char* s)
	{
		std::string o;
		if (!s)
			return o;
		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
		{
			const char c = static_cast<char>(*p);
			if (c == '\\' || c == '"')
			{
				o.push_back('\\');
				o.push_back(c);
			}
			else if (c == '\n')
				o += "\\n";
			else if (static_cast<unsigned char>(c) < 0x20)
				continue;
			else
				o.push_back(c);
		}
		return o;
	}

	std::string ParseJsonString(const std::string& json, size_t& i)
	{
		while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
			++i;
		if (i >= json.size() || json[i] != '"')
			return {};
		++i;
		std::string out;
		while (i < json.size())
		{
			const char c = json[i++];
			if (c == '"')
				break;
			if (c == '\\' && i < json.size())
			{
				const char e = json[i++];
				if (e == 'n')
					out.push_back('\n');
				else if (e == 't')
					out.push_back('\t');
				else
					out.push_back(e);
			}
			else
				out.push_back(c);
		}
		return out;
	}

	int ParseJsonInt(const std::string& json, size_t& i)
	{
		while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
			++i;
		char* end = nullptr;
		const long v = std::strtol(json.c_str() + static_cast<long>(i), &end, 10);
		if (!end || end == json.c_str() + static_cast<long>(i))
			return 0;
		i = static_cast<size_t>(end - json.c_str());
		return static_cast<int>(v);
	}

	bool LoadFromJson(const std::string& json)
	{
		gFavoriteCount = 0;
		gFavoriteFolderCount = 0;
		gFavoriteNextFolderId = 1;
		for (int i = 0; i < kMaxFavorites; ++i)
		{
			gFavoriteUrls[i][0] = 0;
			gFavoriteTitles[i][0] = 0;
			gFavoriteFolderIds[i] = kUnfiledFavoriteFolderId;
		}

		auto FindKey = [](const std::string& obj, const char* key, size_t from = 0) -> size_t {
			std::string pat = "\"";
			pat += key;
			pat += "\"";
			return obj.find(pat, from);
		};

		const size_t foldersKey = FindKey(json, "folders");
		if (foldersKey != std::string::npos)
		{
			size_t i = json.find('[', foldersKey);
			if (i != std::string::npos)
			{
				++i;
				while (i < json.size() && gFavoriteFolderCount < kMaxFavoriteFolders)
				{
					while (i < json.size() && strchr(" \t\r\n,", json[i]))
						++i;
					if (i >= json.size() || json[i] == ']')
						break;
					if (json[i] != '{')
						break;
					const size_t objEnd = json.find('}', i);
					if (objEnd == std::string::npos)
						break;
					const std::string obj = json.substr(i, objEnd - i + 1);
					size_t p = FindKey(obj, "id");
					int id = 0;
					if (p != std::string::npos)
					{
						p = obj.find(':', p);
						if (p != std::string::npos)
						{
							++p;
							id = ParseJsonInt(obj, p);
						}
					}
					p = FindKey(obj, "name");
					std::string name;
					if (p != std::string::npos)
					{
						p = obj.find(':', p);
						if (p != std::string::npos)
						{
							++p;
							name = ParseJsonString(obj, p);
						}
					}
					if (id > 0 && !name.empty() && name.size() < sizeof(gFavoriteFolders[0].name))
					{
						gFavoriteFolders[gFavoriteFolderCount].id = id;
						std::snprintf(gFavoriteFolders[gFavoriteFolderCount].name,
							sizeof(gFavoriteFolders[0].name), "%s", name.c_str());
						++gFavoriteFolderCount;
						if (id >= gFavoriteNextFolderId)
							gFavoriteNextFolderId = id + 1;
					}
					i = objEnd + 1;
				}
			}
		}

		const size_t itemsKey = FindKey(json, "items");
		if (itemsKey != std::string::npos)
		{
			size_t i = json.find('[', itemsKey);
			if (i != std::string::npos)
			{
				++i;
				while (i < json.size() && gFavoriteCount < kMaxFavorites)
				{
					while (i < json.size() && strchr(" \t\r\n,", json[i]))
						++i;
					if (i >= json.size() || json[i] == ']')
						break;
					if (json[i] != '{')
						break;
					const size_t objEnd = json.find('}', i);
					if (objEnd == std::string::npos)
						break;
					const std::string obj = json.substr(i, objEnd - i + 1);
					size_t p = FindKey(obj, "url");
					std::string url;
					if (p != std::string::npos)
					{
						p = obj.find(':', p);
						if (p != std::string::npos)
						{
							++p;
							url = ParseJsonString(obj, p);
						}
					}
					if (url.empty())
					{
						p = FindKey(obj, "id");
						if (p != std::string::npos)
						{
							p = obj.find(':', p);
							if (p != std::string::npos)
							{
								++p;
								url = ParseJsonString(obj, p);
							}
						}
					}
					p = FindKey(obj, "title");
					std::string title;
					if (p != std::string::npos)
					{
						p = obj.find(':', p);
						if (p != std::string::npos)
						{
							++p;
							title = ParseJsonString(obj, p);
						}
					}
					p = FindKey(obj, "folder");
					int folder = kUnfiledFavoriteFolderId;
					if (p != std::string::npos)
					{
						p = obj.find(':', p);
						if (p != std::string::npos)
						{
							++p;
							folder = ParseJsonInt(obj, p);
						}
					}
					if (!url.empty() && url.size() < kMaxFavoriteUrl)
					{
						if (!FolderIdKnown(folder))
							folder = kUnfiledFavoriteFolderId;
						std::snprintf(gFavoriteUrls[gFavoriteCount], kMaxFavoriteUrl, "%s",
							url.c_str());
						if (!title.empty() && title.size() < kMaxFavoriteTitle)
							std::snprintf(gFavoriteTitles[gFavoriteCount], kMaxFavoriteTitle, "%s",
								title.c_str());
						else
							gFavoriteTitles[gFavoriteCount][0] = 0;
						gFavoriteFolderIds[gFavoriteCount] = folder;
						++gFavoriteCount;
					}
					i = objEnd + 1;
				}
			}
		}
		++gFavoriteGeneration;
		return true;
	}
}

void Sites::SaveFavoritesStore()
{
	std::string out = "{\n  \"version\": 2,\n  \"folders\": [\n";
	for (int i = 0; i < gFavoriteFolderCount; ++i)
	{
		if (i)
			out += ",\n";
		out += "    {\"id\":";
		out += std::to_string(gFavoriteFolders[i].id);
		out += ",\"name\":\"";
		out += JsonEsc(gFavoriteFolders[i].name);
		out += "\"}";
	}
	out += "\n  ],\n  \"items\": [\n";
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (i)
			out += ",\n";
		out += "    {\"title\":\"";
		out += JsonEsc(gFavoriteTitles[i]);
		out += "\",\"url\":\"";
		out += JsonEsc(gFavoriteUrls[i]);
		out += "\",\"folder\":";
		out += std::to_string(gFavoriteFolderIds[i]);
		out += "}";
	}
	out += "\n  ]\n}\n";
	WriteUtf8File(FavoritesPathW(), out);
}

void Sites::LoadFavoritesStore()
{
	const std::string json = ReadUtf8File(FavoritesPathW());
	if (!json.empty())
	{
		LoadFromJson(json);
		SitesRuntimeDetail::MigrateFavoriteSiteIds();
		return;
	}
	/* Migrate legacy FavoriteIds= from settings (already parsed into memory). */
	if (gFavoriteCount > 0)
	{
		SitesRuntimeDetail::MigrateFavoriteSiteIds();
		SaveFavoritesStore();
	}
}

void Sites::ParseFavorites(const char* csv)
{
	gFavoriteCount = 0;
	gFavoriteFolderCount = 0;
	gFavoriteNextFolderId = 1;
		++gFavoriteGeneration;
	if (!csv || !csv[0])
		return;
	const char* p = csv;
	while (*p && gFavoriteCount < kMaxFavorites)
	{
		while (*p == ' ' || *p == ',')
			++p;
		if (!*p)
			break;
		const char* start = p;
		while (*p && *p != ',')
			++p;
		size_t len = static_cast<size_t>(p - start);
		while (len > 0 && start[len - 1] == ' ')
			--len;
		if (len == 0 || len >= kMaxFavoriteUrl)
			continue;
		std::memcpy(gFavoriteUrls[gFavoriteCount], start, len);
		gFavoriteUrls[gFavoriteCount][len] = 0;
		gFavoriteTitles[gFavoriteCount][0] = 0;
		gFavoriteFolderIds[gFavoriteCount] = kUnfiledFavoriteFolderId;
		++gFavoriteCount;
	}
}

void Sites::SerializeFavorites(char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return;
	out[0] = 0;
	size_t used = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		const int si = Sites::FavoriteSiteIndex(i);
		const char* id = nullptr;
		if (si >= 0)
		{
			size_t n = 0;
			const SiteDef* sites = Sites::All(&n);
			if (sites && si < static_cast<int>(n))
				id = sites[si].id;
		}
		if (!id || !id[0])
			continue;
		const size_t idLen = std::strlen(id);
		const size_t need = idLen + (used ? 1u : 0u);
		if (used + need + 1 >= outLen)
			break;
		if (used)
			out[used++] = ',';
		std::memcpy(out + used, id, idLen);
		used += idLen;
		out[used] = 0;
	}
}

void Sites::PruneFavorites()
{
	SitesRuntimeDetail::MigrateFavoriteSiteIds();
	int w = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (!gFavoriteUrls[i][0])
			continue;
		bool dup = false;
		for (int j = 0; j < w; ++j)
		{
			if (SitesRuntimeDetail::FavoriteUrlsMatch(gFavoriteUrls[j], gFavoriteUrls[i]))
			{
				dup = true;
				break;
			}
		}
		if (dup)
			continue;
		if (w != i)
		{
			std::snprintf(gFavoriteUrls[w], kMaxFavoriteUrl, "%s", gFavoriteUrls[i]);
			std::snprintf(gFavoriteTitles[w], kMaxFavoriteTitle, "%s", gFavoriteTitles[i]);
			gFavoriteFolderIds[w] = gFavoriteFolderIds[i];
		}
		if (!FolderIdKnown(gFavoriteFolderIds[w]))
			gFavoriteFolderIds[w] = kUnfiledFavoriteFolderId;
		if (!gFavoriteTitles[w][0])
			SitesRuntimeDetail::FavoriteTitleFromUrl(gFavoriteTitles[w], kMaxFavoriteTitle,
				gFavoriteUrls[w]);
		++w;
	}
	for (int i = w; i < gFavoriteCount; ++i)
	{
		gFavoriteUrls[i][0] = 0;
		gFavoriteTitles[i][0] = 0;
		gFavoriteFolderIds[i] = kUnfiledFavoriteFolderId;
	}
	if (w != gFavoriteCount)
		++gFavoriteGeneration;
	gFavoriteCount = w;
}

bool Sites::MoveFavorite(int fromSlot, int toSlot)
{
	if (fromSlot < 0 || toSlot < 0 || fromSlot >= gFavoriteCount || toSlot >= gFavoriteCount)
		return false;
	if (fromSlot == toSlot)
		return false;

	char tmpUrl[kMaxFavoriteUrl];
	char tmpTitle[kMaxFavoriteTitle];
	const int tmpFolder = gFavoriteFolderIds[fromSlot];
	std::snprintf(tmpUrl, sizeof(tmpUrl), "%s", gFavoriteUrls[fromSlot]);
	std::snprintf(tmpTitle, sizeof(tmpTitle), "%s", gFavoriteTitles[fromSlot]);
	if (fromSlot < toSlot)
	{
		for (int i = fromSlot; i < toSlot; ++i)
		{
			std::snprintf(gFavoriteUrls[i], kMaxFavoriteUrl, "%s", gFavoriteUrls[i + 1]);
			std::snprintf(gFavoriteTitles[i], kMaxFavoriteTitle, "%s", gFavoriteTitles[i + 1]);
			gFavoriteFolderIds[i] = gFavoriteFolderIds[i + 1];
		}
	}
	else
	{
		for (int i = fromSlot; i > toSlot; --i)
		{
			std::snprintf(gFavoriteUrls[i], kMaxFavoriteUrl, "%s", gFavoriteUrls[i - 1]);
			std::snprintf(gFavoriteTitles[i], kMaxFavoriteTitle, "%s", gFavoriteTitles[i - 1]);
			gFavoriteFolderIds[i] = gFavoriteFolderIds[i - 1];
		}
	}
	std::snprintf(gFavoriteUrls[toSlot], kMaxFavoriteUrl, "%s", tmpUrl);
	std::snprintf(gFavoriteTitles[toSlot], kMaxFavoriteTitle, "%s", tmpTitle);
	gFavoriteFolderIds[toSlot] = tmpFolder;
	SitesRuntimeDetail::MarkFavoritesChanged(true);
	return true;
}

bool Sites::MoveFavoriteInFolder(int folderId, int fromSlot, int toSlot)
{
	if (fromSlot == toSlot || fromSlot < 0 || toSlot < 0)
		return false;
	int fromGlobal = -1;
	int toGlobal = -1;
	int seen = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (gFavoriteFolderIds[i] != folderId)
			continue;
		if (seen == fromSlot)
			fromGlobal = i;
		if (seen == toSlot)
			toGlobal = i;
		++seen;
	}
	if (fromGlobal < 0 || toGlobal < 0)
		return false;
	return MoveFavorite(fromGlobal, toGlobal);
}
