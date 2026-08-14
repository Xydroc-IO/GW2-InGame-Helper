#include "CompletionShared.h"

#include "AddonPaths.h"
#include "Settings.h"
#include "UiAscii.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace CompletionDetail
{
	uint32_t gKindMask =
		((1u << static_cast<int>(ObjKind::Count)) - 1u) &
		~(1u << static_cast<int>(ObjKind::Achievement));
	bool gAtlasFavOnly = false;

	namespace
	{
		std::mutex gFavMu;
		std::unordered_set<uint32_t> gFavIds;
		bool gFavLoaded = false;

		std::wstring FavPath()
		{
			return AddonPaths::ConfigDir() + L"\\completion-favorites.txt";
		}
	}

	bool KindVisible(ObjKind k)
	{
		const int i = static_cast<int>(k);
		if (i < 0 || i >= static_cast<int>(ObjKind::Count))
			return false;
		return (gKindMask & (1u << i)) != 0;
	}

	bool IsMapCompletionRouteKind(ObjKind k)
	{
		return k != ObjKind::Achievement;
	}

	void SetKindVisible(ObjKind k, bool on)
	{
		const int i = static_cast<int>(k);
		if (i < 0 || i >= static_cast<int>(ObjKind::Count))
			return;
		if (on) gKindMask |= (1u << i);
		else gKindMask &= ~(1u << i);
		if (gKindMask == 0)
		{
			gKindMask =
				((1u << static_cast<int>(ObjKind::Count)) - 1u) &
				~(1u << static_cast<int>(ObjKind::Achievement));
		}
	}

	void SetAllKindsVisible(bool on)
	{
		const uint32_t mcMask =
			((1u << static_cast<int>(ObjKind::Count)) - 1u) &
			~(1u << static_cast<int>(ObjKind::Achievement));
		gKindMask = on ? mcMask : 0u;
		if (gKindMask == 0)
			gKindMask = mcMask;
	}

	void LoadFavorites()
	{
		std::lock_guard<std::mutex> lock(gFavMu);
		if (gFavLoaded) return;
		gFavLoaded = true;
		gFavIds.clear();
		FILE* f = _wfopen(FavPath().c_str(), L"rb");
		if (!f) return;
		char line[64]{};
		while (std::fgets(line, sizeof(line), f))
		{
			uint32_t id = 0;
			if (std::sscanf(line, "%u", &id) == 1 && id != 0)
				gFavIds.insert(id);
		}
		std::fclose(f);
	}

	void SaveFavorites()
	{
		std::lock_guard<std::mutex> lock(gFavMu);
		CreateDirectoryW(AddonPaths::ConfigDir().c_str(), nullptr);
		FILE* f = _wfopen(FavPath().c_str(), L"wb");
		if (!f) return;
		for (uint32_t id : gFavIds)
			std::fprintf(f, "%u\n", id);
		std::fclose(f);
	}

	bool IsFavorite(uint32_t mapId)
	{
		LoadFavorites();
		std::lock_guard<std::mutex> lock(gFavMu);
		return gFavIds.count(mapId) != 0;
	}

	void ToggleFavorite(uint32_t mapId)
	{
		if (mapId == 0) return;
		LoadFavorites();
		{
			std::lock_guard<std::mutex> lock(gFavMu);
			if (gFavIds.count(mapId))
				gFavIds.erase(mapId);
			else
				gFavIds.insert(mapId);
		}
		SaveFavorites();
		std::snprintf(gStatus, sizeof(gStatus),
			IsFavorite(mapId) ? "Favorited map %u." : "Unfavorited map %u.", mapId);
	}

	namespace
	{
		std::vector<int> gAchPins;
		bool gPinsLoaded = false;

		std::wstring PinPath()
		{
			return AddonPaths::ConfigDir() + L"\\achievement-pins.txt";
		}

		void SaveAchPinsUnlocked()
		{
			CreateDirectoryW(AddonPaths::ConfigDir().c_str(), nullptr);
			FILE* f = _wfopen(PinPath().c_str(), L"wb");
			if (!f)
				return;
			for (int id : gAchPins)
				std::fprintf(f, "%d\n", id);
			std::fclose(f);
		}
	}

	void LoadAchPins()
	{
		if (gPinsLoaded)
			return;
		gPinsLoaded = true;
		gAchPins.clear();
		FILE* f = _wfopen(PinPath().c_str(), L"rb");
		if (!f)
			return;
		char line[64]{};
		while (std::fgets(line, sizeof(line), f))
		{
			int id = 0;
			if (std::sscanf(line, "%d", &id) == 1 && id > 0)
			{
				bool dup = false;
				for (int p : gAchPins)
				{
					if (p == id)
					{
						dup = true;
						break;
					}
				}
				if (!dup && static_cast<int>(gAchPins.size()) < kMaxAchPins)
					gAchPins.push_back(id);
			}
		}
		std::fclose(f);
	}

	const std::vector<int>& AchPins()
	{
		LoadAchPins();
		return gAchPins;
	}

	bool IsAchPinned(int achievementId)
	{
		LoadAchPins();
		for (int p : gAchPins)
		{
			if (p == achievementId)
				return true;
		}
		return false;
	}

	bool ToggleAchPin(int achievementId)
	{
		if (achievementId <= 0)
			return false;
		LoadAchPins();
		for (size_t i = 0; i < gAchPins.size(); ++i)
		{
			if (gAchPins[i] == achievementId)
			{
				gAchPins.erase(gAchPins.begin() + static_cast<std::ptrdiff_t>(i));
				SaveAchPinsUnlocked();
				return true;
			}
		}
		if (static_cast<int>(gAchPins.size()) >= kMaxAchPins)
			return false;
		gAchPins.push_back(achievementId);
		SaveAchPinsUnlocked();
		return true;
	}

	void FocusAchPin(int achievementId)
	{
		if (achievementId <= 0)
			return;
		gApSelAchId = achievementId;
		const int cid = CategoryIdContainingAchievement(achievementId);
		if (cid > 0)
		{
			gApSelCatId = cid;
			BeginAchDefsRefresh(cid);
		}
	}
}
