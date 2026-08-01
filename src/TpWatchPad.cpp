#include "TpWatchPad.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "PadDock.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kMaxItems = 120;
	constexpr int kHttpTimeoutMs = 2000;
	constexpr float kTpPadW = 420.f;

	struct Row
	{
		int id = 0;
		std::string name;
		long long buy = 0;
		long long sell = 0;
	};

	std::mutex gMu;
	std::vector<Row> gRows;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gResultReady{false};
	std::vector<Row> gPending;
	HANDLE gThread = nullptr;
	char gAddBuf[160] = {};
	std::string gStatus;
	bool gRequestFocus = false;

	std::string FormatCoins(long long copper)
	{
		if (copper < 0) copper = 0;
		const long long g = copper / 10000;
		const long long s = (copper % 10000) / 100;
		const long long c = copper % 100;
		char buf[64];
		if (g > 0)
			std::snprintf(buf, sizeof(buf), "%lldg %02llds %02lldc", g, s, c);
		else if (s > 0)
			std::snprintf(buf, sizeof(buf), "%llds %02lldc", s, c);
		else
			std::snprintf(buf, sizeof(buf), "%lldc", c);
		return buf;
	}

	void ParseIds(const char* csv, std::vector<int>& out)
	{
		out.clear();
		if (!csv) return;
		const char* p = csv;
		while (*p && out.size() < static_cast<size_t>(kMaxItems))
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t') ++p;
			if (!*p) break;
			int v = 0;
			bool any = false;
			while (*p >= '0' && *p <= '9')
			{
				any = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (any && v > 0)
			{
				bool dup = false;
				for (int x : out) if (x == v) { dup = true; break; }
				if (!dup) out.push_back(v);
			}
			while (*p && *p != ',' && *p != ';' && !(*p >= '0' && *p <= '9')) ++p;
		}
	}

	void SaveIds(const std::vector<int>& ids)
	{
		std::string s;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) s += ',';
			s += std::to_string(ids[i]);
		}
		if (s.size() >= sizeof(G::TpWatchIds))
			s.resize(sizeof(G::TpWatchIds) - 1);
		std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", s.c_str());
		Settings::SetDirty();
	}

	int ParseItemInput(const char* text)
	{
		if (!text || !text[0]) return 0;
		const char* a = std::strstr(text, "[&");
		if (a)
		{
			a += 2;
			const char* b = std::strchr(a, ']');
			if (b && b > a)
			{
				static const char kB64[] =
					"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				int buf = 0, bits = 0;
				unsigned char out[16]{};
				size_t n = 0;
				for (const char* p = a; p < b && n < sizeof(out); ++p)
				{
					if (*p == '=' || *p == ' ') break;
					const char* q = std::strchr(kB64, *p);
					if (!q) continue;
					buf = (buf << 6) | static_cast<int>(q - kB64);
					bits += 6;
					if (bits >= 8)
					{
						bits -= 8;
						out[n++] = static_cast<unsigned char>((buf >> bits) & 0xFF);
					}
				}
				if (n >= 5 && out[0] == 0x02)
				{
					const int id = out[2] | (out[3] << 8) | (out[4] << 16);
					if (id > 0) return id;
				}
			}
		}
		int id = 0;
		for (const char* p = text; *p; ++p)
		{
			if (*p >= '0' && *p <= '9')
				id = id * 10 + (*p - '0');
			else if (id > 0)
				break;
		}
		return id;
	}

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
			if (c == '\\' && k < json.size()) { out.push_back(json[k++]); continue; }
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

	std::string IdsQuery(const std::vector<int>& ids)
	{
		std::string q;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) q += ',';
			q += std::to_string(ids[i]);
		}
		return q;
	}

	void FetchInto(std::vector<Row>& rows)
	{
		if (rows.empty()) return;
		std::vector<int> ids;
		ids.reserve(rows.size());
		for (const Row& r : rows) ids.push_back(r.id);

		{
			std::string path = "/v2/items?ids=";
			path += IdsQuery(ids);
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (r.ok)
			{
				size_t p = 0;
				while (p < r.body.size())
				{
					size_t brace = r.body.find('{', p);
					if (brace == std::string::npos) break;
					size_t end = JsonObjectEnd(r.body, brace);
					if (end == std::string::npos) break;
					long long id = JsonIntAfterKey(r.body, "id", brace);
					std::string name = JsonStringAfterKey(r.body, "name", brace);
					if (id > 0 && !name.empty())
					{
						for (Row& row : rows)
							if (row.id == static_cast<int>(id)) { row.name = name; break; }
					}
					p = end + 1;
				}
			}
		}
		{
			std::string path = "/v2/commerce/prices?ids=";
			path += IdsQuery(ids);
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (r.ok)
			{
				size_t p = 0;
				while (p < r.body.size())
				{
					size_t brace = r.body.find('{', p);
					if (brace == std::string::npos) break;
					size_t end = JsonObjectEnd(r.body, brace);
					if (end == std::string::npos) break;
					long long id = JsonIntAfterKey(r.body, "id", brace);
					size_t buys = r.body.find("\"buys\"", brace);
					size_t sells = r.body.find("\"sells\"", brace);
					long long buy = -1, sell = -1;
					if (buys != std::string::npos && buys < end)
						buy = JsonIntAfterKey(r.body, "unit_price", buys);
					if (sells != std::string::npos && sells < end)
						sell = JsonIntAfterKey(r.body, "unit_price", sells);
					if (id > 0)
					{
						for (Row& row : rows)
						{
							if (row.id == static_cast<int>(id))
							{
								if (buy >= 0) row.buy = buy;
								if (sell >= 0) row.sell = sell;
								break;
							}
						}
					}
					p = end + 1;
				}
			}
		}
	}

	DWORD WINAPI FetchProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		std::vector<int> ids;
		ParseIds(G::TpWatchIds, ids);
		std::vector<Row> rows;
		rows.reserve(ids.size());
		for (int id : ids)
		{
			Row r;
			r.id = id;
			rows.push_back(std::move(r));
		}
		FetchInto(rows);
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPending = std::move(rows);
			gResultReady = true;
			gBusy = false;
		}
		return 0;
	}

	void StartFetch()
	{
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		gStatus = "Fetching prices…";
		gThread = CreateThread(nullptr, 0, FetchProc, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			gStatus = "Could not start price fetch.";
		}
	}

	void SyncRowsFromSettings()
	{
		std::vector<int> ids;
		ParseIds(G::TpWatchIds, ids);
		std::lock_guard<std::mutex> lock(gMu);
		std::vector<Row> next;
		next.reserve(ids.size());
		for (int id : ids)
		{
			Row r;
			r.id = id;
			for (const Row& old : gRows)
			{
				if (old.id == id)
				{
					r = old;
					break;
				}
			}
			next.push_back(std::move(r));
		}
		gRows = std::move(next);
	}
}

