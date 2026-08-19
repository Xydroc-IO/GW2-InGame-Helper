#include <windows.h>
#include <cstdio>
#include <cstring>

#include "imgui/imgui.h"

#include "AccountPad.h"
#include "CharacterProfiles.h"
#include "ConfirmedWaypoints.h"
#include "AddonVersion.h"
#include "Globals.h"
#include "HelperQuickAccess.h"
#include "LookupPad.h"
#include "NotesPad.h"
#include "SessionHistoryData.h"
#include "TpWatchPad.h"
#include "EventsPad.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "VaultPad.h"
#include "WalletPad.h"
#include "WorldOverlay.h"
#include "Settings.h"
#include "Sites.h"
#include "UI.h"
#include "entryInternal.h"
#include "WikiBrowser.h"

namespace G
{
	AddonDefinition_t AddonDef{};
	AddonAPI_t*       API       = nullptr;
	NexusLinkData_t*  NexusLink = nullptr;
	MumbleLinkedMem*  Mumble    = nullptr;
	HMODULE           Self      = nullptr;

	bool  ShowWiki     = false;
	bool  ShowOptions  = true;
	bool  ShowNotes    = false;
	bool  ShowTpWatch  = false;
	bool  ShowLookup   = false;
	bool  ShowWallet   = false;
	bool  ShowVault    = false;
	bool  ShowAccount  = false;
	bool  ShowEvents   = false;
	bool  ShowLogManager = false;
	bool  ShowEconomy = false;
	bool  ShowCrafting = false;
	bool  ShowInstances = false;
	bool  ShowAchievements = false;
	bool  ShowPathingGuides = false;
	bool  ShowPathingTrails = true;
	bool  EnablePathingLua = false; /* opt-in Blish script-* subset */
	bool  LadyBarefoot = true;  /* Lady map-completion foot routes */
	bool  LadyWpOnly = false;   /* Lady Core WP Only routes */
	bool  LadyWithMounts = false; /* off by default so Barefoot works out of the box */
	bool  LadyHearts = false; /* heartpath trails - own Features toggle */
	bool  LadyHeroPointTrain = false; /* legs.hp.* train - own Features toggle */
	bool  ShowCompassOverlay = true;
	bool  ShowWorldTrails = true;
	bool  MapAssistEnabled = false; /* opt-in — Settings / Pathing */
	bool  MapAssistClickWaypoint = false; /* never auto-confirm teleport */
	bool  ShowDirectionCompass = false;
	bool  ShowCompassPad = false;
	bool  ShowWatch = false;
	bool  ShowWatchMirror = false;
	bool  ShowSettings = false;
	float DirectionLetterScale = 1.f;
	float DirectionWorldRadiusScale = 1.f;
	bool  HideWhenMapOpen = true;
	bool  HideOutOfGameplay = true;
	float WorldTrailMaxDist = 120.f;
	float WorldTrailWidth = 1.f;
	float WorldTrailPlayerClear = 1.f; /* 0 = full path; 1 = default clear bubble */
	float WorldMarkerPlayerClear = 1.f; /* 0 = no marker hole; 1 = ~2-5.5m soft-clear */
	float WorldMarkerScale = 2.f; /* world GPS icons */
	float CompassMarkerScale = 1.f; /* stock compass / minimap icons */
	float Opacity      = 0.97f;
	float FontScale    = 1.25f;
	bool  FontScaleAuto = false; /* opt-in only - default stays 1.25 */
	char  ThemeId[64]  = "";
	/* Default trims a typical browser tab+toolbar strip; tune in Watch pad. */
	float WatchCropTop = 0.14f;
	float WatchCropBottom = 0.f;
	float WatchCropLeft = 0.f;
	float WatchCropRight = 0.f;
	float WindowWidth  = 1100.f;
	float WindowHeight = 760.f;
	float WindowPosX   = 60.f;
	float WindowPosY   = 60.f;
	bool  HasSavedPos  = false;
	bool  HasSavedSize = false;
	bool  KeepHelperWarm = false;
	float SideRailW = 0.f;
	char  LastQuery[128] = "";
	char  ActiveSiteId[64] = "browse";
	char  DefaultSiteId[64] = "browse";
	char  Gw2ApiKey[128] = "";
	char  TpWatchIds[1024] = "";
	char  TpWatchAlerts[2048] = "";
	char  TpWatchBuyAlerts[2048] = "";
	char  EventTrackIds[4096] = "";
	bool  EventAlerts = true;
	bool  EventAlertsTrackedOnly = false;
	bool  EventAlertsThisMap = false;
	char  PathingEnabled[8192] = "";
	char  LogFolder[512] = "";
	char  EliteInsightsPath[512] = "";
	char  DpsReportToken[128] = "";
	float LogManagerListFrac = 0.55f;
	float LogManagerWinW = 1760.f;
	float LogManagerWinH = 900.f;
	float LogManagerWinX = -1.f;
	float LogManagerWinY = -1.f;
	bool  LogManagerGroupByEncounter = true;
	bool  LogManagerAutoParse = true;
	PadGeom PadAccount{};
	PadGeom PadPathing{};
	PadGeom PadEvents{};
	PadGeom PadNotes{};
	PadGeom PadCompass{};
	PadGeom PadWatch{};
	PadGeom PadWatchMirror{};
	PadGeom PadSettings{};
	PadGeom PadTp{};
	PadGeom PadLookup{};
	PadGeom PadWallet{};
	PadGeom PadVault{};
	PadGeom PadEconomy{};
	PadGeom PadCrafting{};
	PadGeom PadInstances{};
	PadGeom PadAchievements{};
	PadGeom PadEventAlert{};
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
		G::Self = hModule;
	return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
	G::AddonDef.Signature        = ADDON_SIG;
	G::AddonDef.APIVersion       = NEXUS_API_VERSION;
	G::AddonDef.Name             = ADDON_NAME;
	G::AddonDef.Version.Major    = ADDON_VERSION_MAJOR;
	G::AddonDef.Version.Minor    = ADDON_VERSION_MINOR;
	G::AddonDef.Version.Build    = ADDON_VERSION_BUILD;
	G::AddonDef.Version.Revision = ADDON_VERSION_REVISION;
	G::AddonDef.Author           = "xydroc";
	G::AddonDef.Description      =
		"In-game browser for Guild Wars 2 - Wiki, Snow Crows, MetaBattle, Guildjen, and more.";
	G::AddonDef.Load             = EntryDetail::AddonLoad;
	G::AddonDef.Unload           = EntryDetail::AddonUnload;
	/* Hot-unload on Nexus Disable - WikiBrowser::Shutdown stops the CEF helper first. */
	G::AddonDef.Flags            = AF_None;
	G::AddonDef.Provider         = UP_GitHub;
	G::AddonDef.UpdateLink       = "https://github.com/Xydroc-IO/GW2-InGame-Helper";
	return &G::AddonDef;
}
