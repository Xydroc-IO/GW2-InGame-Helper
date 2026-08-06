#include "SessionHistoryData.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "InventoryData.h"
#include "MumbleIdentity.h"
#include "UnlocksData.h"

#include "imgui/imgui.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kVersion = 1;
	constexpr size_t kMaxEntries = 80;
	constexpr DWORD kSnapshotMs = 60u * 1000u;

	struct Entry
	{
		unsigned atMs = 0;
		std::string character;
		size_t skins = 0;
		size_t dyes = 0;
		size_t minis = 0;
		size_t unlockTotal = 0;
		size_t invUnique = 0;
		size_t invStacks = 0;
	};

	std::mutex gMu;
	std::vector<Entry> gEntries;
	bool gDirty = false;
	DWORD gLastSnap = 0;
	DWORD gSessionStart = 0;
	Entry gSessionBaseline{};
	bool gHaveBaseline = false;

	std::wstring PathW()
	{
		return AddonPaths::ConfigDir() + L"\\session_history.json";
	}

	bool WriteUtf8File(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		CloseHandle(h);
		return ok && written == data.size();
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 2 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok || read != out.size())
			return {};
		return out;
	}

	std::string EscapeJson(const std::string& s)
	{
		std::string o;
		for (char c : s)
		{
			if (c == '\\' || c == '"') { o.push_back('\\'); o.push_back(c); }
			else if (static_cast<unsigned char>(c) >= 0x20) o.push_back(c);
		}
		return o;
	}

	Entry CaptureNow()
	{
		Entry e;
		e.atMs = GetTickCount();
		e.character = MumbleIdentity::CharacterNameStr();
		e.skins = UnlocksData::Count(UnlocksData::Kind::Skins);
		e.dyes = UnlocksData::Count(UnlocksData::Kind::Dyes);
		e.minis = UnlocksData::Count(UnlocksData::Kind::Minis);
		e.unlockTotal = 0;
		for (int i = 0; i < static_cast<int>(UnlocksData::Kind::Count); ++i)
			e.unlockTotal += UnlocksData::Count(static_cast<UnlocksData::Kind>(i));
		e.invUnique = InventoryData::UniqueItemCount();
		e.invStacks = InventoryData::TotalStackCount();
		return e;
	}

	std::string SerializeLocked()
	{
		std::string out = "{\n  \"version\": ";
		out += std::to_string(kVersion);
		out += ",\n  \"entries\": [\n";
		for (size_t i = 0; i < gEntries.size(); ++i)
		{
			const Entry& e = gEntries[i];
			if (i) out += ",\n";
			char buf[256];
			std::snprintf(buf, sizeof(buf),
				"    { \"at\": %u, \"character\": \"%s\", \"skins\": %zu, \"dyes\": %zu, "
				"\"minis\": %zu, \"unlocks\": %zu, \"inv_unique\": %zu, \"inv_stacks\": %zu }",
				e.atMs, EscapeJson(e.character).c_str(), e.skins, e.dyes, e.minis,
				e.unlockTotal, e.invUnique, e.invStacks);
			out += buf;
		}
		out += "\n  ]\n}\n";
		return out;
	}

	unsigned JsonUIntNear(const std::string& json, size_t from, size_t limit, const char* key)
	{
		const std::string pat = std::string("\"") + key + "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos || k >= limit) return 0;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos || k >= limit) return 0;
		++k;
		while (k < limit && (json[k] == ' ' || json[k] == '\t')) ++k;
		return static_cast<unsigned>(std::strtoul(json.c_str() + k, nullptr, 10));
	}

	std::string JsonStringNear(const std::string& json, size_t from, size_t limit, const char* key)
	{
		const std::string pat = std::string("\"") + key + "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos || k >= limit) return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos || k >= limit) return {};
		++k;
		while (k < limit && (json[k] == ' ' || json[k] == '\t')) ++k;
		if (k >= limit || json[k] != '"') return {};
		++k;
		std::string out;
		while (k < limit)
		{
			char c = json[k++];
			if (c == '\\' && k < limit) { out.push_back(json[k++]); continue; }
			if (c == '"') break;
			out.push_back(c);
		}
		return out;
	}

	void Parse(const std::string& raw)
	{
		gEntries.clear();
		size_t arr = raw.find('[');
		if (arr == std::string::npos) return;
		size_t i = arr + 1;
		while (i < raw.size())
		{
			size_t brace = raw.find('{', i);
			if (brace == std::string::npos) break;
			int depth = 0;
			size_t end = brace;
			bool inStr = false, esc = false;
			for (; end < raw.size(); ++end)
			{
				char c = raw[end];
				if (inStr)
				{
					if (esc) esc = false;
					else if (c == '\\') esc = true;
					else if (c == '"') inStr = false;
					continue;
				}
				if (c == '"') { inStr = true; continue; }
				if (c == '{') ++depth;
				else if (c == '}')
				{
					--depth;
					if (depth == 0) { ++end; break; }
				}
			}
			Entry e;
			e.atMs = JsonUIntNear(raw, brace, end, "at");
			e.character = JsonStringNear(raw, brace, end, "character");
			e.skins = JsonUIntNear(raw, brace, end, "skins");
			e.dyes = JsonUIntNear(raw, brace, end, "dyes");
			e.minis = JsonUIntNear(raw, brace, end, "minis");
			e.unlockTotal = JsonUIntNear(raw, brace, end, "unlocks");
			e.invUnique = JsonUIntNear(raw, brace, end, "inv_unique");
			e.invStacks = JsonUIntNear(raw, brace, end, "inv_stacks");
			gEntries.push_back(std::move(e));
			i = end;
			if (gEntries.size() >= kMaxEntries)
				break;
		}
	}
}

