#include "Settings.h"

#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "Sites.h"

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
		else if (std::strcmp(key, "Opacity") == 0) G::Opacity = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "FontScale") == 0) G::FontScale = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WindowWidth") == 0) G::WindowWidth = static_cast<float>(std::atof(val));
		else if (std::strcmp(key, "WindowHeight") == 0) G::WindowHeight = static_cast<float>(std::atof(val));
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
		else if (std::strcmp(key, "FavoriteIds") == 0)
			Sites::ParseFavorites(val);
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

	if (!Sites::SetActiveById(G::DefaultSiteId) ||
		std::strcmp(G::DefaultSiteId, "gw2lunchbox") == 0)
	{
		Sites::SetActiveById("home");
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

	gDirty = false;
}

void Settings::Save()
{
	if (!gDirty)
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
	std::fprintf(f, "Opacity=%.4f\n", G::Opacity);
	std::fprintf(f, "FontScale=%.4f\n", G::FontScale);
	std::fprintf(f, "WindowWidth=%.1f\n", G::WindowWidth);
	std::fprintf(f, "WindowHeight=%.1f\n", G::WindowHeight);
	std::fprintf(f, "WindowPosX=%.1f\n", G::WindowPosX);
	std::fprintf(f, "WindowPosY=%.1f\n", G::WindowPosY);
	std::fprintf(f, "LastQuery=%s\n", G::LastQuery);
	std::fprintf(f, "ActiveSiteId=%s\n", G::ActiveSiteId);
	std::fprintf(f, "DefaultSiteId=%s\n", G::DefaultSiteId);
	char favBuf[640]{};
	Sites::SerializeFavorites(favBuf, sizeof(favBuf));
	std::fprintf(f, "FavoriteIds=%s\n", favBuf);
	BrowserTabs::WriteSettings(f);

	std::fclose(f);
	gDirty = false;
}
