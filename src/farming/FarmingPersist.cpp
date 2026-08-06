#include "FarmingShared.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace FarmingDetail
{
	bool gFocus = false;
	bool gPlaceOnce = false;
	int gTab = 0;
	int gSelectedRun = 0;
	char gStatus[160]{};
	char gFishName[64]{};
	char gFishMap[64]{};
	std::vector<Run> gRuns;
	std::vector<FishEntry> gFishLog;

	namespace
	{
		std::wstring PersistPath()
		{
			return AddonPaths::ConfigDir() + L"\\farming-state.txt";
		}

		void AddStep(Run& r, const char* text)
		{
			RunStep s{};
			std::snprintf(s.text, sizeof(s.text), "%s", text ? text : "");
			r.steps.push_back(s);
		}
	}

	void EnsureSeed()
	{
		if (!gRuns.empty()) return;
		{
			Run r{}; r.id = 1;
			std::snprintf(r.name, sizeof(r.name), "Silverwastes farm loop");
			std::snprintf(r.pathingHint, sizeof(r.pathingHint), "tw_guides.tw_gatheringnodes");
			AddStep(r, "Enter The Silverwastes");
			AddStep(r, "Clear fort / breach events");
			AddStep(r, "Loot chests | gather nodes");
			AddStep(r, "Bank / salvage | repeat");
			gRuns.push_back(r);
		}
		{
			Run r{}; r.id = 2;
			std::snprintf(r.name, sizeof(r.name), "Dry Top crystals");
			std::snprintf(r.pathingHint, sizeof(r.pathingHint), "tw_guides.tw_gatheringnodes");
			AddStep(r, "Enter Dry Top");
			AddStep(r, "Crash site / aspect events");
			AddStep(r, "Mine crystals | pack assist");
			gRuns.push_back(r);
		}
		{
			Run r{}; r.id = 3;
			std::snprintf(r.name, sizeof(r.name), "Fishing - Kryta coasts");
			std::snprintf(r.pathingHint, sizeof(r.pathingHint), "tw_guides.tw_fishing");
			AddStep(r, "Equip fishing gear");
			AddStep(r, "Visit coastal holes");
			AddStep(r, "Log catches in Fishing tab");
			gRuns.push_back(r);
		}
		{
			Run r{}; r.id = 4;
			std::snprintf(r.name, sizeof(r.name), "Home instance nodes");
			std::snprintf(r.pathingHint, sizeof(r.pathingHint), "tw_guides.tw_gatheringnodes");
			AddStep(r, "Enter home instance");
			AddStep(r, "Gather planted nodes");
			AddStep(r, "Collect daily chests");
			gRuns.push_back(r);
		}
	}

	void ToggleStep(size_t run, size_t step)
	{
		EnsureSeed();
		if (run >= gRuns.size() || step >= gRuns[run].steps.size()) return;
		gRuns[run].steps[step].done = !gRuns[run].steps[step].done;
		Save();
	}

	void ResetRun(size_t run)
	{
		EnsureSeed();
		if (run >= gRuns.size()) return;
		for (RunStep& s : gRuns[run].steps) s.done = false;
		Save();
	}

	void AddFish(const char* name, const char* map)
	{
		if (!name || !name[0]) return;
		for (FishEntry& e : gFishLog)
		{
			if (_stricmp(e.name, name) == 0 &&
				(!map || !map[0] || _stricmp(e.map, map ? map : "") == 0))
			{
				++e.count;
				Save();
				return;
			}
		}
		FishEntry e{};
		std::snprintf(e.name, sizeof(e.name), "%s", name);
		if (map) std::snprintf(e.map, sizeof(e.map), "%s", map);
		e.count = 1;
		gFishLog.push_back(e);
		Save();
	}

	void ClearFish()
	{
		gFishLog.clear();
		Save();
	}

	bool StartRunPathing(size_t run)
	{
		EnsureSeed();
		if (run >= gRuns.size()) return false;
		const Run& r = gRuns[run];
		G::ShowPathingTrails = true;
		PathingTrails::SetMasterEnabled(true);
		PathingTrails::SetCategoryEnabled("tw_guides", true);
		if (r.pathingHint[0])
			PathingTrails::SetCategoryEnabled(r.pathingHint, true);
		PathingGuidesPad::Open();
		std::snprintf(gStatus, sizeof(gStatus),
			"Pathing open - enabled \"%s\".", r.pathingHint[0] ? r.pathingHint : "tw_guides");
		Settings::SetDirty();
		return true;
	}

	void Load()
	{
		EnsureSeed();
		const std::wstring path = PersistPath();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 512 * 1024)
		{ CloseHandle(h); return; }
		std::string raw(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD rd = 0;
		if (!ReadFile(h, raw.data(), static_cast<DWORD>(raw.size()), &rd, nullptr))
		{ CloseHandle(h); return; }
		CloseHandle(h);
		raw.resize(rd);
		gFishLog.clear();
		size_t i = 0;
		while (i < raw.size())
		{
			while (i < raw.size() && (raw[i] == '\r' || raw[i] == '\n')) ++i;
			size_t s = i;
			while (i < raw.size() && raw[i] != '\r' && raw[i] != '\n') ++i;
			if (s >= i) continue;
			std::string line = raw.substr(s, i - s);
			if (line.rfind("R ", 0) == 0)
			{
				int rid = 0, step = 0;
				if (std::sscanf(line.c_str() + 2, "%d %d", &rid, &step) == 2)
				{
					for (Run& r : gRuns)
						if (r.id == rid && step >= 0 &&
							static_cast<size_t>(step) < r.steps.size())
							r.steps[static_cast<size_t>(step)].done = true;
				}
			}
			else if (line.rfind("F ", 0) == 0)
			{
				int count = 0;
				char name[64]{}, map[64]{};
				if (std::sscanf(line.c_str() + 2, "%d\t%63[^\t]\t%63[^\n]",
					&count, name, map) >= 2)
				{
					FishEntry e{};
					e.count = count > 0 ? count : 1;
					std::snprintf(e.name, sizeof(e.name), "%s", name);
					std::snprintf(e.map, sizeof(e.map), "%s", map);
					gFishLog.push_back(e);
				}
			}
		}
	}

	void Save()
	{
		const std::wstring path = PersistPath();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return;
		for (const Run& r : gRuns)
		{
			for (size_t si = 0; si < r.steps.size(); ++si)
			{
				if (!r.steps[si].done) continue;
				char line[64];
				const int n = std::snprintf(line, sizeof(line), "R %d %zu\n", r.id, si);
				DWORD w = 0;
				if (n > 0) WriteFile(h, line, static_cast<DWORD>(n), &w, nullptr);
			}
		}
		for (const FishEntry& e : gFishLog)
		{
			char line[160];
			const int n = std::snprintf(line, sizeof(line), "F %d\t%s\t%s\n",
				e.count, e.name, e.map);
			DWORD w = 0;
			if (n > 0) WriteFile(h, line, static_cast<DWORD>(n), &w, nullptr);
		}
		CloseHandle(h);
	}
}
