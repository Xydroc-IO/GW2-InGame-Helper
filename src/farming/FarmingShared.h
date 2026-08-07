#pragma once

#include <cstddef>
#include <vector>

namespace FarmingDetail
{
	struct RunStep
	{
		char text[120]{};
		bool done = false;
	};

	struct Run
	{
		int id = 0;
		int mapId = 0; /* preferred map for live nodes; 0 = any / current */
		char name[96]{};
		char pathingHint[96]{};
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
	extern int gTab; /* 0 Runs | 1 Fishing */
	extern int gSelectedRun;
	extern char gStatus[160];
	extern char gFishName[64];
	extern char gFishMap[64];
	extern std::vector<Run> gRuns;
	extern std::vector<FishEntry> gFishLog;

	void EnsureSeed();
	void ToggleStep(size_t run, size_t step);
	void ResetRun(size_t run);
	void AddFish(const char* name, const char* map);
	void ClearFish();
	bool StartRunPathing(size_t run);
	void Load();
	void Save();

	void RefreshLiveNodes(size_t run);
	const std::vector<LiveNode>& LiveNodes();
	bool GuideLiveNode(size_t idx);
	bool GuideNearestLiveNode();
}
