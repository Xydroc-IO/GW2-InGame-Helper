#include "InstancesShared.h"
#include "AddonPaths.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace InstancesDetail
{
	bool gFocus = false;
	bool gPlaceOnce = false;
	Kind gKind = Kind::Raid;
	int gSelected = -1;
	char gStatus[160] = {};
	static std::vector<Entry> gEntries;
	static bool gReady = false;

	static void Add(int id, Kind k, const char* name, const char* blurb,
		std::initializer_list<const char*> steps)
	{
		Entry e{};
		e.id = id;
		e.kind = k;
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

	void EnsureCatalog()
	{
		if (gReady) return;
		Add(1, Kind::Raid, "Spirit Vale (W1)", "Weekly raid wing",
			{"Vale Guardian", "Spirit Woods", "Gorseval", "Sabetha"});
		Add(2, Kind::Raid, "Salvation Pass (W2)", "Weekly raid wing",
			{"Slothasor", "Bandit Trio", "Matthias"});
		Add(3, Kind::Raid, "Stronghold of the Faithful (W3)", "Weekly raid wing",
			{"Escort", "Keep Construct", "Xera"});
		Add(4, Kind::Raid, "Bastion of the Penitent (W4)", "Weekly raid wing",
			{"Cairn", "Mursaat Overseer", "Samarog", "Deimos"});
		Add(5, Kind::Raid, "Hall of Chains (W5)", "Weekly raid wing",
			{"Soulless Horror", "River of Souls", "Statues of Grenth", "Dhuum"});
		Add(6, Kind::Raid, "Mythwright Gambit (W6)", "Weekly raid wing",
			{"Conjured Amalgamate", "Twin Largos", "Qadim"});
		Add(7, Kind::Raid, "The Key of Ahdashim (W7)", "Weekly raid wing",
			{"Cardinal Adina", "Cardinal Sabir", "Qadim the Peerless"});
		Add(10, Kind::Fractal, "Nightmare", "CM / challenge",
			{"MAMA", "Siax", "Ensolyss"});
		Add(11, Kind::Fractal, "Shattered Observatory", "CM / challenge",
			{"Skorvald", "Artsariiv", "Arkk"});
		Add(12, Kind::Fractal, "Sunqua Peak", "CM / challenge",
			{"Ai - elemental", "Ai - dark"});
		Add(13, Kind::Fractal, "Silent Surf", "CM / challenge",
			{"Kanaxai"});
		Add(14, Kind::Fractal, "Lonely Tower", "CM / challenge",
			{"Eparch"});
		Add(15, Kind::Fractal, "Kinfall", "CM / challenge",
			{"Whispering Shadow"});
		Add(20, Kind::Strike, "Icebrood Saga strikes", "Weekly strikes",
			{"Fraenir of Jormag", "Voice and Claw", "Boneskinner", "Whisper of Jormag",
				"Cold War"});
		Add(21, Kind::Strike, "End of Dragons strikes", "Weekly strikes",
			{"Aetherblade Hideout", "Xunlai Jade Junkyard", "Kaineng Overlook",
				"Harvest Temple", "Old Lion's Court"});
		Add(22, Kind::Strike, "Secrets of the Obscure", "Weekly strikes",
			{"Cosmic Observatory", "Temple of Febe"});
		Add(23, Kind::Strike, "Janthir Wilds", "Weekly strikes",
			{"Mount Balrior - Greer", "Decima", "Ura"});
		Add(30, Kind::Story, "Personal Story", "Core chapters",
			{"Chapter 1-3", "Chapter 4-5", "Chapter 6-7", "Chapter 8"});
		Add(31, Kind::Story, "Living World Season 2", "Episode tracker",
			{"S2E1", "S2E2", "S2E3", "S2E4", "S2E5", "S2E6", "S2E7", "S2E8"});
		Add(32, Kind::Story, "Heart of Thorns", "Expansion story",
			{"Prologue", "Act 1", "Act 2", "Act 3", "Act 4"});
		Add(33, Kind::Story, "Path of Fire", "Expansion story",
			{"Act 1", "Act 2", "Act 3"});
		Add(34, Kind::Story, "End of Dragons", "Expansion story",
			{"Prologue", "Chapter 1-5", "Chapter 6-10", "Chapter 11-15", "Chapter 16-20"});
		Add(35, Kind::Story, "Secrets of the Obscure", "Expansion story",
			{"Prologue", "Through the Looking Glass", "The World Spire", "The Realm of Dreams"});
		gReady = true;
	}

	size_t Count() { EnsureCatalog(); return gEntries.size(); }
	Entry* At(size_t i) { EnsureCatalog(); return i < gEntries.size() ? &gEntries[i] : nullptr; }

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
		if (all)
			e->cleared = true;
		SaveProgress();
	}

	void ToggleCleared(size_t entry)
	{
		Entry* e = At(entry);
		if (!e) return;
		e->cleared = !e->cleared;
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
		/* Always reset first - Load only SETs from file, so stale in-memory ticks
		   would stick after an uncheck+reopen without this wipe. */
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
