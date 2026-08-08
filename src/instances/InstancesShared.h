#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace InstancesDetail
{
	enum class Kind : int { Story = 0, Fractal, Raid, Strike, Count };
	inline const char* KindName(Kind k)
	{
		switch (k) {
		case Kind::Story: return "Story";
		case Kind::Fractal: return "Fractals";
		case Kind::Raid: return "Raids";
		case Kind::Strike: return "Strikes";
		default: return "?";
		}
	}

	struct Step
	{
		char text[160]{};
		char apiId[48]{}; /* /v2/account/raids encounter id; empty = none */
		int  achId = 0;   /* lifetime /v2/account/achievements overlay */
		std::vector<int> storyIds; /* /v2/stories ids — all must be quest-complete */
		bool done = false;
	};
	struct Entry
	{
		int id = 0;
		Kind kind = Kind::Raid;
		char name[96]{};
		char blurb[128]{};
		std::vector<Step> steps;
		bool cleared = false; /* weekly / local / overlay */
		int  achId = 0; /* optional entry-level achievement overlay */
	};

	extern bool gFocus;
	extern bool gPlaceOnce;
	extern Kind gKind;
	extern int gSelected;
	extern char gStatus[160];
	extern int gFractalLevel; /* from /v2/account; 0 = unknown */
	extern std::vector<std::string> gDailyFractals; /* today's fractal daily names */

	void EnsureCatalog();
	size_t Count();
	Entry* At(size_t i);
	void ToggleStep(size_t entry, size_t step);
	void ToggleCleared(size_t entry);
	bool EntryHasApiSteps(const Entry& e);
	bool StepSynced(const Step& s); /* raid / ach / story — lock when key present */
	bool EntrySynced(const Entry& e);
	void ClearKind(Kind k); /* reset clears + steps for a category */
	void ResetEntry(size_t entry);
	void LoadProgress();
	void SaveProgress();
	int CountCleared(Kind k);
	int CountEntries(Kind k);
	int CountStepsDone(size_t entry);

	/* Apply /v2/account/raids encounter ids onto raid steps (SoT for weekly clears). */
	void ApplyRaidEncounterIds(const std::vector<std::string>& ids);
	void ApplyAchievementIds(const std::vector<int>& doneIds);
	void ApplyStoryCompletions(const std::vector<int>& completeStoryIds);

	void StartRaidSync(bool force); /* force: + CM ach; soft: raids/dailies/FR only */
	void TickRaidSync();
	bool RaidSyncBusy();
}
