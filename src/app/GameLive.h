#pragma once

/* MumbleLink freshness — MapId/UIState go stale on load screens while UITick freezes. */
namespace GameLive
{
	/* True when MumbleLink UITick advanced within the last ~100ms. */
	bool IsLive();

	/* Map open / combat / focus helpers (null-safe). */
	bool IsMapOpen();
	bool GameHasFocus();
	bool TextboxHasFocus();
	bool IsInCombat();
}
