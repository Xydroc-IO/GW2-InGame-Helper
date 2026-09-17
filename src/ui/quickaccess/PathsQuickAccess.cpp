#include "PathsQuickAccess.h"

#include "Globals.h"
#include "PathsQuickAccessIcon.h"
#include "Settings.h"

namespace
{
	constexpr const char* kQaId       = "QA_GW2_HELPER_PATHS";
	/* Bump suffix when the baked PNG changes — Nexus caches by id. */
	constexpr const char* kTexId      = "TEX_GW2_HELPER_PATHS_v1";
	constexpr const char* kTexHoverId = "TEX_GW2_HELPER_PATHS_HOVER_v1";
	constexpr const char* kKbToggle   = "KB_HELPER_PATHS_TOGGLE";
	bool gAdded = false;

	void OnToggle(const char*, bool release)
	{
		if (release)
			return;
		G::ShowPathingTrails = !G::ShowPathingTrails;
		Settings::SetDirty();
		if (G::API && G::API->Log)
			G::API->Log(LOGL_INFO, ADDON_NAME,
				G::ShowPathingTrails ? "Path overlays on" : "Path overlays off");
	}
}

void PathsQuickAccess::Init()
{
	if (!G::API || gAdded)
		return;
	if (!G::API->Textures_GetOrCreateFromMemory || !G::API->QuickAccess_Add)
		return;
	if (!G::API->InputBinds_RegisterWithString)
		return;

	G::API->Textures_GetOrCreateFromMemory(
		kTexId,
		const_cast<unsigned char*>(kPathsIconPng),
		static_cast<uint64_t>(kPathsIconPng_len));
	G::API->Textures_GetOrCreateFromMemory(
		kTexHoverId,
		const_cast<unsigned char*>(kPathsIconHoverPng),
		static_cast<uint64_t>(kPathsIconHoverPng_len));

	/* Click-only by default — rebind in Nexus if desired. */
	G::API->InputBinds_RegisterWithString(kKbToggle, OnToggle, "");
	G::API->QuickAccess_Add(
		kQaId,
		kTexId,
		kTexHoverId,
		kKbToggle,
		"Path overlays — compass, world GPS, world-map trails");
	gAdded = true;

	if (G::API->Log)
		G::API->Log(LOGL_INFO, ADDON_NAME, "Paths QuickAccess icon registered");
}

void PathsQuickAccess::Shutdown()
{
	if (!G::API || !gAdded)
		return;
	if (G::API->QuickAccess_Remove)
		G::API->QuickAccess_Remove(kQaId);
	if (G::API->InputBinds_Deregister)
		G::API->InputBinds_Deregister(kKbToggle);
	gAdded = false;
}
