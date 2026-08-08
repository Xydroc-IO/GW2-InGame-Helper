#include "FarmingShared.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "WaypointsData.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include <shlwapi.h>
#include <windows.h>

namespace FarmingDetail
{
	bool gFocus = false;
	bool gPlaceOnce = false;
	int  gDeferHeavy = 0;
	int gTab = 0;
	int gSelectedRun = 0;
	int gFocusStep = -1;
	bool gAutoArrive = true;
	float gArriveRadius = 120.f;
	RunTag gFilterTag = RunTag::All;
	bool gFavoritesOnly = false;
	char gStatus[160]{};
	char gFilter[64]{};
	char gFishName[64]{};
	char gFishMap[64]{};
	char gNewRunName[96]{};
	char gNewStepText[120]{};
	std::vector<Run> gRuns;
	std::vector<FishEntry> gFishLog;

	namespace
	{
		constexpr int kCustomIdBase = 10000;

		std::wstring PersistPath()
		{
			return AddonPaths::ConfigDir() + L"\\farming-state.txt";
		}

		Run* FindById(int id)
		{
			for (Run& r : gRuns)
				if (r.id == id) return &r;
			return nullptr;
		}

		int NextCustomId()
		{
			int maxId = kCustomIdBase;
			for (const Run& r : gRuns)
				if (r.id >= maxId) maxId = r.id + 1;
			return maxId < kCustomIdBase ? kCustomIdBase : maxId;
		}

		void StripTabs(char* s)
		{
			if (!s) return;
			for (; *s; ++s)
				if (*s == '\t' || *s == '\r' || *s == '\n') *s = ' ';
		}
	}

	bool RunMatchesFilter(const Run& r)
	{
		if (gFavoritesOnly && !r.favorite) return false;
		if (gFilterTag == RunTag::Custom)
		{
			if (!r.custom) return false;
		}
		else if (gFilterTag != RunTag::All && r.tag != gFilterTag)
			return false;
		if (!gFilter[0]) return true;
		char hay[160]{};
		std::snprintf(hay, sizeof(hay), "%s %s %s", r.name, r.blurb, TagLabel(r.tag));
		return StrStrIA(hay, gFilter) != nullptr;
	}

	void RunProgress(const Run& r, int& done, int& total)
	{
		done = 0;
		total = static_cast<int>(r.steps.size());
		for (const RunStep& s : r.steps)
			if (s.done) ++done;
	}

	int NextUndoneStep(size_t run)
	{
		if (run >= gRuns.size()) return -1;
		const Run& r = gRuns[run];
		for (size_t i = 0; i < r.steps.size(); ++i)
			if (!r.steps[i].done) return static_cast<int>(i);
		return -1;
	}

	void ToggleStep(size_t run, size_t step)
	{
		EnsureCatalog();
		if (run >= gRuns.size() || step >= gRuns[run].steps.size()) return;
		gRuns[run].steps[step].done = !gRuns[run].steps[step].done;
		if (gFocusStep == static_cast<int>(step) && gRuns[run].steps[step].done)
			gFocusStep = NextUndoneStep(run);
		Save();
	}

	void ResetRun(size_t run)
	{
		EnsureCatalog();
		if (run >= gRuns.size()) return;
		for (RunStep& s : gRuns[run].steps) s.done = false;
		gFocusStep = gRuns[run].steps.empty() ? -1 : 0;
		Save();
	}

	void ToggleFavorite(size_t run)
	{
		EnsureCatalog();
		if (run >= gRuns.size()) return;
		gRuns[run].favorite = !gRuns[run].favorite;
		Save();
	}

	bool AddCustomRun(const char* name, RunTag tag, int mapId)
	{
		EnsureCatalog();
		if (!name || !name[0]) return false;
		Run r{};
		r.id = NextCustomId();
		r.mapId = mapId;
		r.tag = (tag == RunTag::All) ? RunTag::Custom : tag;
		r.custom = true;
		std::snprintf(r.name, sizeof(r.name), "%s", name);
		std::snprintf(r.blurb, sizeof(r.blurb), "Custom run");
		std::snprintf(r.pathingHint, sizeof(r.pathingHint), "tw_guides.tw_gatheringnodes");
		gRuns.push_back(r);
		gSelectedRun = static_cast<int>(gRuns.size() - 1);
		gFocusStep = -1;
		std::snprintf(gStatus, sizeof(gStatus), "Added custom run \"%s\".", name);
		Save();
		return true;
	}

