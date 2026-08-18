#include "Sites.h"
#include "SitesInternal.h"

#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <string>

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
	std::string StripUrlNoise(const char* url)
	{
		if (!url || !url[0])
			return {};
		std::string u = url;
		const size_t hash = u.find('#');
		if (hash != std::string::npos)
			u.resize(hash);
		while (!u.empty() && u.back() == '/')
			u.pop_back();
		return u;
	}

	int FindFavoriteSlot(const char* key)
	{
		if (!key || !key[0])
			return -1;
		for (int i = 0; i < gFavoriteCount; ++i)
		{
			if (SitesRuntimeDetail::FavoriteUrlsMatch(gFavoriteUrls[i], key))
				return i;
		}
		const int si = Sites::IndexOfId(key);
		if (si < 0)
			return -1;
		size_t n = 0;
		const SiteDef* sites = Sites::All(&n);
		if (!sites || si >= static_cast<int>(n) || !sites[si].homeUrl)
			return -1;
		for (int i = 0; i < gFavoriteCount; ++i)
		{
			if (SitesRuntimeDetail::FavoriteUrlsMatch(gFavoriteUrls[i], sites[si].homeUrl))
				return i;
		}
		return -1;
	}

	void FillTitle(int slot, const char* title, const char* url)
	{
		if (title && title[0])
			std::snprintf(gFavoriteTitles[slot], kMaxFavoriteTitle, "%s", title);
		else
			SitesRuntimeDetail::FavoriteTitleFromUrl(gFavoriteTitles[slot], kMaxFavoriteTitle, url);
	}

	void MarkChanged(bool saveNow)
	{
		SitesRuntimeDetail::MarkFavoritesChanged(saveNow);
	}

	bool RemoveSlot(int slot)
	{
		if (slot < 0 || slot >= gFavoriteCount)
			return false;
		for (int j = slot; j < gFavoriteCount - 1; ++j)
		{
			std::snprintf(gFavoriteUrls[j], kMaxFavoriteUrl, "%s", gFavoriteUrls[j + 1]);
			std::snprintf(gFavoriteTitles[j], kMaxFavoriteTitle, "%s", gFavoriteTitles[j + 1]);
			gFavoriteFolderIds[j] = gFavoriteFolderIds[j + 1];
		}
		gFavoriteUrls[gFavoriteCount - 1][0] = 0;
		gFavoriteTitles[gFavoriteCount - 1][0] = 0;
		gFavoriteFolderIds[gFavoriteCount - 1] = kUnfiledFavoriteFolderId;
		--gFavoriteCount;
		return true;
	}
}

bool SitesRuntimeDetail::FavoriteUrlsMatch(const char* a, const char* b)
{
	if (!a || !b || !a[0] || !b[0])
		return false;
	if (std::strcmp(a, b) == 0)
		return true;
	return StripUrlNoise(a) == StripUrlNoise(b);
}

void SitesRuntimeDetail::FavoriteTitleFromUrl(char* dst, size_t dstLen, const char* url)
{
	if (!dst || dstLen == 0)
		return;
	dst[0] = 0;
	if (!url || !url[0])
		return;
	const char* p = std::strstr(url, "://");
	p = p ? p + 3 : url;
	if (std::strncmp(p, "www.", 4) == 0)
		p += 4;
	const char* slash = std::strchr(p, '/');
	const char* q = std::strchr(p, '?');
	const char* end = p + std::strlen(p);
	if (slash && slash < end)
		end = slash;
	if (q && q < end)
		end = q;
	if (end <= p)
	{
		std::snprintf(dst, dstLen, "%s", url);
		return;
	}
	const size_t n = static_cast<size_t>(end - p);
	if (n >= dstLen)
	{
		std::memcpy(dst, p, dstLen - 1);
		dst[dstLen - 1] = 0;
		return;
	}
	std::memcpy(dst, p, n);
	dst[n] = 0;
}

void SitesRuntimeDetail::MigrateFavoriteSiteIds()
{
	size_t n = 0;
	const SiteDef* sites = Sites::All(&n);
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (std::strstr(gFavoriteUrls[i], "://"))
			continue;
		const int si = Sites::IndexOfId(gFavoriteUrls[i]);
		if (si < 0 || !sites || si >= static_cast<int>(n) || !sites[si].homeUrl)
			continue;
		if (!gFavoriteTitles[i][0] && sites[si].label)
			std::snprintf(gFavoriteTitles[i], kMaxFavoriteTitle, "%s", sites[si].label);
		std::snprintf(gFavoriteUrls[i], kMaxFavoriteUrl, "%s", sites[si].homeUrl);
	}
}

void SitesRuntimeDetail::MarkFavoritesChanged(bool saveNow)
{
	++gFavoriteGeneration;
	Settings::SetDirty();
	Sites::SaveFavoritesStore();
	if (saveNow)
		Settings::SaveNow();
}

