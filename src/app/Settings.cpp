#include "Settings.h"

#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "PadDock.h"
#include "Sites.h"
#include "PathingTrails.h"
#include "UI.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <windows.h>

namespace
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
	gDirty = true;
}

void Settings::SaveNow()
{
	gDirty = true;
	Save(true);
}

void Settings::Load()
{
	char path[MAX_PATH]{};
	SettingsPath(path, sizeof(path));
	FILE* f = std::fopen(path, "r");
	if (!f)
	{
		Sites::SetActiveById(G::DefaultSiteId[0] ? G::DefaultSiteId : "home");
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
		else if (std::strcmp(key, "ShowPathingTrails") == 0 ||
			std::strcmp(key, "ShowTekkitTrails") == 0)
			G::ShowPathingTrails = AsBool(val);
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
		else if (std::strcmp(key, "Gw2ApiKey") == 0)
			std::snprintf(G::Gw2ApiKey, sizeof(G::Gw2ApiKey), "%s", val);
		else if (std::strcmp(key, "TpWatchIds") == 0)
			std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", val);
		else if (std::strcmp(key, "TpWatchAlerts") == 0)
			std::snprintf(G::TpWatchAlerts, sizeof(G::TpWatchAlerts), "%s", val);
		else if (std::strcmp(key, "EventTrackIds") == 0)
			std::snprintf(G::EventTrackIds, sizeof(G::EventTrackIds), "%s", val);
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
		else if (std::strcmp(key, "PadEvents") == 0)
			PadDock::ParseGeom(val, G::PadEvents);
		else if (std::strcmp(key, "PadNotes") == 0)
			PadDock::ParseGeom(val, G::PadNotes);
		else if (std::strcmp(key, "PadCompass") == 0)
			PadDock::ParseGeom(val, G::PadCompass);
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
	if (G::FontScale < 0.75f) G::FontScale = 0.75f;
	if (G::FontScale > 2.f) G::FontScale = 2.f;
	/* One-shot: early auto used Nexus UI scale and pushed many installs to ~2×.
	   Default is 1.0; manual slider values persist when auto is off. */
	if (G::FontScaleAuto || !sawFontScaleAuto)
	{
		if (G::FontScaleAuto)
		{
			G::FontScale = 1.f;
			gDirty = true;
		}
		G::FontScaleAuto = false;
	}
	if (G::WindowWidth < 320.f) G::WindowWidth = 320.f;
	if (G::WindowHeight < 240.f) G::WindowHeight = 240.f;

	if (Sites::IndexOfId(G::DefaultSiteId) < 0 ||
		std::strcmp(G::DefaultSiteId, "gw2lunchbox") == 0)
	{
		std::snprintf(G::DefaultSiteId, sizeof(G::DefaultSiteId), "home");
	}
	if (!Sites::SetActiveById(G::ActiveSiteId) ||
		std::strcmp(G::ActiveSiteId, "gw2lunchbox") == 0)
	{
		std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", G::DefaultSiteId);
		Sites::SetActiveById(G::ActiveSiteId);
	}
	Sites::PruneFavorites();
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

	gDirty = false;
}

void Settings::Save(bool force)
{
	if (!gDirty)
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
	SettingsPath(path, sizeof(path));
	FILE* f = std::fopen(path, "w");
	if (!f)
		return;

	std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
	if (!G::DefaultSiteId[0])
		std::snprintf(G::DefaultSiteId, sizeof(G::DefaultSiteId), "home");

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
	std::fprintf(f, "ShowPathingGuides=0\n");
	std::fprintf(f, "ShowPathingTrails=%d\n", G::ShowPathingTrails ? 1 : 0);
	std::fprintf(f, "LadyBarefoot=%d\n", G::LadyBarefoot ? 1 : 0);
	std::fprintf(f, "LadyWpOnly=%d\n", G::LadyWpOnly ? 1 : 0);
	std::fprintf(f, "LadyWithMounts=%d\n", G::LadyWithMounts ? 1 : 0);
	std::fprintf(f, "LadyHearts=%d\n", G::LadyHearts ? 1 : 0);
	std::fprintf(f, "LadyHeroPointTrain=%d\n", G::LadyHeroPointTrain ? 1 : 0);
	std::fprintf(f, "ShowCompassOverlay=%d\n", G::ShowCompassOverlay ? 1 : 0);
	std::fprintf(f, "ShowWorldTrails=%d\n", G::ShowWorldTrails ? 1 : 0);
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
	std::fprintf(f, "Gw2ApiKey=%s\n", G::Gw2ApiKey);
	std::fprintf(f, "TpWatchIds=%s\n", G::TpWatchIds);
	std::fprintf(f, "TpWatchAlerts=%s\n", G::TpWatchAlerts);
	std::fprintf(f, "EventTrackIds=%s\n", G::EventTrackIds);
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
	PadDock::WriteGeom(f, "PadEvents", G::PadEvents);
	PadDock::WriteGeom(f, "PadNotes", G::PadNotes);
	PadDock::WriteGeom(f, "PadCompass", G::PadCompass);
	PadDock::WriteGeom(f, "PadSettings", G::PadSettings);
	PadDock::WriteGeom(f, "PadTp", G::PadTp);
	PadDock::WriteGeom(f, "PadLookup", G::PadLookup);
	PadDock::WriteGeom(f, "PadWallet", G::PadWallet);
	PadDock::WriteGeom(f, "PadVault", G::PadVault);
	char favBuf[640]{};
	Sites::SerializeFavorites(favBuf, sizeof(favBuf));
	std::fprintf(f, "FavoriteIds=%s\n", favBuf);
	UI_WriteBrowseOpen(f);
	BrowserTabs::WriteSettings(f);

	std::fclose(f);
	gDirty = false;
	sLastSaveMs = now;
}
