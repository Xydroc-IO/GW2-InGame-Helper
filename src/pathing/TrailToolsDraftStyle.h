#pragma once

#include "PathingTrails.h"
#include "TrailToolsShared.h"

#include <cstdint>
#include <string>

/* Resolve draft category Looks + upload authoring PNGs for WYSIWYG preview. */
namespace TrailToolsDraftStyle
{
	struct Resolved
	{
		std::string textureRel; /* pack-relative texture path */
		std::string iconRel;
		char        textureId[160]{};
		char        iconId[160]{};
		uint32_t    color = 0xFFFFFFFFu;
		float       trailScale = 1.f;
		float       iconSize = 1.f;
		float       alpha = 1.f;
		float       fadeNear = -1.f;
		float       fadeFar = -1.f;
	};

	void BeginFrame(); /* upload pending draft textures */
	Resolved ResolveTrail();
	Resolved ResolveMarkerType(const std::string& typePath);

	/* Build a WorldSnippet from the active draft trail (empty if <2 pts). */
	PathingTrails::WorldSnippet BuildActiveSnippet();
	/* Build a Marker-like draw helper for one draft POI. */
	PathingTrails::Marker BuildDraftMarker(const TrailToolsDetail::DraftPoi& poi);
}
