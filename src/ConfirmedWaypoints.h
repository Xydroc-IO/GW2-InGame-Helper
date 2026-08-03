#pragma once

#include <cstddef>

/* Per-character waypoints confirmed by walking near them (Mumble continent
   position vs official API coords). Independent of third-party addons —
   our own JSON under the addon data dir. Display / routing only. */
namespace ConfirmedWaypoints
{
	void Load();
	void Save(bool force = false);

	/* Call each frame after MumbleIdentity::Tick (throttled internally). */
	void Tick();

	bool IsConfirmed(int waypointId);
	size_t CountForActive();
	size_t CountOnMap(int mapId);

	/* Prefer confirmed waypoints when ranking route suggestions. */
	bool PreferConfirmed();
	void SetPreferConfirmed(bool on);
}
