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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <windows.h>

namespace SettingsDetail
{
	bool gDirty = false;

	const char* SettingsPath(char* out, size_t outLen)
	{
		const std::string dir = AddonPaths::DataDirUtf8();
		if (dir.empty())
		{
			std::snprintf(out, outLen, "GW2-InGame-Helper_settings.ini");
			return out;
		}
		std::snprintf(out, outLen, "%s\\settings.ini", dir.c_str());
		return out;
	}
}

void Settings::SetDirty()
{
	SettingsDetail::gDirty = true;
}

void Settings::SaveNow()
{
	SettingsDetail::gDirty = true;
	Save(true);
}

void Settings::Load()
{
	char path[MAX_PATH]{};
	SettingsDetail::SettingsPath(path, sizeof(path));
	FILE* f = std::fopen(path, "r");
	if (!f)
	{
		Sites::SetActiveById(G::DefaultSiteId[0] ? G::DefaultSiteId : "browse");
		std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
		BrowserTabs::FinalizeLoad();
		return;
	}

	bool sawFontScaleAuto = false;
	bool sawLadyBarefoot = false;
	bool sawLadyWpOnly = false;

	/* PathingEnabled can be multi-KB — old 768-byte fgets truncated category lists
	   so ParseEnabledPaths saw a partial/empty value and reset toggles on reload. */
	char line[8704];
	char key[64];
	char val[8192];
	while (std::fgets(line, sizeof(line), f))
	{
		const char* eq = std::strchr(line, '=');
		if (!eq)
			continue;
		size_t kn = static_cast<size_t>(eq - line);
		if (kn == 0 || kn >= sizeof(key))
			continue;
		std::memcpy(key, line, kn);
		key[kn] = 0;
		std::snprintf(val, sizeof(val), "%s", eq + 1);
		size_t vl = std::strlen(val);
		while (vl > 0 && (val[vl - 1] == '\r' || val[vl - 1] == '\n' || val[vl - 1] == ' '))
			val[--vl] = 0;

		auto AsBool = [](const char* v) {
			return v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y';
		};

		if (std::strcmp(key, "ShowWiki") == 0) G::ShowWiki = AsBool(val);
		else if (std::strcmp(key, "ShowOptions") == 0) G::ShowOptions = AsBool(val);
		/* Pad open flags are session-only — never restore open pads. */
		else if (std::strcmp(key, "ShowNotes") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowCompassPad") == 0) { /* ignore — session only */ }
		else if (std::strcmp(key, "ShowWatch") == 0) { /* ignore — session only */ }
		else if (std::strcmp(key, "ShowWatchMirror") == 0) { /* ignore — session only */ }
		else if (std::strcmp(key, "ShowSettings") == 0) { /* ignore — session only */ }
		else if (std::strcmp(key, "ShowTpWatch") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowLookup") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowWallet") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowVault") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowAccount") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowEvents") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowLogManager") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowPathingGuides") == 0 ||
			std::strcmp(key, "ShowTekkitGuides") == 0) { /* ignore — session only */ }
		else if (std::strcmp(key, "ShowTrailTools") == 0) { /* ignore — session only */ }
		else if (std::strcmp(key, "ShowTrailEditor") == 0) { /* ignore — legacy */ }
		else if (std::strcmp(key, "ShowMarkerEditor") == 0) { /* ignore — legacy */ }
		else if (std::strcmp(key, "TrailToolsLastTrlDir") == 0)
		{
			std::snprintf(TrailToolsDetail::gDraft.lastTrlDir,
				sizeof(TrailToolsDetail::gDraft.lastTrlDir), "%s", val);
		}
		else if (std::strcmp(key, "TrailToolsXmlLayout") == 0)
			TrailToolsDetail::gDraft.xmlLayout = std::atoi(val) != 0 ? 1 : 0;
		else if (std::strcmp(key, "TrailToolsBinds") == 0)
			TrailToolsBinds::Deserialize(val);
		else if (std::strcmp(key, "PanelBinds") == 0)
			PanelBinds::Deserialize(val);
		else if (std::strcmp(key, "ShowPathingTrails") == 0 ||
			std::strcmp(key, "ShowTekkitTrails") == 0)
			G::ShowPathingTrails = AsBool(val);
		else if (std::strcmp(key, "EnablePathingLua") == 0)
			G::EnablePathingLua = AsBool(val);
		else if (std::strcmp(key, "LadyBarefoot") == 0)
		{
			G::LadyBarefoot = AsBool(val);
			sawLadyBarefoot = true;
		}
		else if (std::strcmp(key, "LadyWpOnly") == 0)
		{
			G::LadyWpOnly = AsBool(val);
			sawLadyWpOnly = true;
		}
		else if (std::strcmp(key, "LadyRouteEdition") == 0)
		{
			/* Legacy only when new keys were not present in this file. */
			if (sawLadyBarefoot || sawLadyWpOnly)
				continue;
			const int ed = std::atoi(val);
			if (ed == 1)
			{
				G::LadyBarefoot = true;
				G::LadyWpOnly = false;
				G::LadyWithMounts = true;
			}
			else if (ed == 2)
			{
				G::LadyBarefoot = false;
				G::LadyWpOnly = true;
			}
			else if (ed == 0)
			{
				G::LadyBarefoot = true;
				G::LadyWpOnly = false;
			}
			else
			{
				G::LadyBarefoot = false;
				G::LadyWpOnly = false;
			}
		}
		else if (std::strcmp(key, "LadyWithMounts") == 0)
			G::LadyWithMounts = AsBool(val);
		else if (std::strcmp(key, "LadyHearts") == 0)
			G::LadyHearts = AsBool(val);
		else if (std::strcmp(key, "LadyHeroPointTrain") == 0)
			G::LadyHeroPointTrain = AsBool(val);
		else if (std::strcmp(key, "ShowCompassOverlay") == 0) G::ShowCompassOverlay = AsBool(val);
		else if (std::strcmp(key, "ShowWorldTrails") == 0) G::ShowWorldTrails = AsBool(val);
		else if (std::strcmp(key, "MapAssistEnabled") == 0) G::MapAssistEnabled = AsBool(val);
		else if (std::strcmp(key, "MapAssistClickWaypoint") == 0) G::MapAssistClickWaypoint = AsBool(val);
		else if (std::strcmp(key, "ShowDirectionCompass") == 0) G::ShowDirectionCompass = AsBool(val);
		else if (std::strcmp(key, "DirectionLetterScale") == 0)
			G::DirectionLetterScale = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "DirectionWorldRadiusScale") == 0)
			G::DirectionWorldRadiusScale = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "HideWhenMapOpen") == 0) G::HideWhenMapOpen = AsBool(val);
		else if (std::strcmp(key, "HideOutOfGameplay") == 0) G::HideOutOfGameplay = AsBool(val);
		else if (std::strcmp(key, "WorldTrailMaxDist") == 0)
			G::WorldTrailMaxDist = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WorldTrailWidth") == 0)
			G::WorldTrailWidth = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WorldTrailPlayerClear") == 0)
			G::WorldTrailPlayerClear = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WorldMarkerPlayerClear") == 0)
			G::WorldMarkerPlayerClear = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WorldMarkerScale") == 0)
			G::WorldMarkerScale = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "CompassMarkerScale") == 0)
			G::CompassMarkerScale = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "Opacity") == 0) G::Opacity = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "FontScale") == 0)
			G::FontScale = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "FontScaleAuto") == 0)
		{
			G::FontScaleAuto = AsBool(val);
			sawFontScaleAuto = true;
		}
		else if (std::strcmp(key, "ThemeId") == 0)
			std::snprintf(G::ThemeId, sizeof(G::ThemeId), "%s", val);
		else if (std::strcmp(key, "WatchCropTop") == 0)
			G::WatchCropTop = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WatchCropBottom") == 0)
			G::WatchCropBottom = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WatchCropLeft") == 0)
			G::WatchCropLeft = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WatchCropRight") == 0)
			G::WatchCropRight = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WindowWidth") == 0)
		{
			G::WindowWidth = static_cast<float>(std::atof(val));
			G::HasSavedSize = true;
		}
		else if (std::strcmp(key, "WindowHeight") == 0)
		{
			G::WindowHeight = static_cast<float>(std::atof(val));
			G::HasSavedSize = true;
		}
		else if (std::strcmp(key, "WindowPosX") == 0)
		{
			G::WindowPosX = static_cast<float>(std::atof(val));
			G::HasSavedPos = true;
		}
		else if (std::strcmp(key, "WindowPosY") == 0)
		{
			G::WindowPosY = static_cast<float>(std::atof(val));
			G::HasSavedPos = true;
		}
		else if (std::strcmp(key, "LastQuery") == 0)
			std::snprintf(G::LastQuery, sizeof(G::LastQuery), "%s", val);
		else if (std::strcmp(key, "ActiveSiteId") == 0)
			std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", val);
		else if (std::strcmp(key, "DefaultSiteId") == 0)
			std::snprintf(G::DefaultSiteId, sizeof(G::DefaultSiteId), "%s", val);
		else if (std::strcmp(key, "KeepHelperWarm") == 0)
			G::KeepHelperWarm = AsBool(val);
		else if (std::strcmp(key, "ShowRailLabels") == 0)
			G::ShowRailLabels = AsBool(val);
		else if (std::strcmp(key, "Gw2ApiKey") == 0)
			std::snprintf(G::Gw2ApiKey, sizeof(G::Gw2ApiKey), "%s", val);
		else if (std::strcmp(key, "TpWatchIds") == 0)
			std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", val);
		else if (std::strcmp(key, "TpWatchAlerts") == 0)
			std::snprintf(G::TpWatchAlerts, sizeof(G::TpWatchAlerts), "%s", val);
		else if (std::strcmp(key, "TpWatchBuyAlerts") == 0)
			std::snprintf(G::TpWatchBuyAlerts, sizeof(G::TpWatchBuyAlerts), "%s", val);
		else if (std::strcmp(key, "EventTrackIds") == 0)
			std::snprintf(G::EventTrackIds, sizeof(G::EventTrackIds), "%s", val);
		else if (std::strcmp(key, "EventAlerts") == 0)
			G::EventAlerts = AsBool(val);
		else if (std::strcmp(key, "EventAlertsTrackedOnly") == 0)
			G::EventAlertsTrackedOnly = AsBool(val);
		else if (std::strcmp(key, "EventAlertsThisMap") == 0)
			G::EventAlertsThisMap = AsBool(val);
		else if (std::strcmp(key, "PathingEnabled") == 0 ||
			std::strcmp(key, "TekkitEnabled") == 0)
			std::snprintf(G::PathingEnabled, sizeof(G::PathingEnabled), "%s", val);
		else if (std::strcmp(key, "LogFolder") == 0)
			std::snprintf(G::LogFolder, sizeof(G::LogFolder), "%s", val);
		else if (std::strcmp(key, "EliteInsightsPath") == 0)
			std::snprintf(G::EliteInsightsPath, sizeof(G::EliteInsightsPath), "%s", val);
		else if (std::strcmp(key, "DpsReportToken") == 0)
			std::snprintf(G::DpsReportToken, sizeof(G::DpsReportToken), "%s", val);
		else if (std::strcmp(key, "LogManagerListFrac") == 0)
			G::LogManagerListFrac = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "LogManagerWinW") == 0)
			G::LogManagerWinW = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "LogManagerWinH") == 0)
			G::LogManagerWinH = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "LogManagerWinX") == 0)
			G::LogManagerWinX = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "LogManagerWinY") == 0)
			G::LogManagerWinY = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "LogManagerGroupByEncounter") == 0)
			G::LogManagerGroupByEncounter = std::atoi(val) != 0;
		else if (std::strcmp(key, "LogManagerAutoParse") == 0)
			G::LogManagerAutoParse = std::atoi(val) != 0;
		else if (std::strcmp(key, "PadAccount") == 0)
			PadDock::ParseGeom(val, G::PadAccount);
		else if (std::strcmp(key, "PadPathing") == 0)
			PadDock::ParseGeom(val, G::PadPathing);
		else if (std::strcmp(key, "PadTrailTools") == 0)
			PadDock::ParseGeom(val, G::PadTrailTools);
		else if (std::strcmp(key, "PadTrailEditor") == 0)
			PadDock::ParseGeom(val, G::PadTrailEditor);
		else if (std::strcmp(key, "PadMarkerEditor") == 0)
			PadDock::ParseGeom(val, G::PadMarkerEditor);
		else if (std::strcmp(key, "PadEvents") == 0)
			PadDock::ParseGeom(val, G::PadEvents);
		else if (std::strcmp(key, "PadNotes") == 0)
			PadDock::ParseGeom(val, G::PadNotes);
		else if (std::strcmp(key, "PadCompass") == 0)
			PadDock::ParseGeom(val, G::PadCompass);
		else if (std::strcmp(key, "PadWatch") == 0)
			PadDock::ParseGeom(val, G::PadWatch);
		else if (std::strcmp(key, "PadWatchMirror") == 0)
			PadDock::ParseGeom(val, G::PadWatchMirror);
		else if (std::strcmp(key, "PadSettings") == 0)
			PadDock::ParseGeom(val, G::PadSettings);
		else if (std::strcmp(key, "PadTp") == 0)
			PadDock::ParseGeom(val, G::PadTp);
		else if (std::strcmp(key, "PadLookup") == 0)
			PadDock::ParseGeom(val, G::PadLookup);
		else if (std::strcmp(key, "PadWallet") == 0)
			PadDock::ParseGeom(val, G::PadWallet);
		else if (std::strcmp(key, "PadVault") == 0)
			PadDock::ParseGeom(val, G::PadVault);
		else if (std::strcmp(key, "PadEconomy") == 0)
			PadDock::ParseGeom(val, G::PadEconomy);
		else if (std::strcmp(key, "PadInstances") == 0)
			PadDock::ParseGeom(val, G::PadInstances);
		else if (std::strcmp(key, "PadCompletion") == 0)
			PadDock::ParseGeom(val, G::PadCompletion);
		else if (std::strcmp(key, "PadFarming") == 0)
			PadDock::ParseGeom(val, G::PadFarming);
		else if (std::strcmp(key, "PadEventAlert") == 0)
			PadDock::ParseGeom(val, G::PadEventAlert);
		else if (std::strcmp(key, "FavoriteIds") == 0)
			Sites::ParseFavorites(val);
		else if (std::strcmp(key, "BrowseOpen") == 0)
			UI_ParseBrowseOpen(val);
		else
			BrowserTabs::ParseKey(key, val);
	}
	std::fclose(f);

	if (G::Opacity < 0.15f) G::Opacity = 0.15f;
	if (G::Opacity > 1.f) G::Opacity = 1.f;
	auto clampCrop = [](float& v) {
		if (v < 0.f) v = 0.f;
		if (v > 0.45f) v = 0.45f;
	};
	clampCrop(G::WatchCropTop);
	clampCrop(G::WatchCropBottom);
	clampCrop(G::WatchCropLeft);
	clampCrop(G::WatchCropRight);
	if (G::WatchCropTop + G::WatchCropBottom > 0.85f)
		G::WatchCropBottom = (std::max)(0.f, 0.85f - G::WatchCropTop);
	if (G::WatchCropLeft + G::WatchCropRight > 0.85f)
		G::WatchCropRight = (std::max)(0.f, 0.85f - G::WatchCropLeft);
	if (G::FontScale < 0.75f) G::FontScale = 0.75f;
	if (G::FontScale > 2.f) G::FontScale = 2.f;
	/* One-shot: early auto used Nexus UI scale and pushed many installs to ~2×.
	   Default is 1.0; manual slider values persist when auto is off. */
	if (G::FontScaleAuto || !sawFontScaleAuto)
	{
		if (G::FontScaleAuto)
		{
			G::FontScale = 1.f;
			SettingsDetail::gDirty = true;
		}
		G::FontScaleAuto = false;
	}
	if (G::WindowWidth < 320.f) G::WindowWidth = 320.f;
	if (G::WindowHeight < 240.f) G::WindowHeight = 240.f;

	if (Sites::IndexOfId(G::DefaultSiteId) < 0 ||
		std::strcmp(G::DefaultSiteId, "gw2lunchbox") == 0 ||
		std::strcmp(G::DefaultSiteId, "home") == 0)
	{
		/* Former factory default was How to use (home); Browse hub is the landing now. */
		std::snprintf(G::DefaultSiteId, sizeof(G::DefaultSiteId), "browse");
	}
	if (!Sites::SetActiveById(G::ActiveSiteId) ||
		std::strcmp(G::ActiveSiteId, "gw2lunchbox") == 0 ||
		std::strcmp(G::ActiveSiteId, "home") == 0)
	{
		std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", G::DefaultSiteId);
		Sites::SetActiveById(G::ActiveSiteId);
	}
	/* Favorites pruned after Sites::Init() in AddonLoad — catalog is empty here. */
	BrowserTabs::FinalizeLoad();

	if (G::WorldTrailMaxDist < 40.f) G::WorldTrailMaxDist = 40.f;
	if (G::WorldTrailMaxDist > 200.f) G::WorldTrailMaxDist = 200.f;
	if (G::WorldTrailWidth < 0.5f) G::WorldTrailWidth = 0.5f;
	if (G::WorldTrailWidth > 4.f) G::WorldTrailWidth = 4.f;
	if (G::WorldTrailPlayerClear < 0.f) G::WorldTrailPlayerClear = 0.f;
	if (G::WorldTrailPlayerClear > 3.f) G::WorldTrailPlayerClear = 3.f;
	if (G::WorldMarkerPlayerClear < 0.f) G::WorldMarkerPlayerClear = 0.f;
	if (G::WorldMarkerPlayerClear > 3.f) G::WorldMarkerPlayerClear = 3.f;
	if (G::WorldMarkerScale < 0.5f) G::WorldMarkerScale = 0.5f;
	if (G::WorldMarkerScale > 3.f) G::WorldMarkerScale = 3.f;
	if (G::CompassMarkerScale < 0.5f) G::CompassMarkerScale = 0.5f;
	if (G::CompassMarkerScale > 3.f) G::CompassMarkerScale = 3.f;
	if (G::DirectionLetterScale < 0.5f) G::DirectionLetterScale = 0.5f;
	if (G::DirectionLetterScale > 2.5f) G::DirectionLetterScale = 2.5f;
	if (G::DirectionWorldRadiusScale < 0.4f) G::DirectionWorldRadiusScale = 0.4f;
	if (G::DirectionWorldRadiusScale > 3.0f) G::DirectionWorldRadiusScale = 3.0f;
	if (G::LogManagerListFrac < 0.20f) G::LogManagerListFrac = 0.20f;
	if (G::LogManagerListFrac > 0.72f) G::LogManagerListFrac = 0.72f;
	/* Old shipping defaults — bump once toward screenshot middle-pane width. */
	if (G::LogManagerListFrac > 0.415f && G::LogManagerListFrac < 0.425f)
		G::LogManagerListFrac = 0.55f;
	if (G::LogManagerListFrac > 0.445f && G::LogManagerListFrac < 0.455f)
		G::LogManagerListFrac = 0.55f;
	if (G::LogManagerListFrac > 0.465f && G::LogManagerListFrac < 0.475f)
		G::LogManagerListFrac = 0.55f;
	if (G::LogManagerListFrac > 0.515f && G::LogManagerListFrac < 0.525f)
		G::LogManagerListFrac = 0.55f;
	/* Widen cramped defaults that cannot show the three-pane layout. */
	if (G::LogManagerWinW > 0.f && G::LogManagerWinW < 1400.f)
		G::LogManagerWinW = 1760.f;
	if (G::LogManagerWinH > 0.f && G::LogManagerWinH < 750.f)
		G::LogManagerWinH = 900.f;
	if (G::LogManagerWinW < 960.f) G::LogManagerWinW = 960.f;
	if (G::LogManagerWinH < 480.f) G::LogManagerWinH = 480.f;
	/* Category paths restored in AddonLoad after PathingTrails::Init(). */

	SettingsDetail::gDirty = false;
}

