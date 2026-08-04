#include "entryInternal.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

#include "imgui/imgui.h"

#include "CharacterProfiles.h"
#include "ConfirmedWaypoints.h"
#include "Globals.h"
#include "HelperQuickAccess.h"
#include "NotesPad.h"
#include "SessionHistoryData.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "Sites.h"
#include "UI.h"
#include "WikiBrowser.h"
#include "WorldOverlay.h"

using namespace EntryDetail;

namespace EntryDetail
{
void AddonUnload()
{
	if (!G::API)
		return;

	/* Stop drawing / input before tearing down CEF and workers. */
	G::ShowWiki = false;
	G::ShowNotes = false;
	G::ShowTpWatch = false;
	G::ShowLookup = false;
	G::ShowWallet = false;
	G::ShowVault = false;
	G::ShowAccount = false;
	G::ShowEvents = false;
	G::ShowLogManager = false;
	G::ShowPathingGuides = false;
	G::ShowCompassPad = false;
	G::ShowSettings = false;
	G::ShowDirectionCompass = false;
	G::ShowOptions = false;

	G::API->GUI_Deregister(UI_Render);
	G::API->GUI_Deregister(UI_Options);
	G::API->InputBinds_Deregister(KB_TOGGLE);
	G::API->InputBinds_Deregister(KB_ACCOUNT);
	G::API->InputBinds_Deregister(KB_TEKKIT);
	G::API->InputBinds_Deregister(KB_MARKER);
	G::API->InputBinds_Deregister(KB_EVENTS);
	G::API->InputBinds_Deregister(KB_NOTES);
	G::API->InputBinds_Deregister(KB_ITEM_LEGACY);
	G::API->WndProc_Deregister(OnWndProc);

	HelperQuickAccess::Shutdown();
	WikiBrowser::Shutdown();
	Sites::Shutdown();

	/* Persist pathing toggles before Shutdown clears runtime state. */
	PathingTrails::SerializeEnabledPaths(G::PathingEnabled, sizeof(G::PathingEnabled));
	NotesPad::Save(true);
	CharacterProfiles::CaptureCurrent();
	CharacterProfiles::Save(true);
	ConfirmedWaypoints::Save(true);
	SessionHistoryData::Save(true);
	Settings::SaveNow();

	PathingTrails::Shutdown();
	WorldOverlay::Shutdown();

	if (G::API->Log)
		G::API->Log(LOGL_INFO, ADDON_NAME, "Unloaded (Nexus disable / hot-reload).");

	G::API = nullptr;
	G::NexusLink = nullptr;
	G::Mumble = nullptr;
}

} // namespace EntryDetail
