#include "WikiBrowser.h"
#include "WikiBrowserShared.h"

#include "AddonPaths.h"
#include "Globals.h"

#include <string>

#include <windows.h>

namespace WikiBrowserDetail
{
	static void DeleteIfExists(const std::wstring& path)
	{
		DeleteFileW(path.c_str());
	}

	static void DeleteGlobInDir(const std::wstring& dir, const wchar_t* pattern)
	{
		if (dir.empty() || !pattern)
			return;
		WIN32_FIND_DATAW fd{};
		const std::wstring glob = dir + L"\\" + pattern;
		HANDLE h = FindFirstFileW(glob.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			DeleteFileW((dir + L"\\" + fd.cFileName).c_str());
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}

	static bool PathExistsW(const std::wstring& path)
	{
		return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	/* Move root file into config/ when dest is missing; drop root orphan if dest exists. */
	static void MigrateRootFileToConfig(const std::wstring& data, const wchar_t* fileName)
	{
		if (!fileName || !fileName[0])
			return;
		const std::wstring src = data + L"\\" + fileName;
		if (!PathExistsW(src))
			return;
		const std::wstring cfg = AddonPaths::ConfigDir();
		const std::wstring dst = cfg + L"\\" + fileName;
		if (PathExistsW(dst))
		{
			DeleteFileW(src.c_str());
			return;
		}
		if (!MoveFileW(src.c_str(), dst.c_str()))
		{
			/* Fallback: leave src if move fails (e.g. cross-volume / lock). */
		}
	}

	/* One-time upgrade: generated HTML / cmd / caches moved out of data root;
	   small state files into config/. */
	void MigrateLegacyAddonDataLayout()
	{
		const std::wstring data = AddonDir();
		if (data.empty())
			return;

		static const wchar_t* kConfigFiles[] = {
			L"notes.json",
			L"profiles.json",
			L"session_history.json",
			L"confirmed_waypoints.json",
			L"log-index.json",
			L"marker_behaviors.txt",
			L"ei-helper.conf",
			L"favorites.json",
		};
		for (const wchar_t* name : kConfigFiles)
			MigrateRootFileToConfig(data, name);

		const wchar_t* rootOrphans[] = {
			L"\\helper-home.html",
			L"\\helper-home.ver",
			L"\\home-logo.png",
			L"\\home-cover.jpg",
			L"\\raid-food.html",
			L"\\raid-food.ver",
			L"\\live-tp-cmd.txt",
			L"\\craft-plan-cmd.txt",
			L"\\legendary-detail-cmd.txt",
			L"\\open-about-cmd.txt",
			L"\\waypoints-index.cache",
			L"\\stash-names.cache",
			L"\\unlocks-skins.cache",
			L"\\unlocks-dyes.cache",
			L"\\unlocks-minis.cache",
			L"\\unlocks-finishers.cache",
			L"\\unlocks-outfits.cache",
			L"\\unlocks-gliders.cache",
			L"\\unlocks-mailcarriers.cache",
			L"\\unlocks-novelties.cache",
			L"\\unlocks-titles.cache",
			L"\\live-armory.json",
			L"\\live-armory-names.json",
			L"\\live-colors.json",
			L"\\gw2-api-check.html",
			L"\\gw2-api-check.ver",
			L"\\gw2-api-check.ok",
		};
		for (const wchar_t* name : rootOrphans)
			DeleteIfExists(data + name);

		DeleteGlobInDir(data, L"live-*.html");
		DeleteGlobInDir(data, L"live-*.ver");
		DeleteGlobInDir(data, L"live-*.ok");
		DeleteGlobInDir(data, L"live-*.json");
		DeleteGlobInDir(data, L"live-leg-craft-*.json");
		DeleteGlobInDir(data, L"*-cmd.txt");
	}

	void CleanupStaleAddonRootFiles()
	{
		const std::wstring data = AddonDir();
		wchar_t path[MAX_PATH]{};
		if (!G::Self || !GetModuleFileNameW(G::Self, path, MAX_PATH))
			return;
		std::wstring full = path;
		const size_t slash = full.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return;
		const std::wstring root = full.substr(0, slash);
		if (root.empty() || _wcsicmp(root.c_str(), data.c_str()) == 0)
			return;

		const wchar_t* stale[] = {
			L"\\GW2HelperBrowser.exe",
			L"\\helper-home.html",
			L"\\helper-home.ver",
			L"\\home-logo.png",
			L"\\home-cover.jpg",
			L"\\raid-food.html",
			L"\\raid-food.ver",
			L"\\raid-utilities.html",
			L"\\raid-utilities.ver",
			L"\\fractal-consumables.html",
			L"\\fractal-consumables.ver",
			L"\\sigils-runes.html",
			L"\\sigils-runes.ver",
			L"\\relics-guide.html",
			L"\\relics-guide.ver",
			L"\\boon-checklist.html",
			L"\\boon-checklist.ver",
			L"\\cc-defiance.html",
			L"\\cc-defiance.ver",
			L"\\raid-wings.html",
			L"\\raid-wings.ver",
			L"\\home-garden.html",
			L"\\home-garden.ver",
			L"\\ubers-all-in-one.html",
			L"\\ubers-all-in-one.ver",
			L"\\strike-missions.html",
			L"\\strike-missions.ver",
			L"\\fractal-cm-list.html",
			L"\\fractal-cm-list.ver",
			L"\\squad-template.html",
			L"\\squad-template.ver",
			L"\\stability-cleanse.html",
			L"\\stability-cleanse.ver",
			L"\\material-conversions.html",
			L"\\material-conversions.ver",
			L"\\legendary-paths.html",
			L"\\legendary-paths.ver",
			L"\\live-legendary-vault.html",
			L"\\live-legendary-vault.ver",
			L"\\live-legendary-vault.ok",
			L"\\craft-plan-cmd.txt",
			L"\\open-about-cmd.txt",
			L"\\live-cheatsheets-hub.html",
			L"\\live-cheatsheets-hub.ver",
			L"\\live-cheatsheets-hub.ok",
			L"\\legendary-detail-cmd.txt",
			L"\\mount-unlock.html",
			L"\\mount-unlock.ver",
			L"\\daily-weekly.html",
			L"\\daily-weekly.ver",
			L"\\live-dailies.html",
			L"\\live-dailies.ver",
			L"\\live-dailies.ok",
			L"\\live-news.html",
			L"\\live-news.ver",
			L"\\live-news.ok",
			L"\\live-fashion.html",
			L"\\live-fashion.ver",
			L"\\live-fashion.ok",
			L"\\live-tp.html",
			L"\\live-tp.ver",
			L"\\live-tp.ok",
			L"\\live-tp-cmd.txt",
			L"\\live-progress.html",
			L"\\live-progress.ver",
			L"\\live-progress.ok",
			L"\\live-colors.json",
			L"\\live-armory.json",
			L"\\live-armory-names.json",
			L"\\currency-sinks.html",
			L"\\currency-sinks.ver",
			L"\\ascended-start.html",
			L"\\ascended-start.ver",
			L"\\portals-pulls.html",
			L"\\portals-pulls.ver",
			L"\\homestead-extras.html",
			L"\\homestead-extras.ver",
			L"\\wvw-consumables.html",
			L"\\wvw-consumables.ver",
			L"\\dps-log-setup.html",
			L"\\dps-log-setup.ver",
			L"\\api-key-setup.html",
			L"\\api-key-setup.ver",
			L"\\waypoints-index.cache",
			L"\\stash-names.cache",
			L"\\settings.ini",
		};
		for (const wchar_t* name : stale)
		{
			const std::wstring p = root + name;
			DeleteFileW(p.c_str());
		}
		DeleteGlobInDir(root, L"live-*.html");
		DeleteGlobInDir(root, L"live-*.ver");
		DeleteGlobInDir(root, L"live-*.ok");
		DeleteGlobInDir(root, L"live-*.json");
		DeleteGlobInDir(root, L"*-cmd.txt");
		DeleteGlobInDir(root, L"unlocks-*.cache");
	}
}
