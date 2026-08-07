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
		char apiId[48]{}; /* /v2/account/raids encounter id; empty = local-only */
		bool done = false;
	};
	struct Entry
	{
		int id = 0;
		Kind kind = Kind::Raid;
		char name[96]{};
		char blurb[128]{};
		std::vector<Step> steps;
		bool cleared = false; /* weekly / local */
	};

	extern bool gFocus;
	extern bool gPlaceOnce;
	extern Kind gKind;
	extern int gSelected;
	extern char gStatus[160];

	void EnsureCatalog();
	size_t Count();
	Entry* At(size_t i);
	void ToggleStep(size_t entry, size_t step);
	void ToggleCleared(size_t entry);
	void ClearKind(Kind k); /* reset clears + steps for a category */
	void ResetEntry(size_t entry);
	void LoadProgress();
	void SaveProgress();
	int CountCleared(Kind k);
	int CountEntries(Kind k);
	int CountStepsDone(size_t entry);

	/* Apply /v2/account/raids encounter ids onto raid steps (SoT for weekly clears). */
	void ApplyRaidEncounterIds(const std::vector<std::string>& ids);
	void StartRaidSync(bool force);
	void TickRaidSync(); /* call from pad render — apply pending worker result */
	bool RaidSyncBusy();
}
