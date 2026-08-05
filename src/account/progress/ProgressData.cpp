#include "ProgressData.h"

#include "ProgressDataInternal.h"

#include "BrowserTabs.h"
#include "CraftingData.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace ProgressDetail
{
	std::mutex gMu;
	Snapshot gSnap;
	Snapshot gDraw;
	std::atomic<unsigned> gGen{0};
	unsigned gDrawnGen = 0;
	std::atomic<bool> gBusy{false};
	HANDLE gThread = nullptr;
	char gFilter[96] = {};
	int gShowMode = 0; /* 0 all, 1 missing, 2 unlocked */

	size_t JsonObjectEnd(const std::string& json, size_t openBrace)
	{
		if (openBrace >= json.size() || json[openBrace] != '{')
			return std::string::npos;
		int depth = 0;
		bool inStr = false, esc = false;
		for (size_t i = openBrace; i < json.size(); ++i)
		{
			char c = json[i];
			if (inStr)
			{
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"') inStr = true;
			else if (c == '{') ++depth;
			else if (c == '}')
			{
				--depth;
				if (depth == 0) return i;
			}
		}
		return std::string::npos;
	}

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return {};
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		if (k >= json.size() || json[k] != '"') return {};
		++k;
		std::string out;
		while (k < json.size())
		{
			char c = json[k++];
			if (c == '\\' && k < json.size())
			{
				char e = json[k++];
				if (e == 'n') out.push_back('\n');
				else if (e == 't') out.push_back('\t');
				else if (e == 'u' && k + 3 < json.size()) k += 4;
				else out.push_back(e);
				continue;
			}
			if (c == '"') break;
			out.push_back(c);
		}
		return out;
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return -1;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return -1;
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		bool neg = false;
		if (k < json.size() && json[k] == '-') { neg = true; ++k; }
		long long v = 0;
		bool any = false;
		while (k < json.size() && json[k] >= '0' && json[k] <= '9')
		{
			any = true;
			v = v * 10 + (json[k] - '0');
			++k;
		}
		if (!any) return -1;
		return neg ? -v : v;
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok) return {};
		out.resize(read);
		return out;
	}

	void WriteUtf8File(const std::wstring& path, const std::string& body)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return;
		DWORD written = 0;
		WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, nullptr);
		CloseHandle(h);
	}

	bool FileFresh(const std::wstring& path, DWORD ttlMs)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
			return false;
		FILETIME nowFt{};
		GetSystemTimeAsFileTime(&nowFt);
		ULARGE_INTEGER now{}, then{};
		now.LowPart = nowFt.dwLowDateTime;
		now.HighPart = nowFt.dwHighDateTime;
		then.LowPart = fad.ftLastWriteTime.dwLowDateTime;
		then.HighPart = fad.ftLastWriteTime.dwHighDateTime;
		if (now.QuadPart < then.QuadPart) return true;
		const ULONGLONG age100ns = now.QuadPart - then.QuadPart;
		return age100ns <= (static_cast<ULONGLONG>(ttlMs) * 10000ULL);
	}

	std::string UrlEncodePathSegment(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() * 3);
		for (unsigned char c : s)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~')
				o.push_back(static_cast<char>(c));
			else if (c == ' ')
				o += "%20";
			else
			{
				char buf[8];
				std::snprintf(buf, sizeof(buf), "%%%02X", c);
				o += buf;
			}
		}
		return o;
	}

	void ParseArmoryCatalog(const std::string& body, std::vector<LegRow>& rows)
	{
		rows.clear();
		size_t p = 0;
		while (p < body.size() && rows.size() < 400)
		{
			size_t brace = body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(body, "id", brace);
			long long mx = JsonIntAfterKey(body, "max_count", brace);
			if (id > 0)
			{
				LegRow r;
				r.id = static_cast<int>(id);
				r.maxCount = mx > 0 ? static_cast<int>(mx) : 1;
				rows.push_back(r);
			}
			p = end + 1;
		}
	}

	void ApplyNames(const std::string& json, std::vector<LegRow>& rows)
	{
		size_t p = 0;
		while (p < json.size())
		{
			size_t brace = json.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(json, brace);
			if (end == std::string::npos) break;
			long long id = JsonIntAfterKey(json, "id", brace);
			std::string name = JsonStringAfterKey(json, "name", brace);
			if (id > 0 && !name.empty())
			{
				for (LegRow& r : rows)
				{
					if (r.id == static_cast<int>(id))
					{
						r.name = name;
						break;
					}
				}
			}
			p = end + 1;
		}
	}

	void FetchNames(std::vector<LegRow>& rows)
	{
		std::vector<int> need;
		for (const LegRow& r : rows)
			if (r.name.empty()) need.push_back(r.id);
		for (size_t off = 0; off < need.size(); off += 200)
		{
			const size_t n = (std::min)(need.size() - off, size_t{200});
			std::string path = "/v2/items?ids=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) path += ',';
				path += std::to_string(need[off + i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kBulkTimeoutMs);
			if (r.ok) ApplyNames(r.body, rows);
		}
	}

	/* MediaWiki title path — same rules as LookupPad (spaces → _, ' → %27). */
	std::string WikiTitleToPath(const std::string& title)
	{
		std::string o;
		o.reserve(title.size() + 8);
		for (char c : title)
		{
			if (c == ' ') o.push_back('_');
			else if (c == '\'') o += "%27";
			else o.push_back(c);
		}
		return o;
	}

	void OpenWikiItem(int id, const std::string& name)
	{
		/* Wiki Special:Search does not resolve GW2 item IDs — use the API name. */
		std::string url;
		if (!name.empty())
		{
			url = "https://wiki.guildwars2.com/wiki/";
			url += WikiTitleToPath(name);
		}
		else
		{
			url = "https://wiki.guildwars2.com/wiki/Special:Search?search=";
			url += WikiBrowser::UrlEncode(std::to_string(id));
		}
		G::ShowWiki = true;
		Settings::SetDirty();
		if (BrowserTabs::OpenNewUrl("wiki", url.c_str()) < 0)
			WikiBrowser::Navigate(url);
	}

	bool FilterMatch(const LegRow& r, const char* filter)
	{
		if (!filter || !filter[0]) return true;
		std::string hay = r.name;
		if (hay.empty())
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%d", r.id);
			hay = buf;
		}
		std::string needle = filter;
		for (char& c : hay) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		for (char& c : needle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return hay.find(needle) != std::string::npos;
	}

	void SyncDraw()
	{
		const unsigned gen = gGen.load();
		if (gen == gDrawnGen) return;
		std::lock_guard<std::mutex> lock(gMu);
		gDraw = gSnap;
		gDrawnGen = gen;
	}
} // namespace ProgressDetail

