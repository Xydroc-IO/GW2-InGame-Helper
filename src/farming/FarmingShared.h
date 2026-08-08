#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace FarmingDetail
{
	enum class RunTag : int
	{
		All = -1,
		Meta = 0,
		Gather,
		Currency,
		Fishing,
		Home,
		Festival,
		Custom,
		Count
	};

	struct RunStep
	{
		char text[120]{};
		bool done = false;
		bool hasCoord = false;
		float continentX = 0.f;
		float continentY = 0.f;
	};

	struct Run
	{
		int id = 0;
		int mapId = 0; /* preferred map for live nodes; 0 = any / current */
		RunTag tag = RunTag::Meta;
		bool custom = false;
		bool favorite = false;
		char name[96]{};
		char blurb[160]{};
		char pathingHint[96]{};
		/* Optional EventsData timetable key for static UTC windows (catalog only). */
		char eventsKey[64]{};
		std::vector<RunStep> steps;
	};

	struct FishEntry
	{
		char name[64]{};
		char map[64]{};
		int count = 0;
	};

	struct LiveNode
	{
		char label[96]{};
		float continentX = 0.f;
		float continentY = 0.f;
		float distSq = 0.f;
	};

	extern bool gFocus;
	extern bool gPlaceOnce;
	extern int  gDeferHeavy;
	extern int gTab; /* 0 Runs | 1 Fishing */
	extern int gSelectedRun;
	extern int gFocusStep; /* step index for GPS next highlight; -1 = none */
	extern bool gAutoArrive; /* check next step when near guided Pathing marker */
	extern float gArriveRadius;
	extern RunTag gFilterTag;
	extern bool gFavoritesOnly;
	extern char gStatus[160];
	extern char gFilter[64];
	extern char gFishName[64];
	extern char gFishMap[64];
	extern char gNewRunName[96];
	extern char gNewStepText[120];
	extern std::vector<Run> gRuns;
	extern std::vector<FishEntry> gFishLog;

	const char* TagLabel(RunTag t);
	void EnsureCatalog();
	void ToggleStep(size_t run, size_t step);
	void ResetRun(size_t run);
	void ToggleFavorite(size_t run);
	bool AddCustomRun(const char* name, RunTag tag, int mapId);
	bool AddCustomStep(size_t run, const char* text);
	bool DeleteCustomRun(size_t run);
	void AddFish(const char* name, const char* map);
	void ClearFish();
	int FishTotalCount();
	bool FillFishMapFromMumble();
	bool StartRunPathing(size_t run);
	void Load();
	void Save();

	bool RunMatchesFilter(const Run& r);
	void RunProgress(const Run& r, int& done, int& total);
	int NextUndoneStep(size_t run); /* -1 if none */

	void RefreshLiveNodes(size_t run);
	const std::vector<LiveNode>& LiveNodes();
	bool GuideLiveNode(size_t idx);
	bool GuideNearestLiveNode();
	bool GuideStep(size_t run, size_t step);
	bool GuideNextStep(size_t run);
	void TickAutoArrive(); /* proximity to armed pack-marker GPS target */

	/* Static catalog timer linked to Events timetable (schedule facts only). */
	bool RunScheduleHint(const Run& r, char* out, size_t outLen);
}