bool Sites::IsFavorite(const char* id)
{
	return FindFavoriteSlot(id) >= 0;
}

bool Sites::IsFavoriteUrl(const char* url)
{
	if (!url || !url[0])
		return false;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (SitesRuntimeDetail::FavoriteUrlsMatch(gFavoriteUrls[i], url))
			return true;
	}
	return false;
}

bool Sites::ToggleFavorite(const char* id)
{
	if (!id || !id[0])
		return false;
	const int si = Sites::IndexOfId(id);
	if (si < 0)
		return false;
	size_t n = 0;
	const SiteDef* sites = Sites::All(&n);
	if (!sites || si >= static_cast<int>(n) || !sites[si].homeUrl || !sites[si].homeUrl[0])
		return false;
	const char* title = sites[si].label ? sites[si].label : sites[si].title;
	return ToggleFavoriteUrl(title, sites[si].homeUrl);
}

bool Sites::ToggleFavoriteUrl(const char* title, const char* url)
{
	if (!url || !url[0] || std::strlen(url) >= kMaxFavoriteUrl)
		return false;
	const int slot = FindFavoriteSlot(url);
	if (slot >= 0)
	{
		RemoveSlot(slot);
		MarkChanged(false);
		return false;
	}
	if (gFavoriteCount >= kMaxFavorites)
		return false;
	std::snprintf(gFavoriteUrls[gFavoriteCount], kMaxFavoriteUrl, "%s", url);
	FillTitle(gFavoriteCount, title, url);
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

const char* Sites::FavoriteUrlAt(int slot)
{
	if (slot < 0 || slot >= gFavoriteCount)
		return "";
	return gFavoriteUrls[slot];
}

const char* Sites::FavoriteTitleAt(int slot)
{
	if (slot < 0 || slot >= gFavoriteCount)
		return "";
	if (gFavoriteTitles[slot][0])
		return gFavoriteTitles[slot];
	return gFavoriteUrls[slot];
}

int Sites::FavoriteSiteIndex(int favSlot)
{
	if (favSlot < 0 || favSlot >= gFavoriteCount)
		return -1;
	return BestMatchForUrl(gFavoriteUrls[favSlot]);
}

bool Sites::RenameFavorite(int slot, const char* title)
{
	if (slot < 0 || slot >= gFavoriteCount || !title || !title[0])
		return false;
	char cleaned[kMaxFavoriteTitle]{};
	std::snprintf(cleaned, sizeof(cleaned), "%s", title);
	size_t len = std::strlen(cleaned);
	while (len > 0 && (cleaned[len - 1] == ' ' || cleaned[len - 1] == '\t'))
		cleaned[--len] = 0;
	if (len == 0)
		return false;
	std::snprintf(gFavoriteTitles[slot], kMaxFavoriteTitle, "%s", cleaned);
	MarkChanged(true);
	return true;
}

bool Sites::RemoveFavoriteSlot(int slot)
{
	if (!RemoveSlot(slot))
		return false;
	MarkChanged(true);
	return true;
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

int Sites::FavoriteFolderOf(const char* siteIdOrUrl)
{
	const int slot = FindFavoriteSlot(siteIdOrUrl);
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

int Sites::FavoriteSlotInFolder(int folderId, int slotInFolder)
{
	if (slotInFolder < 0)
		return -1;
	int seen = 0;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (gFavoriteFolderIds[i] != folderId)
			continue;
		if (seen == slotInFolder)
			return i;
		++seen;
	}
	return -1;
}

int Sites::FavoriteSiteIndexInFolder(int folderId, int slotInFolder)
{
	return FavoriteSiteIndex(FavoriteSlotInFolder(folderId, slotInFolder));
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

bool Sites::MoveFavoriteFolder(int fromIndex, int toIndex)
{
	if (fromIndex < 0 || toIndex < 0 || fromIndex >= gFavoriteFolderCount ||
		toIndex >= gFavoriteFolderCount || fromIndex == toIndex)
		return false;
	const SitesRuntimeDetail::FavoriteFolder tmp = gFavoriteFolders[fromIndex];
	if (fromIndex < toIndex)
	{
		for (int i = fromIndex; i < toIndex; ++i)
			gFavoriteFolders[i] = gFavoriteFolders[i + 1];
	}
	else
	{
		for (int i = fromIndex; i > toIndex; --i)
			gFavoriteFolders[i] = gFavoriteFolders[i - 1];
	}
	gFavoriteFolders[toIndex] = tmp;
	MarkChanged(true);
	return true;
}

bool Sites::SetFavoriteFolder(const char* siteIdOrUrl, int folderId)
{
	const int slot = FindFavoriteSlot(siteIdOrUrl);
	if (slot < 0 || !FolderIdKnown(folderId))
		return false;
	if (gFavoriteFolderIds[slot] == folderId)
		return false;
	gFavoriteFolderIds[slot] = folderId;
	MarkChanged(true);
	return true;
}
