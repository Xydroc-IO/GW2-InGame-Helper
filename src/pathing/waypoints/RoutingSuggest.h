#pragma once

#include <cstddef>
#include <string>
#include <vector>

/* Nearest waypoints to a trail start / chat-link destination + orange guide.
   Clipboard only - no auto-teleport. Prefer walk-confirmed WPs when enabled. */
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
		bool confirmed = false; /* walked near this WP on this character */
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

	/* Uses current-map trails when categories are enabled; otherwise player position.
	   Public waypoint index - does not require pathing packs toggled on. */
	Result SuggestNearTrailStart(size_t maxN = 3);

	/* Parse clipboard for [&...] and route orange guide to that POI. */
	Result SuggestFromClipboard();

	/* Sets Tekkit orange guide toward the trail/destination point. */
	void ApplyOrangeGuide(const Result& r);

	void ClearGuide();

	bool CopyChatLink(const char* chatLink);

	/* Last successful / attempted suggest - for Pathing pad UI. */
	const Result& Last();
	void SetLast(Result r);
}
