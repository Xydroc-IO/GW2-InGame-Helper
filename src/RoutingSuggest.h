#pragma once

#include <cstddef>
#include <string>
#include <vector>

/* Nearest public-map waypoints to a trail start + orange search guide.
   Clipboard only — no auto-teleport. Unlock filtering can be added later. */
namespace RoutingSuggest
{
	struct Candidate
	{
		std::string name;
		std::string chatLink;
		float continentX = 0.f;
		float continentY = 0.f;
		float dist = 0.f; /* continent units */
		bool hasCoord = false;
	};

	struct Result
	{
		bool ok = false;
		std::string status;
		float trailX = 0.f;
		float trailY = 0.f;
		char trailLabel[96]{};
		std::vector<Candidate> nearest;
	};

	/* Uses current-map trails (enabled first, else any loaded) + waypoint index. */
	Result SuggestNearTrailStart(size_t maxN = 3);

	/* Sets Tekkit orange guide toward the trail start. */
	void ApplyOrangeGuide(const Result& r);

	void ClearGuide();

	bool CopyChatLink(const char* chatLink);

	/* Last successful / attempted suggest — for Pathing pad UI. */
	const Result& Last();
	void SetLast(Result r);
}
