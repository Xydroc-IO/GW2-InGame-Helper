#pragma once

/* Modular Pathing "Features" tab - curated pack toggles as wrapping button chips
   (no category tree). Calls into PathingTrails; does not own pack data. */
namespace PathingFeatures
{
	/* Returns true if category enable state changed (caller should sync settings). */
	bool RenderContents();
}
