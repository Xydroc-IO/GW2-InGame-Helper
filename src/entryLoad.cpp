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
#include "PanelBinds.h"
#include "SessionHistoryData.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "Sites.h"
#include "UI.h"
#include "WikiBrowser.h"

using namespace EntryDetail;

namespace EntryDetail
{
void AddonLoad(AddonAPI_t* api)
{
	G::API = api;

	ImGui::SetCurrentContext(static_cast<ImGuiContext*>(api->ImguiContext));
	ImGui::SetAllocatorFunctions(
		reinterpret_cast<void* (*)(size_t, void*)>(api->ImguiMalloc),
		reinterpret_cast<void (*)(void*, void*)>(api->ImguiFree));

	G::NexusLink = static_cast<NexusLinkData_t*>(api->DataLink_Get(DL_NEXUS_LINK));
	G::Mumble = static_cast<MumbleLinkedMem*>(api->DataLink_Get(DL_MUMBLE_LINK));

	Settings::Load();
	NotesPad::Load();
	CharacterProfiles::Load();
	ConfirmedWaypoints::Load();
	SessionHistoryData::Load();
	PathingTrails::Init();
	/* Restore category toggles after Init (Init no longer wipes them, but first
	   load applies settings here so order stays Load -> Init -> apply). */
	if (G::PathingEnabled[0])
		PathingTrails::ParseEnabledPaths(G::PathingEnabled);
	else
	{
		/* First run / empty settings - enable Lady Elyssa so Windows users see
		   trails without hunting Categories (was a common "trails broken" report). */
		PathingTrails::EnableAllLadyCategories();
		PathingTrails::SerializeEnabledPaths(G::PathingEnabled, sizeof(G::PathingEnabled));
		Settings::SetDirty();
	}
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
	G::ShowTrailTools = false;
	G::ShowCompassPad = false;
	G::ShowSettings = false;
	gPollToggleHeld = false;
	gSwallowHotkeyKeys = false;
	Sites::Init();
	/* Settings::Load may have parsed legacy FavoriteIds=; json wins / migrates. */
	Sites::LoadFavoritesStore();
	/* Settings::Load parses FavoriteIds before the catalog exists; prune now. */
	{
		const int before = Sites::FavoriteCount();
		Sites::PruneFavorites();
		if (Sites::FavoriteCount() != before)
		{
			Sites::SaveFavoritesStore();
			Settings::SetDirty();
		}
	}
	WikiBrowser::Init();

	api->GUI_Register(RT_Render, UI_Render);
	api->GUI_Register(RT_OptionsRender, UI_Options);

	/* Helper open stays Nexus (QuickAccess). Panel pads are addon-owned binds. */
	PanelBinds::DeregisterLegacyNexusBinds();
	api->InputBinds_RegisterWithString(KB_TOGGLE, OnToggle, "CTRL+SHIFT+H");
	api->WndProc_Register(OnWndProc);
	HelperQuickAccess::Init();

	api->Log(LOGL_INFO, ADDON_NAME,
		"Loaded - Ctrl+Shift+H/K helper; panel binds in Settings -> Keybinds.");
}

} // namespace EntryDetail
