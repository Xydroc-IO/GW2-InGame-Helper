#include "InstancesShared.h"
#include "AddonPaths.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace InstancesDetail
{
	bool gFocus = false;
	bool gPlaceOnce = false;
	int  gDeferHeavy = 0;
	Kind gKind = Kind::Raid;
	int gSelected = -1;
	char gStatus[160] = {};
	int gFractalLevel = 0;
	std::vector<std::string> gDailyFractals;
	static std::vector<Entry> gEntries;
	static bool gReady = false;

	using StoryList = std::initializer_list<int>;
	using StoryStep = std::pair<const char*, StoryList>;

	static void Add(int id, Kind k, const char* name, const char* blurb,
		std::initializer_list<const char*> steps, int achId = 0)
	{
		Entry e{};
		e.id = id;
		e.kind = k;
		e.achId = achId;
		std::snprintf(e.name, sizeof(e.name), "%s", name);
		std::snprintf(e.blurb, sizeof(e.blurb), "%s", blurb);
		for (const char* s : steps)
		{
			Step st{};
			std::snprintf(st.text, sizeof(st.text), "%s", s);
			e.steps.push_back(st);
		}
		gEntries.push_back(std::move(e));
	}

	static void AddRaid(int id, const char* name, const char* blurb,
		std::initializer_list<std::pair<const char*, const char*>> steps)
	{
		Entry e{};
		e.id = id;
		e.kind = Kind::Raid;
		std::snprintf(e.name, sizeof(e.name), "%s", name);
		std::snprintf(e.blurb, sizeof(e.blurb), "%s", blurb);
		for (const auto& p : steps)
		{
			Step st{};
			std::snprintf(st.text, sizeof(st.text), "%s", p.first);
			if (p.second && p.second[0])
				std::snprintf(st.apiId, sizeof(st.apiId), "%s", p.second);
			e.steps.push_back(st);
		}
		gEntries.push_back(std::move(e));
	}

	static void AddStory(int id, const char* name, const char* blurb,
		std::initializer_list<StoryStep> steps)
	{
		Entry e{};
		e.id = id;
		e.kind = Kind::Story;
		std::snprintf(e.name, sizeof(e.name), "%s", name);
		std::snprintf(e.blurb, sizeof(e.blurb), "%s", blurb);
		for (const auto& p : steps)
		{
			Step st{};
			std::snprintf(st.text, sizeof(st.text), "%s", p.first);
			for (int sid : p.second)
				if (sid > 0)
					st.storyIds.push_back(sid);
			e.steps.push_back(std::move(st));
		}
		gEntries.push_back(std::move(e));
	}

	void EnsureCatalog()
	{
		if (gReady) return;
		AddRaid(1, "Spirit Vale (W1)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Vale Guardian", "vale_guardian"},
			 {"Spirit Woods", "spirit_woods"},
			 {"Gorseval", "gorseval"},
			 {"Sabetha", "sabetha"}});
		AddRaid(2, "Salvation Pass (W2)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Slothasor", "slothasor"},
			 {"Bandit Trio", "bandit_trio"},
			 {"Matthias", "matthias"}});
		AddRaid(3, "Stronghold of the Faithful (W3)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Escort", "escort"},
			 {"Keep Construct", "keep_construct"},
			 {"Twisted Castle", "twisted_castle"},
			 {"Xera", "xera"}});
		AddRaid(4, "Bastion of the Penitent (W4)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Cairn", "cairn"},
			 {"Mursaat Overseer", "mursaat_overseer"},
			 {"Samarog", "samarog"},
			 {"Deimos", "deimos"}});
		AddRaid(5, "Hall of Chains (W5)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Soulless Horror", "soulless_horror"},
			 {"River of Souls", "river_of_souls"},
			 {"Statues of Grenth", "statues_of_grenth"},
			 {"Dhuum", "voice_in_the_void"}});
		AddRaid(6, "Mythwright Gambit (W6)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Conjured Amalgamate", "conjured_amalgamate"},
			 {"Twin Largos", "twin_largos"},
			 {"Qadim", "qadim"}});
		AddRaid(7, "The Key of Ahdashim (W7)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Gate", "gate"},
			 {"Cardinal Adina", "adina"},
			 {"Cardinal Sabir", "sabir"},
			 {"Qadim the Peerless", "qadim_the_peerless"}});
		AddRaid(8, "Mount Balrior (W8)", "Weekly raid wing — synced via /v2/account/raids",
			{{"Camp", "camp"},
			 {"Greer", "greer"},
			 {"Decima", "decima"},
			 {"Ura", "ura"}});

		/* Fractal CM lifetime overlay (entry achId); boss rows stay manual. */
		Add(10, Kind::Fractal, "Nightmare", "CM: Up to the Challenge (achievements)",
			{"MAMA", "Siax", "Ensolyss"}, 3180);
		Add(11, Kind::Fractal, "Shattered Observatory", "CM: Closing the Loop (achievements)",
			{"Skorvald", "Artsariiv", "Arkk"}, 3528);
		Add(12, Kind::Fractal, "Sunqua Peak", "CM: Another Side, Another Story (achievements)",
			{"Ai - elemental", "Ai - dark"}, 5451);
		Add(13, Kind::Fractal, "Silent Surf", "CM: Unsundered (achievements)",
			{"Kanaxai"}, 6940);
		Add(14, Kind::Fractal, "Lonely Tower", "CM: Wizard's Tower Is Ours (achievements)",
			{"Eparch"}, 8067);
		Add(15, Kind::Fractal, "Kinfall", "CM: Kinfall Challenge Mode (achievements)",
			{"Whispering Shadow"}, 8710);

		Add(20, Kind::Strike, "Icebrood Saga strikes", "Weekly EMS — local (no account strikes API)",
			{"Fraenir of Jormag", "Voice and Claw", "Boneskinner", "Whisper of Jormag",
				"Cold War"});
		Add(21, Kind::Strike, "End of Dragons strikes", "Weekly EMS — local (no account strikes API)",
			{"Aetherblade Hideout", "Xunlai Jade Junkyard", "Kaineng Overlook",
				"Harvest Temple", "Old Lion's Court"});
		Add(22, Kind::Strike, "Secrets of the Obscure", "Weekly EMS — local (no account strikes API)",
			{"Cosmic Observatory", "Temple of Febe"});
		Add(23, Kind::Strike, "Janthir Wilds", "Weekly EMS — local (no account strikes API)",
			{"Mount Balrior - Greer", "Decima", "Ura"});

		Add(30, Kind::Story, "Personal Story", "Core chapters — local (race branches)",
			{"Chapter 1-3", "Chapter 4-5", "Chapter 6-7", "Chapter 8"});
		AddStory(31, "Living World Season 2", "Episodes — character quest sync",
			{{"Gates of Maguuma", {11}},
			 {"Entanglement", {12}},
			 {"Dragon's Reach Pt 1", {13}},
			 {"Dragon's Reach Pt 2", {14}},
			 {"Echoes of the Past", {15}},
			 {"Tangled Paths", {16}},
			 {"Seeds of Truth", {17}},
			 {"Point of No Return", {18}}});
		AddStory(36, "Living World Season 3", "Episodes — character quest sync",
			{{"Out of the Shadows", {46}},
			 {"Rising Flames", {56}},
			 {"A Crack in the Ice", {63}},
			 {"Head of the Snake", {64}},
			 {"Flashpoint", {65}},
			 {"One Path Ends", {66}}});
		AddStory(37, "Living World Season 4", "Episodes — character quest sync",
			{{"Daybreak", {85}},
			 {"A Bug in the System", {86}},
			 {"Long Live the Lich", {87}},
			 {"A Star to Guide Us", {88}},
			 {"All or Nothing", {89}},
			 {"War Eternal", {90}}});
		AddStory(32, "Heart of Thorns", "Acts — character quest sync",
			{{"Prologue", {19}},
			 {"Act 1", {32, 41, 34, 26, 33}},
			 {"Act 2", {21, 20, 35, 31, 36}},
			 {"Act 3", {23, 22, 42, 27}},
			 {"Act 4", {25}}});
		AddStory(33, "Path of Fire", "Acts — character quest sync",
			{{"Act 1", {83, 67, 82, 72, 79, 69}},
			 {"Act 2", {80, 68, 71, 75, 76}},
			 {"Act 3", {81, 78}}});
		AddStory(38, "Icebrood Saga", "Episodes — character quest sync",
			{{"Bound by Blood", {91}},
			 {"Whisper in the Dark", {93}},
			 {"Shadow in the Ice", {94}},
			 {"Visions of the Past", {95}},
			 {"No Quarter", {96}},
			 {"Jormag Rising", {97}},
			 {"Champions", {98}}});
		AddStory(34, "End of Dragons", "Chapters — character quest sync",
			{{"Prologue", {112}},
			 {"Chapters 1-5", {101, 104, 114, 113, 110}},
			 {"Chapters 6-10", {111, 103, 106, 102, 99}},
			 {"Chapters 11-15", {109, 107, 105, 100, 108}},
			 {"Chapters 16-20", {121, 120, 122, 123}}});
		AddStory(35, "Secrets of the Obscure", "Acts — character quest sync",
			{{"Prologue", {130}},
			 {"Looking Glass", {131, 134, 129, 132, 127, 126, 125, 128}},
			 {"World Spire", {133}},
			 {"Realm of Dreams", {124, 138, 135, 136, 139, 141, 140, 143, 144, 142}}});
		gReady = true;
	}

	size_t Count() { EnsureCatalog(); return gEntries.size(); }
	Entry* At(size_t i) { EnsureCatalog(); return i < gEntries.size() ? &gEntries[i] : nullptr; }

	bool StepSynced(const Step& s)
	{
		return s.apiId[0] || s.achId > 0 || !s.storyIds.empty();
	}

	bool EntrySynced(const Entry& e)
	{
		if (e.achId > 0)
			return true;
		for (const auto& s : e.steps)
			if (StepSynced(s))
				return true;
		return false;
	}

	bool EntryHasApiSteps(const Entry& e)
	{
		return EntrySynced(e);
	}

	void ToggleStep(size_t entry, size_t step)
	{
		Entry* e = At(entry);
		if (!e || step >= e->steps.size()) return;
		e->steps[step].done = !e->steps[step].done;
		bool all = !e->steps.empty();
		for (const auto& s : e->steps)
		{
			if (!s.done) { all = false; break; }
		}
		e->cleared = all;
		SaveProgress();
	}

	void ToggleCleared(size_t entry)
	{
		Entry* e = At(entry);
		if (!e) return;
		e->cleared = !e->cleared;
		for (auto& s : e->steps)
			s.done = e->cleared;
		SaveProgress();
	}

	void ClearKind(Kind k)
	{
		EnsureCatalog();
		for (auto& e : gEntries)
		{
			if (e.kind != k) continue;
			e.cleared = false;
			for (auto& s : e.steps) s.done = false;
		}
		SaveProgress();
		std::snprintf(gStatus, sizeof(gStatus), "Cleared %s checklist.", KindName(k));
	}

	void ResetEntry(size_t entry)
	{
		Entry* e = At(entry);
		if (!e) return;
		e->cleared = false;
		for (auto& s : e->steps) s.done = false;
		SaveProgress();
		std::snprintf(gStatus, sizeof(gStatus), "Reset \"%s\".", e->name);
	}

	int CountCleared(Kind k)
	{
		EnsureCatalog();
		int n = 0;
		for (const auto& e : gEntries)
			if (e.kind == k && e.cleared) ++n;
		return n;
	}

	int CountEntries(Kind k)
	{
		EnsureCatalog();
		int n = 0;
		for (const auto& e : gEntries)
			if (e.kind == k) ++n;
		return n;
	}

	int CountStepsDone(size_t entry)
	{
		Entry* e = At(entry);
		if (!e) return 0;
		int n = 0;
		for (const auto& s : e->steps)
			if (s.done) ++n;
		return n;
	}

	void ApplyRaidEncounterIds(const std::vector<std::string>& ids)
	{
		EnsureCatalog();
		std::unordered_set<std::string> set(ids.begin(), ids.end());
		for (size_t i = 0; i < Count(); ++i)
		{
			Entry* e = At(i);
			if (!e || e->kind != Kind::Raid) continue;
			bool allMapped = !e->steps.empty();
			bool anyMapped = false;
			for (auto& s : e->steps)
			{
				if (!s.apiId[0])
				{
					allMapped = false;
					continue;
				}
				anyMapped = true;
				s.done = set.count(s.apiId) > 0;
				if (!s.done)
					allMapped = false;
			}
			if (anyMapped)
				e->cleared = allMapped && !e->steps.empty();
		}
		SaveProgress();
	}

	void ApplyAchievementIds(const std::vector<int>& doneIds)
	{
		EnsureCatalog();
		std::unordered_set<int> set(doneIds.begin(), doneIds.end());
		for (size_t i = 0; i < Count(); ++i)
		{
			Entry* e = At(i);
			if (!e) continue;
			if (e->achId > 0 && set.count(e->achId))
			{
				e->cleared = true;
				for (auto& s : e->steps)
					s.done = true;
			}
			for (auto& s : e->steps)
			{
				if (s.achId > 0)
					s.done = set.count(s.achId) > 0;
			}
			if (e->kind != Kind::Raid && e->achId <= 0)
			{
				bool any = false, all = !e->steps.empty();
				for (const auto& s : e->steps)
				{
					if (s.achId <= 0) { all = false; continue; }
					any = true;
					if (!s.done) all = false;
				}
				if (any)
					e->cleared = all;
			}
		}
		SaveProgress();
	}

	void ApplyStoryCompletions(const std::vector<int>& completeStoryIds)
	{
		EnsureCatalog();
		std::unordered_set<int> done(completeStoryIds.begin(), completeStoryIds.end());
		for (size_t i = 0; i < Count(); ++i)
		{
			Entry* e = At(i);
			if (!e || e->kind != Kind::Story) continue;
			bool any = false;
			bool all = !e->steps.empty();
			for (auto& s : e->steps)
			{
				if (s.storyIds.empty())
				{
					all = false;
					continue;
				}
				any = true;
				bool stepOk = true;
				for (int sid : s.storyIds)
				{
					if (!done.count(sid))
					{
						stepOk = false;
						break;
					}
				}
				s.done = stepOk;
				if (!stepOk)
					all = false;
			}
			if (any)
				e->cleared = all;
		}
		SaveProgress();
	}

	static std::wstring ProgPath()
	{
		std::wstring dir = AddonPaths::ConfigDir();
		if (dir.empty()) dir = AddonPaths::DataDir();
		if (dir.empty()) return {};
		if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
		dir += L"instances-progress.txt";
		return dir;
	}

	static bool WriteUtf8File(const std::wstring& path, const std::string& body)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, nullptr);
		CloseHandle(h);
		return ok != 0;
	}

	static bool ReadUtf8File(const std::wstring& path, std::string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 4 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		out.resize(static_cast<size_t>(sz.QuadPart));
		DWORD got = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok) { out.clear(); return false; }
		out.resize(got);
		return true;
	}

	void SaveProgress()
	{
		const std::wstring path = ProgPath();
		if (path.empty()) return;
		std::string body;
		for (const auto& e : gEntries)
		{
			body += "E " + std::to_string(e.id) + " " + (e.cleared ? "1" : "0") + "\n";
			for (size_t i = 0; i < e.steps.size(); ++i)
				if (e.steps[i].done)
					body += "S " + std::to_string(e.id) + " " + std::to_string(i) + "\n";
		}
		WriteUtf8File(path, body);
	}

	void LoadProgress()
	{
		EnsureCatalog();
		for (auto& en : gEntries)
		{
			en.cleared = false;
			for (auto& s : en.steps)
				s.done = false;
		}
		const std::wstring path = ProgPath();
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			int id = 0, a = 0, b = 0;
			if (line.rfind("E ", 0) == 0 && std::sscanf(line.c_str() + 2, "%d %d", &id, &a) == 2)
			{
				for (auto& en : gEntries)
					if (en.id == id) en.cleared = a != 0;
			}
			else if (line.rfind("S ", 0) == 0 && std::sscanf(line.c_str() + 2, "%d %d", &id, &b) == 2)
			{
				for (auto& en : gEntries)
					if (en.id == id && b >= 0 && b < static_cast<int>(en.steps.size()))
						en.steps[static_cast<size_t>(b)].done = true;
			}
		}
	}
}
