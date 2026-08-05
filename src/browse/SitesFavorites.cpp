#include "Sites.h"
#include "SitesInternal.h"

#include "AddonPaths.h"
#include "Settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <windows.h>

using SitesRuntimeDetail::gFavoriteCount;
using SitesRuntimeDetail::gFavoriteFolderIds;
using SitesRuntimeDetail::gFavoriteFolders;
using SitesRuntimeDetail::gFavoriteFolderCount;
using SitesRuntimeDetail::gFavoriteGeneration;
using SitesRuntimeDetail::gFavoriteIds;
using SitesRuntimeDetail::gFavoriteNextFolderId;
using SitesRuntimeDetail::kMaxFavoriteFolders;
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

	bool FolderIdKnown(int folderId)
	{
		if (folderId == kUnfiledFavoriteFolderId)
			return true;
		for (int i = 0; i < gFavoriteFolderCount; ++i)
		{
			if (gFavoriteFolders[i].id == folderId)
				return true;
		}
		return false;
	}

	int FindFavoriteSlot(const char* id)
	{
		if (!id || !id[0])
			return -1;
		for (int i = 0; i < gFavoriteCount; ++i)
		{
			if (std::strcmp(gFavoriteIds[i], id) == 0)
				return i;
		}
		return -1;
	}

	void MarkChanged(bool saveNow)
	{
		++gFavoriteGeneration;
		Settings::SetDirty();
		Sites::SaveFavoritesStore();
		if (saveNow)
			Settings::SaveNow();
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
			gFavoriteIds[i][0] = 0;
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
					size_t p = FindKey(obj, "id");
					std::string id;
					if (p != std::string::npos)
					{
						p = obj.find(':', p);
						if (p != std::string::npos)
						{
							++p;
							id = ParseJsonString(obj, p);
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
					if (!id.empty() && id.size() < sizeof(gFavoriteIds[0]))
					{
						if (!FolderIdKnown(folder))
							folder = kUnfiledFavoriteFolderId;
						std::snprintf(gFavoriteIds[gFavoriteCount], sizeof(gFavoriteIds[0]), "%s",
							id.c_str());
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

bool Sites::IsFavorite(const char* id)
{
	return FindFavoriteSlot(id) >= 0;
}

bool Sites::ToggleFavorite(const char* id)
{
	if (!id || !id[0] || IndexOfId(id) < 0)
		return false;

	const int slot = FindFavoriteSlot(id);
	if (slot >= 0)
	{
		for (int j = slot; j < gFavoriteCount - 1; ++j)
		{
			std::snprintf(gFavoriteIds[j], sizeof(gFavoriteIds[j]), "%s", gFavoriteIds[j + 1]);
			gFavoriteFolderIds[j] = gFavoriteFolderIds[j + 1];
		}
		gFavoriteIds[gFavoriteCount - 1][0] = 0;
		gFavoriteFolderIds[gFavoriteCount - 1] = kUnfiledFavoriteFolderId;
		--gFavoriteCount;
		MarkChanged(false);
		return false;
	}

	if (gFavoriteCount >= kMaxFavorites)
		return false;
	std::snprintf(gFavoriteIds[gFavoriteCount], sizeof(gFavoriteIds[gFavoriteCount]), "%s", id);
	gFavoriteFolderIds[gFavoriteCount] = kUnfiledFavoriteFolderId;
	++gFavoriteCount;
	MarkChanged(false);
	return true;
}

int Sites::FavoriteCount()
{
	return gFavoriteCount;
}

unsigned Sites::FavoritesGeneration()
{
	return gFavoriteGeneration;
}

int Sites::FavoriteSiteIndex(int favSlot)
{
	if (favSlot < 0 || favSlot >= gFavoriteCount)
		return -1;
	return IndexOfId(gFavoriteIds[favSlot]);
}

int Sites::FavoriteFolderCount()
{
	return gFavoriteFolderCount;
}

int Sites::FavoriteFolderIdAt(int index)
{
	if (index < 0 || index >= gFavoriteFolderCount)
		return -1;
	return gFavoriteFolders[index].id;
}

const char* Sites::FavoriteFolderName(int folderId)
{
	if (folderId == kUnfiledFavoriteFolderId)
		return "Unfiled";
	for (int i = 0; i < gFavoriteFolderCount; ++i)
	{
		if (gFavoriteFolders[i].id == folderId)
			return gFavoriteFolders[i].name;
	}
	return "Unfiled";
}

int Sites::FavoriteFolderOf(const char* siteId)
{
	const int slot = FindFavoriteSlot(siteId);
	if (slot < 0)
		return kUnfiledFavoriteFolderId;
	return gFavoriteFolderIds[slot];
}

int Sites::FavoriteCountInFolder(int folderId)
{
	int n = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (gFavoriteFolderIds[i] == folderId)
			++n;
	}
	return n;
}

int Sites::FavoriteSiteIndexInFolder(int folderId, int slotInFolder)
{
	if (slotInFolder < 0)
		return -1;
	int seen = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (gFavoriteFolderIds[i] != folderId)
			continue;
		if (seen == slotInFolder)
			return IndexOfId(gFavoriteIds[i]);
		++seen;
	}
	return -1;
}

bool Sites::CreateFavoriteFolder(const char* name)
{
	if (!name || !name[0] || gFavoriteFolderCount >= kMaxFavoriteFolders)
		return false;
	while (*name == ' ' || *name == '\t')
		++name;
	if (!name[0])
		return false;
	char cleaned[48]{};
	std::snprintf(cleaned, sizeof(cleaned), "%s", name);
	size_t len = std::strlen(cleaned);
	while (len > 0 && (cleaned[len - 1] == ' ' || cleaned[len - 1] == '\t'))
		cleaned[--len] = 0;
	if (len == 0)
		return false;
	for (int i = 0; i < gFavoriteFolderCount; ++i)
	{
		if (_stricmp(gFavoriteFolders[i].name, cleaned) == 0)
			return false;
	}
	gFavoriteFolders[gFavoriteFolderCount].id = gFavoriteNextFolderId++;
	std::snprintf(gFavoriteFolders[gFavoriteFolderCount].name,
		sizeof(gFavoriteFolders[0].name), "%s", cleaned);
	++gFavoriteFolderCount;
	MarkChanged(true);
	return true;
}

bool Sites::RenameFavoriteFolder(int folderId, const char* name)
{
	if (folderId == kUnfiledFavoriteFolderId || !name || !name[0])
		return false;
	while (*name == ' ' || *name == '\t')
		++name;
	char cleaned[48]{};
	std::snprintf(cleaned, sizeof(cleaned), "%s", name);
	size_t len = std::strlen(cleaned);
	while (len > 0 && (cleaned[len - 1] == ' ' || cleaned[len - 1] == '\t'))
		cleaned[--len] = 0;
	if (len == 0)
		return false;
	int idx = -1;
	for (int i = 0; i < gFavoriteFolderCount; ++i)
	{
		if (gFavoriteFolders[i].id == folderId)
			idx = i;
		else if (_stricmp(gFavoriteFolders[i].name, cleaned) == 0)
			return false;
	}
	if (idx < 0)
		return false;
	std::snprintf(gFavoriteFolders[idx].name, sizeof(gFavoriteFolders[0].name), "%s", cleaned);
	MarkChanged(true);
	return true;
}

bool Sites::DeleteFavoriteFolder(int folderId)
{
	if (folderId == kUnfiledFavoriteFolderId)
		return false;
	int idx = -1;
	for (int i = 0; i < gFavoriteFolderCount; ++i)
	{
		if (gFavoriteFolders[i].id == folderId)
		{
			idx = i;
			break;
		}
	}
	if (idx < 0)
		return false;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (gFavoriteFolderIds[i] == folderId)
			gFavoriteFolderIds[i] = kUnfiledFavoriteFolderId;
	}
	for (int i = idx; i < gFavoriteFolderCount - 1; ++i)
		gFavoriteFolders[i] = gFavoriteFolders[i + 1];
	gFavoriteFolders[gFavoriteFolderCount - 1].id = 0;
	gFavoriteFolders[gFavoriteFolderCount - 1].name[0] = 0;
	--gFavoriteFolderCount;
	MarkChanged(true);
	return true;
}

bool Sites::SetFavoriteFolder(const char* siteId, int folderId)
{
	const int slot = FindFavoriteSlot(siteId);
	if (slot < 0 || !FolderIdKnown(folderId))
		return false;
	if (gFavoriteFolderIds[slot] == folderId)
		return false;
	gFavoriteFolderIds[slot] = folderId;
	MarkChanged(true);
	return true;
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
		if (len == 0 || len >= sizeof(gFavoriteIds[0]))
			continue;
		std::memcpy(gFavoriteIds[gFavoriteCount], start, len);
		gFavoriteIds[gFavoriteCount][len] = 0;
		gFavoriteFolderIds[gFavoriteCount] = kUnfiledFavoriteFolderId;
		++gFavoriteCount;
	}
	/* Do not prune here — Settings::Load runs before Sites::Init(). */
}

