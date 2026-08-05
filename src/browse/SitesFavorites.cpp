#include "Sites.h"
#include "SitesInternal.h"

#include "Settings.h"

#include <cstdio>
#include <cstring>

using SitesRuntimeDetail::gFavoriteCount;
using SitesRuntimeDetail::gFavoriteGeneration;
using SitesRuntimeDetail::gFavoriteIds;
using SitesRuntimeDetail::kMaxFavorites;

bool Sites::IsFavorite(const char* id)
{
	if (!id || !id[0])
		return false;
	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (std::strcmp(gFavoriteIds[i], id) == 0)
			return true;
	}
	return false;
}

bool Sites::ToggleFavorite(const char* id)
{
	if (!id || !id[0] || IndexOfId(id) < 0)
		return false;

	for (int i = 0; i < gFavoriteCount; ++i)
	{
		if (std::strcmp(gFavoriteIds[i], id) == 0)
		{
			for (int j = i; j < gFavoriteCount - 1; ++j)
				std::snprintf(gFavoriteIds[j], sizeof(gFavoriteIds[j]), "%s", gFavoriteIds[j + 1]);
			gFavoriteIds[gFavoriteCount - 1][0] = 0;
			--gFavoriteCount;
			++gFavoriteGeneration;
			Settings::SaveNow();
			return false;
		}
	}

	if (gFavoriteCount >= kMaxFavorites)
		return false;
	std::snprintf(gFavoriteIds[gFavoriteCount], sizeof(gFavoriteIds[gFavoriteCount]), "%s", id);
	++gFavoriteCount;
	++gFavoriteGeneration;
	Settings::SaveNow();
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

void Sites::ParseFavorites(const char* csv)
{
	gFavoriteCount = 0;
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
		++gFavoriteCount;
	}
	/* Do not prune here — Settings::Load runs before Sites::Init(), and pruning
	   against an empty catalog would wipe every saved favorite on startup. */
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
		/* Drop duplicates while pruning. */
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
			std::snprintf(gFavoriteIds[w], sizeof(gFavoriteIds[w]), "%s", gFavoriteIds[i]);
		++w;
	}
	for (int i = w; i < gFavoriteCount; ++i)
		gFavoriteIds[i][0] = 0;
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
	std::snprintf(tmp, sizeof(tmp), "%s", gFavoriteIds[fromSlot]);
	if (fromSlot < toSlot)
	{
		for (int i = fromSlot; i < toSlot; ++i)
			std::snprintf(gFavoriteIds[i], sizeof(gFavoriteIds[i]), "%s", gFavoriteIds[i + 1]);
	}
	else
	{
		for (int i = fromSlot; i > toSlot; --i)
			std::snprintf(gFavoriteIds[i], sizeof(gFavoriteIds[i]), "%s", gFavoriteIds[i - 1]);
	}
	std::snprintf(gFavoriteIds[toSlot], sizeof(gFavoriteIds[toSlot]), "%s", tmp);
	++gFavoriteGeneration;
	Settings::SaveNow();
	return true;
}
