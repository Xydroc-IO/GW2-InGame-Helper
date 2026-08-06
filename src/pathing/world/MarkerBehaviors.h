#pragma once

#include "PathingTrails.h"

#include <cstdint>

/* Blish Pathing / TacO marker behaviors for curated packs (Hero, Lady, ...):
   GUID persistence, Behavior 0-7 / 101, AutoTrigger, hide/show categories,
   tips / info / copy. Lua script-* attrs are not executed. */

namespace MarkerBehaviors
{
	void Init();
	void Shutdown();
	void Load();
	void Save(bool force = false);

	/* Clear all activated GUID states (like deleting Blish timers.txt). */
	void ResetAllStates();

	/* Request interact on the nearest in-range marker (Nexus keybind). */
	void RequestInteract();

	/* Per-frame: auto-triggers + interact + tip proximity. */
	void Tick(
		uint32_t mapId, uint32_t shardId, uint32_t instanceId,
		float avatarX, float avatarY, float avatarZ,
		const char* characterName,
		const std::vector<PathingTrails::Marker>& markers);

	/* True if the marker should be drawn (behavior state allows it). */
	bool ShouldDisplay(const PathingTrails::Marker& m,
		uint32_t mapId, uint32_t shardId, uint32_t instanceId,
		const char* characterName);

	struct NearbyUi
	{
		bool valid = false;
		bool canInteract = false;
		float distance = 0.f;
		char tipName[96]{};
		char tipDescription[384]{};
		char infoPreview[160]{};
		char status[96]{};
	};
	NearbyUi GetNearbyUi();

	/* ImGui tip / info / interact chrome near the player. */
	void DrawOverlay();
}