void Sites::SerializeFavorites(char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return;
	out[0] = 0;
	size_t used = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		const char* id = gFavoriteIds[i];
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
	int w = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (IndexOfId(gFavoriteIds[i]) < 0)
			continue;
		bool dup = false;
		for (int j = 0; j < w; ++j)
		{
			if (std::strcmp(gFavoriteIds[j], gFavoriteIds[i]) == 0)
			{
				dup = true;
				break;
			}
		}
		if (dup)
			continue;
		if (w != i)
		{
			std::snprintf(gFavoriteIds[w], sizeof(gFavoriteIds[w]), "%s", gFavoriteIds[i]);
			gFavoriteFolderIds[w] = gFavoriteFolderIds[i];
		}
		if (!FolderIdKnown(gFavoriteFolderIds[w]))
			gFavoriteFolderIds[w] = kUnfiledFavoriteFolderId;
		++w;
	}
	for (int i = w; i < gFavoriteCount; ++i)
	{
		gFavoriteIds[i][0] = 0;
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

	char tmp[64];
	const int tmpFolder = gFavoriteFolderIds[fromSlot];
	std::snprintf(tmp, sizeof(tmp), "%s", gFavoriteIds[fromSlot]);
	if (fromSlot < toSlot)
	{
		for (int i = fromSlot; i < toSlot; ++i)
		{
			std::snprintf(gFavoriteIds[i], sizeof(gFavoriteIds[i]), "%s", gFavoriteIds[i + 1]);
			gFavoriteFolderIds[i] = gFavoriteFolderIds[i + 1];
		}
	}
	else
	{
		for (int i = fromSlot; i > toSlot; --i)
		{
			std::snprintf(gFavoriteIds[i], sizeof(gFavoriteIds[i]), "%s", gFavoriteIds[i - 1]);
			gFavoriteFolderIds[i] = gFavoriteFolderIds[i - 1];
		}
	}
	std::snprintf(gFavoriteIds[toSlot], sizeof(gFavoriteIds[toSlot]), "%s", tmp);
	gFavoriteFolderIds[toSlot] = tmpFolder;
	MarkChanged(true);
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

void Sites::SaveFavoritesStore()
{
	std::string out = "{\n  \"version\": 1,\n  \"folders\": [\n";
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
		out += "    {\"id\":\"";
		out += JsonEsc(gFavoriteIds[i]);
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
		return;
	}
	/* Migrate legacy FavoriteIds= from settings (already parsed into memory). */
	if (gFavoriteCount > 0)
		SaveFavoritesStore();
}
