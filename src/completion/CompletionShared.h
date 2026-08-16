#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace CompletionDetail
{
	extern bool gAchFocus;
	extern bool gAchPlaceOnce;
	extern char gStatus[192];
	extern char gApSearch[64];
	extern int gApFilter; /* 0 all | 1 done | 2 open */
	extern int gApSelCatId;
	extern int gApSelAchId;

	constexpr int kMaxAchPins = 5;
	void LoadAchPins();
	const std::vector<int>& AchPins();
	bool IsAchPinned(int achievementId);
	bool ToggleAchPin(int achievementId); /* false if already at kMaxAchPins */
	void FocusAchPin(int achievementId);

	void DrawAchievementsTab();
	void DrawAchievementDetail(int id);

	struct ApProgress
	{
		uint32_t achievementId = 0;
		bool known = false;
		bool done = false;
		int current = 0;
		int max = 0;
		std::vector<int> bits;
	};
	void BeginApOverlayRefresh();
	void ApplyApOverlayResult();
	bool ApOverlayBusy();
	bool LookupApProgress(uint32_t achievementId, ApProgress& out);
	size_t ApProgressCount();

	struct AchGroup
	{
		std::string id;
		std::string name;
		int order = 0;
		std::vector<int> categoryIds;
	};
	struct AchCategory
	{
		int id = 0;
		std::string name;
		int order = 0;
		std::vector<int> achievementIds;
	};
	enum class AchBitKind : int
	{
		Text = 0,
		Item,
		Skin,
		Mini,
		Achievement,
		Other
	};
	struct AchBit
	{
		std::string text;
		AchBitKind kind = AchBitKind::Text;
		int targetId = 0;
	};
	struct AchTier
	{
		int count = 0;
		int points = 0;
	};
	struct AchDef
	{
		int id = 0;
		std::string name;
		std::string requirement;
		std::string description;
		std::string lockedText;
		std::vector<AchBit> bits;
		std::vector<AchTier> tiers;
		int points = 0;
		bool hidden = false;
		bool repeatable = false;
	};
	void BeginAchCatalogRefresh(bool force);
	void ApplyAchCatalogResult();
	bool AchCatalogBusy();
	bool AchCatalogReady();
	const std::vector<AchGroup>& AchGroups();
	const AchCategory* FindAchCategory(int id);
	int CategoryIdContainingAchievement(int achievementId);
	void BeginAchDefsRefresh(int categoryId);
	void BeginAchDefsForIds(const std::vector<int>& ids);
	void ApplyAchDefsResult();
	bool AchDefsBusy();
	const AchDef* FindAchDef(int id);

	void BeginAchWikiThumb(int achievementId, const char* name);
	void ApplyAchWikiThumbResult();
	bool LookupAchWikiThumbUrl(int achievementId, std::string& outUrl);
}
