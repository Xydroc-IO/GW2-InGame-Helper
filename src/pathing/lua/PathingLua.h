#pragma once

#include "PathingTrails.h"

#include <string>
#include <vector>

/* Subset Blish Pathing Lua runtime (script-* attrs). Default OFF. */
namespace PathingLua
{
	void Init();
	void Shutdown();
	bool Enabled();
	void SetEnabled(bool on);
	/* Hot-apply Features → Enable Lua without restarting the game. */
	void ApplyRuntimeToggle(bool on);

	void ClearScripts();
	/* Tear down tick/menus/dynamics; keep stored .lua sources for re-enable. */
	void DisableRuntime();
	/* Re-run pack.lua entry points from stored sources (Lua must be enabled). */
	void EnableRuntime();
	void AddScriptSource(const std::string& name, const std::string& source);
	/* After a pack zip finishes storing .lua files, run pack.lua entry points. */
	void RunPendingPackEntries();

	void Tick(std::vector<PathingTrails::Marker>& markers);
	void OnMarkersLoaded(std::vector<PathingTrails::Marker>& markers);
	/* Append Pack:CreateMarker dynamics into out (current map). */
	void AppendDynamicMarkers(uint32_t mapId, std::vector<PathingTrails::Marker>& out);
	/* ImGui entries from Menu.Add (Pathing -> Features). */
	void DrawScriptMenus();
}
