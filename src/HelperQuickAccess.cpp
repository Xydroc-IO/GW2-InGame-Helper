#include "HelperQuickAccess.h"

#include "Globals.h"
#include "QuickAccessIcon.h"

namespace
{
	constexpr const char* kQaId       = "QA_GW2_INGAME_HELPER";
	constexpr const char* kTexId      = "TEX_GW2_HELPER_ICON";
	constexpr const char* kTexHoverId = "TEX_GW2_HELPER_ICON_HOVER";
	constexpr const char* kKbToggle   = "KB_HELPER_TOGGLE";
	bool gAdded = false;
}

void HelperQuickAccess::Init()
{
	if (!G::API || gAdded)
		return;
	if (!G::API->Textures_GetOrCreateFromMemory || !G::API->QuickAccess_Add)
		return;

	/* Official Nexus texture + QuickAccess APIs only — no GW2 memory reads. */
	G::API->Textures_GetOrCreateFromMemory(
		kTexId,
		const_cast<unsigned char*>(kHelperIconPng),
		static_cast<uint64_t>(kHelperIconPng_len));
	G::API->Textures_GetOrCreateFromMemory(
		kTexHoverId,
		const_cast<unsigned char*>(kHelperIconHoverPng),
		static_cast<uint64_t>(kHelperIconHoverPng_len));

	G::API->QuickAccess_Add(
		kQaId,
		kTexId,
		kTexHoverId,
		kKbToggle,
		"In-Game Helper (Ctrl+Shift+H)");
	gAdded = true;

	if (G::API->Log)
		G::API->Log(LOGL_INFO, ADDON_NAME, "QuickAccess icon registered");
}

void HelperQuickAccess::Shutdown()
{
	if (!G::API || !gAdded)
		return;
	if (G::API->QuickAccess_Remove)
		G::API->QuickAccess_Remove(kQaId);
	gAdded = false;
}