void SessionHistoryData::Load()
{
	std::lock_guard<std::mutex> lock(gMu);
	gEntries.clear();
	gDirty = false;
	gSessionStart = GetTickCount();
	gHaveBaseline = false;
	const std::string raw = ReadUtf8File(PathW());
	if (!raw.empty())
		Parse(raw);
}

void SessionHistoryData::Save(bool force)
{
	std::string payload;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!force && !gDirty)
			return;
		payload = SerializeLocked();
		gDirty = false;
	}
	AddonPaths::ConfigDir();
	WriteUtf8File(PathW(), payload);
}

void SessionHistoryData::Tick()
{
	UnlocksData::Tick();
	InventoryData::Tick();

	const DWORD now = GetTickCount();
	if (gLastSnap != 0 && (now - gLastSnap) < kSnapshotMs)
		return;
	gLastSnap = now;

	/* Need at least skins loaded or inventory ready to be meaningful. */
	if (!UnlocksData::Ready(UnlocksData::Kind::Skins) && !InventoryData::Ready())
		return;

	Entry cur = CaptureNow();
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!gHaveBaseline)
		{
			gSessionBaseline = cur;
			gHaveBaseline = true;
		}
		bool changed = gEntries.empty();
		if (!gEntries.empty())
		{
			const Entry& last = gEntries.back();
			changed = last.unlockTotal != cur.unlockTotal ||
				last.invUnique != cur.invUnique ||
				last.character != cur.character;
		}
		if (changed)
		{
			gEntries.push_back(std::move(cur));
			while (gEntries.size() > kMaxEntries)
				gEntries.erase(gEntries.begin());
			gDirty = true;
		}
	}
	Save(false);
}

void SessionHistoryData::RenderOverviewSnippet()
{
	Tick();
	Entry baseline{};
	Entry latest{};
	bool have = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		have = gHaveBaseline;
		baseline = gSessionBaseline;
		if (!gEntries.empty())
			latest = gEntries.back();
		else
			latest = baseline;
	}
	if (!have)
		return;

	const long long dUnlock = static_cast<long long>(latest.unlockTotal) -
		static_cast<long long>(baseline.unlockTotal);
	const long long dInv = static_cast<long long>(latest.invUnique) -
		static_cast<long long>(baseline.invUnique);
	ImGui::Spacing();
	ImGui::TextColored(HelperTheme::GoldMuted, "This session");
	ImGui::TextDisabled("Unlocks %+lld | Unique items %+lld",
		dUnlock, dInv);
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted, "(local snapshots)");
	PadNav::PopWrap();
}

void SessionHistoryData::RenderContents()
{
	Tick();
	ImGui::TextColored(HelperTheme::Gold, "SESSION HISTORY");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Local snapshots of unlock totals and inventory size. "
		"Guild Wars 2 has no official session-history API - this is Helper-only.");
	PadNav::PopWrap();
	ImGui::Spacing();

	if (ImGui::Button("Snapshot now###gw2igh_hist_snap"))
	{
		gLastSnap = 0;
		Tick();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", InventoryData::Status());

	RenderOverviewSnippet();
	ImGui::Separator();

	std::vector<Entry> copy;
	{
		std::lock_guard<std::mutex> lock(gMu);
		copy = gEntries;
	}
	ImGui::BeginChild("###gw2igh_hist_list", ImVec2(0.f, 0.f), true);
	if (copy.empty())
		ImGui::TextDisabled("No snapshots yet - open Unlocks / Stash with an API key.");
	else
	{
		for (size_t i = copy.size(); i-- > 0; )
		{
			const Entry& e = copy[i];
			ImGui::Text("%s", e.character.empty() ? "(no character)" : e.character.c_str());
			ImGui::TextDisabled(
				"unlocks %zu (skins %zu | dyes %zu | minis %zu) | items %zu unique / %zu stacks",
				e.unlockTotal, e.skins, e.dyes, e.minis, e.invUnique, e.invStacks);
			ImGui::Separator();
		}
	}
	ImGui::EndChild();
}