	bool AddCustomStep(size_t run, const char* text)
	{
		EnsureCatalog();
		if (run >= gRuns.size() || !gRuns[run].custom) return false;
		if (!text || !text[0]) return false;
		RunStep s{};
		std::snprintf(s.text, sizeof(s.text), "%s", text);
		gRuns[run].steps.push_back(s);
		Save();
		return true;
	}

	bool DeleteCustomRun(size_t run)
	{
		EnsureCatalog();
		if (run >= gRuns.size() || !gRuns[run].custom) return false;
		const char* name = gRuns[run].name;
		char buf[96];
		std::snprintf(buf, sizeof(buf), "%s", name);
		gRuns.erase(gRuns.begin() + static_cast<std::ptrdiff_t>(run));
		if (gSelectedRun >= static_cast<int>(gRuns.size()))
			gSelectedRun = static_cast<int>(gRuns.size()) - 1;
		if (gSelectedRun < 0) gSelectedRun = 0;
		gFocusStep = -1;
		std::snprintf(gStatus, sizeof(gStatus), "Deleted custom run \"%s\".", buf);
		Save();
		return true;
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

	int FishTotalCount()
	{
		int n = 0;
		for (const FishEntry& e : gFishLog) n += e.count;
		return n;
	}

	bool FillFishMapFromMumble()
	{
		const int mapId = WaypointsData::CurrentMapId();
		if (mapId <= 0) return false;
		WaypointsData::EnsureLoaded(false);
		WaypointsData::Tick();
		std::vector<WaypointsData::MapRow> maps;
		WaypointsData::ListMaps("", maps, 400);
		for (const auto& m : maps)
		{
			if (m.id == mapId && !m.name.empty())
			{
				std::snprintf(gFishMap, sizeof(gFishMap), "%s", m.name.c_str());
				return true;
			}
		}
		std::snprintf(gFishMap, sizeof(gFishMap), "map %d", mapId);
		return true;
	}

	bool StartRunPathing(size_t run)
	{
		EnsureCatalog();
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
		EnsureCatalog();
		const std::wstring path = PersistPath();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 1024 * 1024)
		{ CloseHandle(h); return; }
		std::string raw(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD rd = 0;
		if (!ReadFile(h, raw.data(), static_cast<DWORD>(raw.size()), &rd, nullptr))
		{ CloseHandle(h); return; }
		CloseHandle(h);
		raw.resize(rd);

		/* Drop previous custom runs before reloading defs. */
		gRuns.erase(std::remove_if(gRuns.begin(), gRuns.end(),
			[](const Run& r) { return r.custom; }), gRuns.end());
		for (Run& r : gRuns)
		{
			r.favorite = false;
			for (RunStep& s : r.steps) s.done = false;
		}
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
					if (Run* r = FindById(rid))
						if (step >= 0 && static_cast<size_t>(step) < r->steps.size())
							r->steps[static_cast<size_t>(step)].done = true;
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
			else if (line.rfind("K ", 0) == 0)
			{
				int rid = 0;
				if (std::sscanf(line.c_str() + 2, "%d", &rid) == 1)
					if (Run* r = FindById(rid)) r->favorite = true;
			}
			else if (line.rfind("A ", 0) == 0)
			{
				int on = 1;
				float rad = 120.f;
				if (std::sscanf(line.c_str() + 2, "%d %f", &on, &rad) >= 1)
				{
					gAutoArrive = on != 0;
					if (rad >= 40.f && rad <= 400.f) gArriveRadius = rad;
				}
			}
			else if (line.rfind("C ", 0) == 0)
			{
				int rid = 0, mapId = 0, tag = 0;
				char hint[96]{}, name[96]{};
				if (std::sscanf(line.c_str() + 2, "%d\t%d\t%d\t%95[^\t]\t%95[^\n]",
					&rid, &mapId, &tag, hint, name) >= 5 && rid >= kCustomIdBase)
				{
					if (FindById(rid)) continue;
					Run r{};
					r.id = rid;
					r.mapId = mapId;
					r.tag = (tag >= 0 && tag < static_cast<int>(RunTag::Count))
						? static_cast<RunTag>(tag) : RunTag::Custom;
					r.custom = true;
					std::snprintf(r.pathingHint, sizeof(r.pathingHint), "%s", hint);
					std::snprintf(r.name, sizeof(r.name), "%s", name);
					std::snprintf(r.blurb, sizeof(r.blurb), "Custom run");
					gRuns.push_back(r);
				}
			}
			else if (line.rfind("S ", 0) == 0)
			{
				int rid = 0, step = 0;
				float cx = 0.f, cy = 0.f;
				char text[120]{};
				const char* p = line.c_str() + 2;
				int n = 0;
				if (std::sscanf(p, "%d\t%d\t%f\t%f\t%119[^\n]%n",
					&rid, &step, &cx, &cy, text, &n) >= 5)
				{
					if (Run* r = FindById(rid))
					{
						if (static_cast<size_t>(step) >= r->steps.size())
							r->steps.resize(static_cast<size_t>(step) + 1);
						RunStep& st = r->steps[static_cast<size_t>(step)];
						std::snprintf(st.text, sizeof(st.text), "%s", text);
						st.hasCoord = true;
						st.continentX = cx;
						st.continentY = cy;
					}
				}
				else if (std::sscanf(p, "%d\t%d\t%119[^\n]", &rid, &step, text) >= 3)
				{
					if (Run* r = FindById(rid))
					{
						if (static_cast<size_t>(step) >= r->steps.size())
							r->steps.resize(static_cast<size_t>(step) + 1);
						RunStep& st = r->steps[static_cast<size_t>(step)];
						std::snprintf(st.text, sizeof(st.text), "%s", text);
					}
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

		auto WriteLine = [&](const char* line, int n) {
			DWORD w = 0;
			if (n > 0) WriteFile(h, line, static_cast<DWORD>(n), &w, nullptr);
		};

		{
			char line[64];
			const int n = std::snprintf(line, sizeof(line), "A %d %.0f\n",
				gAutoArrive ? 1 : 0, gArriveRadius);
			WriteLine(line, n);
		}

		for (const Run& r : gRuns)
		{
			if (r.favorite)
			{
				char line[48];
				const int n = std::snprintf(line, sizeof(line), "K %d\n", r.id);
				WriteLine(line, n);
			}
			if (r.custom)
			{
				char name[96], hint[96];
				std::snprintf(name, sizeof(name), "%s", r.name);
				std::snprintf(hint, sizeof(hint), "%s", r.pathingHint);
				StripTabs(name);
				StripTabs(hint);
				char line[320];
				const int n = std::snprintf(line, sizeof(line), "C %d\t%d\t%d\t%s\t%s\n",
					r.id, r.mapId, static_cast<int>(r.tag), hint, name);
				WriteLine(line, n);
				for (size_t si = 0; si < r.steps.size(); ++si)
				{
					char text[120];
					std::snprintf(text, sizeof(text), "%s", r.steps[si].text);
					StripTabs(text);
					if (r.steps[si].hasCoord)
					{
						const int n2 = std::snprintf(line, sizeof(line),
							"S %d\t%zu\t%.1f\t%.1f\t%s\n",
							r.id, si, r.steps[si].continentX, r.steps[si].continentY, text);
						WriteLine(line, n2);
					}
					else
					{
						const int n2 = std::snprintf(line, sizeof(line), "S %d\t%zu\t%s\n",
							r.id, si, text);
						WriteLine(line, n2);
					}
				}
			}
			for (size_t si = 0; si < r.steps.size(); ++si)
			{
				if (!r.steps[si].done) continue;
				char line[64];
				const int n = std::snprintf(line, sizeof(line), "R %d %zu\n", r.id, si);
				WriteLine(line, n);
			}
		}
		for (const FishEntry& e : gFishLog)
		{
			char line[160];
			const int n = std::snprintf(line, sizeof(line), "F %d\t%s\t%s\n",
				e.count, e.name, e.map);
			WriteLine(line, n);
		}
		CloseHandle(h);
	}
}
