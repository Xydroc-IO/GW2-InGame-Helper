#include "Settings.h"
#include "SettingsInternal.h"

#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "PadDock.h"
#include "Sites.h"
#include "PathingTrails.h"
#include "TrailToolsShared.h"
#include "TrailToolsBinds.h"
#include "PanelBinds.h"
#include "UI.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <windows.h>

void Settings::Save(bool force)
{
	if (!SettingsDetail::gDirty)
		return;

	/* Opening the helper used to call Save every ImGui frame. Title/URL sync
	   and float window pos could keep the dirty flag set → fopen settings.ini
	   every frame and freeze GW2. Debounce unless forced (unload only). */
	static DWORD sLastSaveMs = 0;
	const DWORD now = GetTickCount();
	if (!force && sLastSaveMs != 0 && (now - sLastSaveMs) < 2500u)
		return;

	AddonPaths::DataDir(); /* ensure folder exists */
	char path[MAX_PATH]{};
	SettingsDetail::SettingsPath(path, sizeof(path));
	FILE* f = std::fopen(path, "w");
	if (!f)
		return;

	std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
	if (!G::DefaultSiteId[0])
		std::snprintf(G::DefaultSiteId, sizeof(G::DefaultSiteId), "browse");

	std::fprintf(f, "ShowWiki=%d\n", G::ShowWiki ? 1 : 0);
	std::fprintf(f, "ShowOptions=%d\n", G::ShowOptions ? 1 : 0);
	std::fprintf(f, "ShowNotes=0\n");
	std::fprintf(f, "ShowTpWatch=0\n");
	std::fprintf(f, "ShowLookup=0\n");
	std::fprintf(f, "ShowWallet=0\n");
	std::fprintf(f, "ShowVault=0\n");
	std::fprintf(f, "ShowAccount=0\n");
	std::fprintf(f, "ShowEvents=0\n");
	std::fprintf(f, "ShowLogManager=0\n");
	std::fprintf(f, "ShowEconomy=0\n");
	std::fprintf(f, "ShowInstances=0\n");
	std::fprintf(f, "ShowCompletion=0\n");
	std::fprintf(f, "ShowFarming=0\n");
	std::fprintf(f, "ShowPathingGuides=0\n");
	std::fprintf(f, "ShowTrailTools=0\n");
	std::fprintf(f, "TrailToolsLastTrlDir=%s\n", TrailToolsDetail::gDraft.lastTrlDir);
	std::fprintf(f, "TrailToolsXmlLayout=%d\n", TrailToolsDetail::gDraft.xmlLayout != 0 ? 1 : 0);
	std::fprintf(f, "TrailToolsBinds=%s\n", TrailToolsBinds::Serialize().c_str());
	std::fprintf(f, "PanelBinds=%s\n", PanelBinds::Serialize().c_str());
	std::fprintf(f, "ShowPathingTrails=%d\n", G::ShowPathingTrails ? 1 : 0);
	std::fprintf(f, "EnablePathingLua=%d\n", G::EnablePathingLua ? 1 : 0);
	std::fprintf(f, "LadyBarefoot=%d\n", G::LadyBarefoot ? 1 : 0);
	std::fprintf(f, "LadyWpOnly=%d\n", G::LadyWpOnly ? 1 : 0);
	std::fprintf(f, "LadyWithMounts=%d\n", G::LadyWithMounts ? 1 : 0);
	std::fprintf(f, "LadyHearts=%d\n", G::LadyHearts ? 1 : 0);
	std::fprintf(f, "LadyHeroPointTrain=%d\n", G::LadyHeroPointTrain ? 1 : 0);
	std::fprintf(f, "ShowCompassOverlay=%d\n", G::ShowCompassOverlay ? 1 : 0);
	std::fprintf(f, "ShowWorldTrails=%d\n", G::ShowWorldTrails ? 1 : 0);
	std::fprintf(f, "MapAssistEnabled=%d\n", G::MapAssistEnabled ? 1 : 0);
	std::fprintf(f, "MapAssistClickWaypoint=%d\n", G::MapAssistClickWaypoint ? 1 : 0);
	std::fprintf(f, "ShowDirectionCompass=%d\n", G::ShowDirectionCompass ? 1 : 0);
	std::fprintf(f, "DirectionLetterScale=%.2f\n", G::DirectionLetterScale);
	std::fprintf(f, "DirectionWorldRadiusScale=%.2f\n", G::DirectionWorldRadiusScale);
	std::fprintf(f, "HideWhenMapOpen=%d\n", G::HideWhenMapOpen ? 1 : 0);
	std::fprintf(f, "HideOutOfGameplay=%d\n", G::HideOutOfGameplay ? 1 : 0);
	std::fprintf(f, "WorldTrailMaxDist=%.1f\n", G::WorldTrailMaxDist);
	std::fprintf(f, "WorldTrailWidth=%.2f\n", G::WorldTrailWidth);
	std::fprintf(f, "WorldTrailPlayerClear=%.2f\n", G::WorldTrailPlayerClear);
	std::fprintf(f, "WorldMarkerPlayerClear=%.2f\n", G::WorldMarkerPlayerClear);
	std::fprintf(f, "WorldMarkerScale=%.2f\n", G::WorldMarkerScale);
	std::fprintf(f, "CompassMarkerScale=%.2f\n", G::CompassMarkerScale);
	PathingTrails::SerializeEnabledPaths(G::PathingEnabled, sizeof(G::PathingEnabled));
	std::fprintf(f, "PathingEnabled=%s\n", G::PathingEnabled);
	std::fprintf(f, "Opacity=%.4f\n", G::Opacity);
	std::fprintf(f, "FontScale=%.4f\n", G::FontScale);
	std::fprintf(f, "FontScaleAuto=%d\n", G::FontScaleAuto ? 1 : 0);
	std::fprintf(f, "WindowWidth=%.1f\n", G::WindowWidth);
	std::fprintf(f, "WindowHeight=%.1f\n", G::WindowHeight);
	std::fprintf(f, "WindowPosX=%.1f\n", G::WindowPosX);
	std::fprintf(f, "WindowPosY=%.1f\n", G::WindowPosY);
	std::fprintf(f, "LastQuery=%s\n", G::LastQuery);
	std::fprintf(f, "ActiveSiteId=%s\n", G::ActiveSiteId);
	std::fprintf(f, "DefaultSiteId=%s\n", G::DefaultSiteId);
	std::fprintf(f, "KeepHelperWarm=%d\n", G::KeepHelperWarm ? 1 : 0);
	std::fprintf(f, "ShowRailLabels=%d\n", G::ShowRailLabels ? 1 : 0);
	std::fprintf(f, "Gw2ApiKey=%s\n", G::Gw2ApiKey);
	std::fprintf(f, "TpWatchIds=%s\n", G::TpWatchIds);
	std::fprintf(f, "TpWatchAlerts=%s\n", G::TpWatchAlerts);
	std::fprintf(f, "TpWatchBuyAlerts=%s\n", G::TpWatchBuyAlerts);
	std::fprintf(f, "EventTrackIds=%s\n", G::EventTrackIds);
	std::fprintf(f, "EventAlerts=%d\n", G::EventAlerts ? 1 : 0);
	std::fprintf(f, "EventAlertsTrackedOnly=%d\n", G::EventAlertsTrackedOnly ? 1 : 0);
	std::fprintf(f, "EventAlertsThisMap=%d\n", G::EventAlertsThisMap ? 1 : 0);
	std::fprintf(f, "LogFolder=%s\n", G::LogFolder);
	std::fprintf(f, "EliteInsightsPath=%s\n", G::EliteInsightsPath);
	std::fprintf(f, "DpsReportToken=%s\n", G::DpsReportToken);
	std::fprintf(f, "LogManagerListFrac=%.4f\n", G::LogManagerListFrac);
	std::fprintf(f, "LogManagerWinW=%.1f\n", G::LogManagerWinW);
	std::fprintf(f, "LogManagerWinH=%.1f\n", G::LogManagerWinH);
	std::fprintf(f, "LogManagerWinX=%.1f\n", G::LogManagerWinX);
	std::fprintf(f, "LogManagerWinY=%.1f\n", G::LogManagerWinY);
	std::fprintf(f, "LogManagerGroupByEncounter=%d\n", G::LogManagerGroupByEncounter ? 1 : 0);
	std::fprintf(f, "LogManagerAutoParse=%d\n", G::LogManagerAutoParse ? 1 : 0);
	PadDock::WriteGeom(f, "PadAccount", G::PadAccount);
	PadDock::WriteGeom(f, "PadPathing", G::PadPathing);
	PadDock::WriteGeom(f, "PadTrailTools", G::PadTrailTools);
	PadDock::WriteGeom(f, "PadTrailEditor", G::PadTrailEditor);
	PadDock::WriteGeom(f, "PadMarkerEditor", G::PadMarkerEditor);
	PadDock::WriteGeom(f, "PadEvents", G::PadEvents);
	PadDock::WriteGeom(f, "PadNotes", G::PadNotes);
	PadDock::WriteGeom(f, "PadCompass", G::PadCompass);
	PadDock::WriteGeom(f, "PadSettings", G::PadSettings);
	PadDock::WriteGeom(f, "PadTp", G::PadTp);
	PadDock::WriteGeom(f, "PadLookup", G::PadLookup);
	PadDock::WriteGeom(f, "PadWallet", G::PadWallet);
	PadDock::WriteGeom(f, "PadVault", G::PadVault);
	PadDock::WriteGeom(f, "PadEconomy", G::PadEconomy);
	PadDock::WriteGeom(f, "PadInstances", G::PadInstances);
	PadDock::WriteGeom(f, "PadCompletion", G::PadCompletion);
	PadDock::WriteGeom(f, "PadFarming", G::PadFarming);
	PadDock::WriteGeom(f, "PadEventAlert", G::PadEventAlert);
	char favBuf[4096]{};
	Sites::SerializeFavorites(favBuf, sizeof(favBuf));
	std::fprintf(f, "FavoriteIds=%s\n", favBuf);
	UI_WriteBrowseOpen(f);
	BrowserTabs::WriteSettings(f);

	std::fclose(f);
	SettingsDetail::gDirty = false;
	sLastSaveMs = now;
}
