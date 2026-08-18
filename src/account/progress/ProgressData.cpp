#include "ProgressData.h"
#include "PadLayout.h"
#include "PadNav.h"

#include "ProgressDataInternal.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "HelperTheme.h"
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
#include <unordered_map>
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
	int gCatFilter = 0;
	int gGenFilter = 0;

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

	/* MediaWiki title path - same rules as LookupPad (spaces -> _, ' -> %27). */
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
		/* Wiki Special:Search does not resolve GW2 item IDs - use the API name. */
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
		hay.push_back(' ');
		hay += r.category;
		hay.push_back(' ');
		hay += r.generation;
		hay.push_back(' ');
		hay += r.itemType;
		if (hay.find_first_not_of(' ') == std::string::npos)
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

	bool RowVisible(const LegRow& r)
	{
		if (!FilterMatch(r, gFilter)) return false;
		const bool have = r.owned > 0;
		if (gShowMode == 1 && have) return false;
		if (gShowMode == 2 && !have) return false;
		if (gCatFilter > 0 && gCatFilter < kArmoryCatCount)
		{
			const char* want = kArmoryCats[gCatFilter];
			const char* haveCat = r.category.empty() ? "Other" : r.category.c_str();
			if (std::strcmp(haveCat, want) != 0) return false;
		}
		if (gGenFilter > 0 && gGenFilter < kArmoryGenCount)
		{
			const char* want = kArmoryGens[gGenFilter];
			const char* haveGen = r.generation.empty() ? "Other" : r.generation.c_str();
			if (std::strcmp(haveGen, want) != 0) return false;
		}
		return true;
	}

	/* Embedded compact legendaries catalog (same blob as the CEF Ledger). */
	extern "C" {
		extern const unsigned char _binary_legendaries_catalog_json_start[];
		extern const unsigned char _binary_legendaries_catalog_json_end[];
	}

	std::string JsonStringBounded(const std::string& json, const char* key,
		size_t from, size_t to)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos || k >= to) return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos || k >= to) return {};
		++k;
		while (k < to && (json[k] == ' ' || json[k] == '\t')) ++k;
		if (k >= to || json[k] != '"') return {};
		++k;
		std::string out;
		while (k < json.size() && k < to)
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

	void ParseItemIdsBounded(const std::string& json, size_t from, size_t to,
		std::vector<int>& ids)
	{
		size_t key = json.find("\"itemIds\"", from);
		if (key == std::string::npos || key >= to) return;
		size_t br = json.find('[', key);
		if (br == std::string::npos || br >= to) return;
		size_t end = json.find(']', br);
		if (end == std::string::npos || end > to) return;
		size_t i = br + 1;
		while (i < end)
		{
			while (i < end && (json[i] < '0' || json[i] > '9')) ++i;
			int id = 0;
			bool any = false;
			while (i < end && json[i] >= '0' && json[i] <= '9')
			{
				any = true;
				id = id * 10 + (json[i] - '0');
				++i;
			}
			if (any && id > 0)
				ids.push_back(id);
		}
	}

	const std::unordered_map<int, LegRow>& LegCatalogByItemId()
	{
		static std::unordered_map<int, LegRow> map;
		static std::once_flag once;
		std::call_once(once, []() {
			const unsigned char* begin = _binary_legendaries_catalog_json_start;
			const unsigned char* end = _binary_legendaries_catalog_json_end;
			if (!begin || !end || end <= begin)
				return;
			const std::string json(reinterpret_cast<const char*>(begin),
				static_cast<size_t>(end - begin));
			const size_t itemsKey = json.find("\"items\"");
			if (itemsKey == std::string::npos) return;
			const size_t arr = json.find('[', itemsKey);
			if (arr == std::string::npos) return;
			size_t p = arr + 1;
			while (p < json.size())
			{
				size_t brace = json.find('{', p);
				if (brace == std::string::npos) break;
				size_t objEnd = JsonObjectEnd(json, brace);
				if (objEnd == std::string::npos) break;
				std::string cat = JsonStringBounded(json, "category", brace, objEnd);
				std::string gen = JsonStringBounded(json, "generation", brace, objEnd);
				std::string typ = JsonStringBounded(json, "item_type", brace, objEnd);
				std::vector<int> ids;
				ParseItemIdsBounded(json, brace, objEnd, ids);
				for (int id : ids)
				{
					if (id <= 0) continue;
					LegRow& m = map[id];
					if (m.category.empty()) m.category = cat.empty() ? "Other" : cat;
					if (m.generation.empty()) m.generation = gen.empty() ? "Other" : gen;
					if (m.itemType.empty()) m.itemType = typ;
				}
				p = objEnd + 1;
			}
		});
		return map;
	}

	void ApplyLegCatalogMeta(std::vector<LegRow>& rows)
	{
		const auto& meta = LegCatalogByItemId();
		if (meta.empty()) return;
		for (LegRow& r : rows)
		{
			auto it = meta.find(r.id);
			if (it == meta.end())
			{
				if (r.category.empty()) r.category = "Other";
				if (r.generation.empty()) r.generation = "Other";
				continue;
			}
			if (r.category.empty()) r.category = it->second.category;
			if (r.generation.empty()) r.generation = it->second.generation;
			if (r.itemType.empty()) r.itemType = it->second.itemType;
		}
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

	PadNav::Blurb(
		"Armory grouped by type (same catalog as the Ledger). "
		"Use Plan on a legendary to open Crafting with its gift / forge tree.");

	if (PadNav::RefreshButton("###gw2igh_prog_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (ImGui::SmallButton("Open Ledger###gw2igh_prog_ledger"))
	{
		G::ShowWiki = true;
		Settings::SetDirty();
		if (BrowserTabs::OpenNewUrl("legvault", "about:legendary-vault") < 0)
			WikiBrowser::Navigate("about:legendary-vault");
	}
	ImGui::SameLine();
	if (gBusy)
		PadNav::StatusBusy();
	else if (!snap.status.empty())
		PadNav::StatusOk(snap.status.c_str());

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_prog_filter", "Filter name, type, set...",
		gFilter, sizeof(gFilter));

	PadNav::Meta("Type");
	ImGui::PushID("###gw2igh_prog_cat");
	for (int i = 0; i < kArmoryCatCount; ++i)
	{
		ImGui::PushID(i);
		if (PadNav::WrapButton(kArmoryCats[i], i == gCatFilter, /*first=*/i == 0))
			gCatFilter = i;
		ImGui::PopID();
	}
	ImGui::PopID();

	PadNav::Meta("Set");
	ImGui::PushID("###gw2igh_prog_gen");
	for (int i = 0; i < kArmoryGenCount; ++i)
	{
		ImGui::PushID(i);
		if (PadNav::WrapButton(kArmoryGens[i], i == gGenFilter, /*first=*/i == 0))
			gGenFilter = i;
		ImGui::PopID();
	}
	ImGui::PopID();

	PadNav::Meta("Show");
	if (PadNav::WrapButton("All", gShowMode == 0, /*first=*/true))
		gShowMode = 0;
	if (PadNav::WrapButton("Missing", gShowMode == 1))
		gShowMode = 1;
	if (PadNav::WrapButton("Unlocked", gShowMode == 2))
		gShowMode = 2;

	ImGui::Separator();

	PadLayout::BeginList("###gw2igh_prog_list", 80.f);

	DrawArmoryList(snap);

	ImGui::Spacing();
	ImGui::Separator();
	DrawCharacterRoster(snap);

	PadLayout::EndList();
}