void TpWatchPad::Load() {}

void TpWatchPad::OpenAndRefresh()
{
	G::ShowTpWatch = true;
	gRequestFocus = true;
	Settings::SetDirty();
	SyncRowsFromSettings();
	StartFetch();
}

void TpWatchPad::Tick()
{
	if (!gResultReady)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	if (!gResultReady)
		return;
	gRows = std::move(gPending);
	gPending.clear();
	gResultReady = false;
	gStatus = gRows.empty() ? "Watchlist empty." : "Prices updated.";
	if (gThread)
	{
		WaitForSingleObject(gThread, 0);
		CloseHandle(gThread);
		gThread = nullptr;
	}
}

bool TpWatchPad::Render()
{
	TpWatchPad::Tick();
	if (!G::ShowTpWatch)
	{
		PadDock::ClearTp();
		return false;
	}

	std::vector<Row> rows;
	{
		std::lock_guard<std::mutex> lock(gMu);
		rows = gRows;
	}

	const ImGuiIO& io = ImGui::GetIO();
	const float maxWinH = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y * 0.85f : 720.f;
	/* Few items: auto-size to content (no clipping, no empty void).
	   Many items: fixed-ish window + scrolling list. */
	constexpr size_t kAutoFitMax = 8;
	const bool autoFit = rows.size() <= kAutoFitMax;

	ImGui::SetNextWindowSizeConstraints(ImVec2(380.f, 0.f), ImVec2(520.f, maxWinH));
	if (!autoFit)
		ImGui::SetNextWindowSize(ImVec2(440.f, maxWinH * 0.72f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gRequestFocus)
	{
		ImGui::SetNextWindowPos(PadDock::ForTp(kTpPadW), ImGuiCond_Always);
		ImGui::SetNextWindowFocus();
		gRequestFocus = false;
	}

	ImGuiWindowFlags winFlags = autoFit ? ImGuiWindowFlags_AlwaysAutoResize : 0;
	bool open = G::ShowTpWatch;
	if (!ImGui::Begin("TP Watchlist##GW2InGameHelperTpWatch", &open, winFlags))
	{
		PadDock::RememberTp(ImGui::GetWindowPos(), ImGui::GetWindowSize());
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowTpWatch = false;
			PadDock::ClearTp();
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowTpWatch = false;
		PadDock::ClearTp();
		Settings::SetDirty();
	}
	PadDock::RememberTp(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	ImGui::TextUnformatted("Trading Post watchlist");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Paste a chat code (Shift+click in game) or item ID. "
		"Read-only prices — never buys or sells.");
	ImGui::PopTextWrapPos();

	auto tryAdd = [&]() {
		if (!gAddBuf[0]) return;
		const int id = ParseItemInput(gAddBuf);
		if (id <= 0)
		{
			gStatus = "Could not read that chat code / ID.";
			return;
		}
		std::vector<int> ids;
		ParseIds(G::TpWatchIds, ids);
		for (int x : ids)
		{
			if (x == id)
			{
				gStatus = "Already on your watchlist.";
				return;
			}
		}
		if (static_cast<int>(ids.size()) >= kMaxItems)
		{
			gStatus = "Watchlist full (120).";
			return;
		}
		ids.push_back(id);
		SaveIds(ids);
		gAddBuf[0] = 0;
		SyncRowsFromSettings();
		StartFetch();
		gStatus = "Added. Fetching price…";
	};

	const float addBtnW = ImGui::CalcTextSize("Add").x + ImGui::GetStyle().FramePadding.x * 2.f + 16.f;
	const float addFieldW = ImGui::GetContentRegionAvail().x - addBtnW - ImGui::GetStyle().ItemSpacing.x;
	ImGui::SetNextItemWidth(addFieldW > 120.f ? addFieldW : 120.f);
	if (ImGui::InputTextWithHint("###gw2igh_tp_pad_add", "[&AgEAAAA=] or item ID",
			gAddBuf, sizeof(gAddBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		tryAdd();
	ImGui::SameLine();
	if (ImGui::Button("Add###gw2igh_tp_pad_addbtn", ImVec2(addBtnW, 0.f)))
		tryAdd();

	if (ImGui::Button("Refresh prices###gw2igh_tp_pad_ref"))
		StartFetch();
	ImGui::SameLine();
	if (gBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Loading…");
	else if (!gStatus.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", gStatus.c_str());

	ImGui::Separator();

	auto drawRows = [&]() {
		if (rows.empty())
		{
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextWrapped(
				"No items yet. Shift+click an item in chat, paste the [&…] code above, then Add.");
			ImGui::PopTextWrapPos();
			return;
		}
		for (size_t i = 0; i < rows.size(); ++i)
		{
			const Row& r = rows[i];
			ImGui::PushID(static_cast<int>(r.id));
			const char* name = r.name.empty() ? "…" : r.name.c_str();

			/* Name + actions on one row so buttons never sit below a clipped edge. */
			const ImGuiStyle& st = ImGui::GetStyle();
			const float removeW = ImGui::CalcTextSize("Remove").x + st.FramePadding.x * 2.f;
			const float bltcW = ImGui::CalcTextSize("BLTC").x + st.FramePadding.x * 2.f;
			const float actionsW = removeW + bltcW + st.ItemSpacing.x;
			float nameW = ImGui::GetContentRegionAvail().x - actionsW - st.ItemSpacing.x;
			if (nameW < 80.f) nameW = 80.f;

			ImGui::BeginGroup();
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + nameW);
			ImGui::TextUnformatted(name);
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();
			ImGui::SameLine(0.f, st.ItemSpacing.x);
			if (ImGui::SmallButton("Remove"))
			{
				std::vector<int> ids;
				ParseIds(G::TpWatchIds, ids);
				std::vector<int> next;
				for (int x : ids) if (x != r.id) next.push_back(x);
				SaveIds(next);
				SyncRowsFromSettings();
				StartFetch();
				gStatus = "Removed.";
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("BLTC"))
			{
				char url[128];
				std::snprintf(url, sizeof(url), "https://www.gw2bltc.com/en/item/%d", r.id);
				G::ShowWiki = true; /* show helper so the new tab is visible */
				Settings::SetDirty();
				if (BrowserTabs::OpenNewUrl("gw2bltc", url) < 0)
					WikiBrowser::Navigate(url); /* tab bar full — current tab */
			}

			ImGui::PushTextWrapPos(0.f);
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "#%d", r.id);
			char priceLine[192];
			if (r.sell > r.buy && r.buy > 0)
			{
				std::snprintf(priceLine, sizeof(priceLine),
					"Buy %s  ·  Sell %s  ·  spread %s",
					FormatCoins(r.buy).c_str(), FormatCoins(r.sell).c_str(),
					FormatCoins(r.sell - r.buy).c_str());
				ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", priceLine);
			}
			else if (r.buy == 0 && r.sell == 0 && !r.name.empty())
			{
				std::snprintf(priceLine, sizeof(priceLine), "Buy %s  ·  Sell %s",
					FormatCoins(r.buy).c_str(), FormatCoins(r.sell).c_str());
				ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", priceLine);
				ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "No TP listings");
			}
			else
			{
				std::snprintf(priceLine, sizeof(priceLine), "Buy %s  ·  Sell %s",
					FormatCoins(r.buy).c_str(), FormatCoins(r.sell).c_str());
				ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", priceLine);
			}
			ImGui::PopTextWrapPos();

			if (i + 1 < rows.size())
				ImGui::Separator();
			ImGui::PopID();
		}
	};

	if (autoFit)
	{
		drawRows();
	}
	else
	{
		const float footerH = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
		float listH = ImGui::GetContentRegionAvail().y - footerH;
		if (listH < 120.f) listH = 120.f;
		ImGui::BeginChild("###gw2igh_tp_pad_list", ImVec2(0.f, listH), true);
		drawRows();
		ImGui::EndChild();
	}

	if (!rows.empty())
	{
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"%d item%s", static_cast<int>(rows.size()),
			rows.size() == 1 ? "" : "s");
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
