#include "CompletionShared.h"

#include "AddonPaths.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include <windows.h>

namespace CompletionDetail
{
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