using namespace ProgressDetail;

void ProgressData::RefreshIfNeeded(bool force)
{
	StartFetch(force);
}

void ProgressData::RenderContents()
{
	SyncDraw();
	const Snapshot& snap = gDraw;

	ImGui::TextUnformatted("Legendaries & characters");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Legendary Armory unlocks and roster — official API, read-only. "
		"Use Plan on a legendary to open Crafting with its gift / forge tree.");
	ImGui::PopTextWrapPos();

	if (ImGui::Button("Refresh###gw2igh_prog_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (gBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Updating…");
	else if (!snap.status.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", snap.status.c_str());

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_prog_filter", "Filter legendaries…",
		gFilter, sizeof(gFilter));

	ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Show");
	ImGui::RadioButton("All###gw2igh_prog_m0", &gShowMode, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Missing###gw2igh_prog_m1", &gShowMode, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Unlocked###gw2igh_prog_m2", &gShowMode, 2);

	ImGui::Separator();

	const float listH = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("###gw2igh_prog_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);

	ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.f), "Legendary Armory");
	if (snap.legs.empty() && !gBusy)
	{
		ImGui::TextWrapped("No catalog yet — click Refresh.");
	}
	else
	{
		int shown = 0;
		for (const LegRow& r : snap.legs)
		{
			if (!FilterMatch(r, gFilter)) continue;
			const bool have = r.owned > 0;
			if (gShowMode == 1 && have) continue;
			if (gShowMode == 2 && !have) continue;
			++shown;
			ImGui::PushID(r.id);
			const char* name = r.name.empty() ? "…" : r.name.c_str();
			if (have)
				ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.f), "%s", name);
			else
				ImGui::TextUnformatted(name);
			ImGui::SameLine();
			if (r.owned >= 0)
			{
				ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
					"%d/%d", r.owned, r.maxCount);
			}
			else
				ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "#%d", r.id);
			ImGui::SameLine();
			if (ImGui::SmallButton("Plan"))
			{
				/* Prefer item ID — skips wiki name search (snappy). */
				char idBuf[24];
				std::snprintf(idBuf, sizeof(idBuf), "%d", r.id);
				CraftingData::QueuePlan(idBuf);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Wiki"))
				OpenWikiItem(r.id, r.name);
			ImGui::PopID();
		}
		if (shown == 0)
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "No matches.");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.f), "Characters");
	if (!snap.hasKey)
	{
		ImGui::TextWrapped("Add an API key with the characters scope to list your roster.");
	}
	else if (snap.chars.empty())
	{
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
			snap.scopeFail ? "Check API scopes." : "No characters loaded yet.");
	}
	else
	{
		const size_t n = (std::min)(snap.chars.size(), kMaxCharDetails);
		for (size_t i = 0; i < n; ++i)
		{
			const CharRow& c = snap.chars[i];
			ImGui::TextUnformatted(c.name.c_str());
			ImGui::SameLine();
			if (c.level >= 0 || !c.profession.empty())
			{
				char meta[96];
				if (c.level >= 0 && !c.profession.empty())
					std::snprintf(meta, sizeof(meta), "Lv %lld · %s", c.level, c.profession.c_str());
				else if (c.level >= 0)
					std::snprintf(meta, sizeof(meta), "Lv %lld", c.level);
				else
					std::snprintf(meta, sizeof(meta), "%s", c.profession.c_str());
				ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "%s", meta);
			}
		}
		if (snap.chars.size() > n)
		{
			ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
				"Showing %d of %d", static_cast<int>(n), static_cast<int>(snap.chars.size()));
		}
	}

	ImGui::EndChild();
}
