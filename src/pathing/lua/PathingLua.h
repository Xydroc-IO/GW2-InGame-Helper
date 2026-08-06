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

	void ClearScripts();
	void AddScriptSource(const std::string& name, const std::string& source);
	/* After a pack zip finishes storing .lua files, run pack.lua entry points. */
	void RunPendingPackEntries();

	void Tick(std::vector<PathingTrails::Marker>& markers);
	void OnMarkersLoaded(std::vector<PathingTrails::Marker>& markers);
	/* Append Pack:CreateMarker dynamics into out (current map). */
	void AppendDynamicMarkers(uint32_t mapId, std::vector<PathingTrails::Marker>& out);
	/* ImGui entries from Menu.Add (Pathing → Features). */
	void DrawScriptMenus();
}
