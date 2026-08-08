#pragma once

/* Opt-in world-map steering for Pathing Guides.
   Uses OS cursor input only while GW2's fullscreen map is open — the game does
   not honor injected WndProc mouse for map drag. Never confirms the teleport
   dialog; the player must accept (or cancel) in-game. */
namespace MapAssist
{
	bool Enabled();
	bool ClickWaypointEnabled();

	/* Steer the open map so MapCenter approaches continent (x,y). */
	void RequestPanTo(float continentX, float continentY);

	/* Steer, then optionally tap the projected WP (if ClickWaypoint enabled). */
	void RequestTravelAssist(float continentX, float continentY);

	/* Open map via Nexus GameBinds if needed, then steer. */
	void OpenMapAndPanTo(float continentX, float continentY);

	void Tick();
	bool Busy();
	const char* Status();
	void Cancel();
}
