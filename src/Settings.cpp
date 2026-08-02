#include "Settings.h"

#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "Sites.h"
#include "TekkitTrails.h"
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

	char line[768];
	char key[64];
	char val[640];
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
		else if (std::strcmp(key, "ShowTpWatch") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowLookup") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowWallet") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowVault") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowAccount") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowEvents") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowLogManager") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowTekkitGuides") == 0) { /* ignore */ }
		else if (std::strcmp(key, "ShowTekkitTrails") == 0) G::ShowTekkitTrails = AsBool(val);
		else if (std::strcmp(key, "ShowCompassOverlay") == 0) G::ShowCompassOverlay = AsBool(val);
		else if (std::strcmp(key, "ShowWorldTrails") == 0) G::ShowWorldTrails = AsBool(val);
		else if (std::strcmp(key, "HideWhenMapOpen") == 0) G::HideWhenMapOpen = AsBool(val);
		else if (std::strcmp(key, "HideOutOfGameplay") == 0) G::HideOutOfGameplay = AsBool(val);
		else if (std::strcmp(key, "WorldTrailMaxDist") == 0)
			G::WorldTrailMaxDist = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WorldTrailWidth") == 0)
			G::WorldTrailWidth = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "Opacity") == 0) G::Opacity = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "FontScale") == 0) G::FontScale = static_cast<float>(std::atof(val));
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
		else if (std::strcmp(key, "TekkitEnabled") == 0)
			std::snprintf(G::TekkitEnabled, sizeof(G::TekkitEnabled), "%s", val);
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
	if (G::LogManagerListFrac < 0.20f) G::LogManagerListFrac = 0.20f;
	if (G::LogManagerListFrac > 0.72f) G::LogManagerListFrac = 0.72f;
	/* Old shipping default was 0.42 — bump once so existing installs match the
	   filters | list | detail proportions without wiping custom drags. */
	if (G::LogManagerListFrac > 0.415f && G::LogManagerListFrac < 0.425f)
		G::LogManagerListFrac = 0.52f;
	if (G::LogManagerWinW < 880.f) G::LogManagerWinW = 880.f;
	if (G::LogManagerWinH < 420.f) G::LogManagerWinH = 420.f;
	/* Category paths restored in AddonLoad after TekkitTrails::Init(). */

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
	std::fprintf(f, "ShowTekkitGuides=0\n");
	std::fprintf(f, "ShowTekkitTrails=%d\n", G::ShowTekkitTrails ? 1 : 0);
	std::fprintf(f, "ShowCompassOverlay=%d\n", G::ShowCompassOverlay ? 1 : 0);
	std::fprintf(f, "ShowWorldTrails=%d\n", G::ShowWorldTrails ? 1 : 0);
	std::fprintf(f, "HideWhenMapOpen=%d\n", G::HideWhenMapOpen ? 1 : 0);
	std::fprintf(f, "HideOutOfGameplay=%d\n", G::HideOutOfGameplay ? 1 : 0);
	std::fprintf(f, "WorldTrailMaxDist=%.1f\n", G::WorldTrailMaxDist);
	std::fprintf(f, "WorldTrailWidth=%.2f\n", G::WorldTrailWidth);
	TekkitTrails::SerializeEnabledPaths(G::TekkitEnabled, sizeof(G::TekkitEnabled));
	std::fprintf(f, "TekkitEnabled=%s\n", G::TekkitEnabled);
	std::fprintf(f, "Opacity=%.4f\n", G::Opacity);
	std::fprintf(f, "FontScale=%.4f\n", G::FontScale);
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
	char favBuf[640]{};
	Sites::SerializeFavorites(favBuf, sizeof(favBuf));
	std::fprintf(f, "FavoriteIds=%s\n", favBuf);
	UI_WriteBrowseOpen(f);
	BrowserTabs::WriteSettings(f);

	std::fclose(f);
	gDirty = false;
	sLastSaveMs = now;
}
