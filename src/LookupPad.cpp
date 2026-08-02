#include "LookupPad.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "HelperTheme.h"
#include "Settings.h"
#include "TpWatchPad.h"
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
	constexpr int kHttpTimeoutMs = 2500;

	struct Hit
	{
		int id = 0;
		std::string name;
		std::string rarity;
		std::string type;
		std::string level;
		long long buy = 0;
		long long sell = 0;
		bool hasPrices = false;
		bool ok = false;
		std::string status;
		std::vector<std::string> nameHints; /* wiki search titles when no id */
	};

	std::mutex gMu;
	Hit gHit;
	Hit gPending;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gReady{false};
	HANDLE gThread = nullptr;
	char gQuery[192] = {};
	bool gFocus = false;
	bool gPlaceOnce = false;

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

	ImVec4 RarityColor(const std::string& r)
	{
		if (r == "Legendary") return ImVec4(0.75f, 0.40f, 0.95f, 1.f);
		if (r == "Ascended") return ImVec4(0.95f, 0.35f, 0.55f, 1.f);
		if (r == "Exotic") return ImVec4(0.95f, 0.72f, 0.20f, 1.f);
		if (r == "Rare") return ImVec4(0.35f, 0.65f, 0.95f, 1.f);
		if (r == "Masterwork") return ImVec4(0.35f, 0.85f, 0.40f, 1.f);
		if (r == "Fine") return ImVec4(0.45f, 0.70f, 0.95f, 1.f);
		if (r == "Junk") return ImVec4(0.55f, 0.55f, 0.55f, 1.f);
		return ImVec4(0.85f, 0.85f, 0.85f, 1.f);
	}

	int ParseItemId(const char* text)
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
		/* Pure / leading numeric ID (ignore trailing junk). */
		bool onlyDigits = true;
		int id = 0;
		for (const char* p = text; *p; ++p)
		{
			if (*p == ' ' || *p == '\t') continue;
			if (*p >= '0' && *p <= '9')
				id = id * 10 + (*p - '0');
			else
			{
				onlyDigits = false;
				break;
			}
		}
		if (onlyDigits && id > 0) return id;
		return 0;
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
			if (c == '\\' && k < json.size())
			{
				char e = json[k++];
				if (e == 'n') out.push_back('\n');
				else if (e == 't') out.push_back('\t');
				else if (e == 'u' && k + 3 < json.size())
				{
					/* skip \uXXXX — keep ASCII fallback */
					k += 4;
				}
				else
					out.push_back(e);
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

	std::string UrlEncode(const char* s)
	{
		std::string o;
		static const char* hex = "0123456789ABCDEF";
		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
		{
			unsigned char c = *p;
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
				o.push_back(static_cast<char>(c));
			else if (c == ' ')
				o.push_back('+');
			else
			{
				o.push_back('%');
				o.push_back(hex[c >> 4]);
				o.push_back(hex[c & 15]);
			}
		}
		return o;
	}

	std::string WikiTitleToPath(const std::string& title)
	{
		std::string o;
		for (char c : title)
		{
			if (c == ' ') o.push_back('_');
			else if (c == '\'') o += "%27";
			else o.push_back(c);
		}
		return o;
	}

	int ExtractWikiItemId(const std::string& wikitext)
	{
		/* Infobox: | id = 19721  or |id=19721 */
		size_t p = 0;
		while (p < wikitext.size())
		{
			size_t bar = wikitext.find('|', p);
			if (bar == std::string::npos) break;
			size_t k = bar + 1;
			while (k < wikitext.size() && (wikitext[k] == ' ' || wikitext[k] == '\t')) ++k;
			if (k + 2 < wikitext.size() &&
				(wikitext[k] == 'i' || wikitext[k] == 'I') &&
				(wikitext[k + 1] == 'd' || wikitext[k + 1] == 'D') &&
				(wikitext[k + 2] == ' ' || wikitext[k + 2] == '=' || wikitext[k + 2] == '\t'))
			{
				while (k < wikitext.size() && wikitext[k] != '=') ++k;
				if (k < wikitext.size() && wikitext[k] == '=')
				{
					++k;
					while (k < wikitext.size() && (wikitext[k] == ' ' || wikitext[k] == '\t')) ++k;
					int id = 0;
					while (k < wikitext.size() && wikitext[k] >= '0' && wikitext[k] <= '9')
					{
						id = id * 10 + (wikitext[k] - '0');
						++k;
					}
					if (id > 0) return id;
				}
			}
			p = bar + 1;
		}
		return 0;
	}

	bool FillFromItemId(int id, Hit& hit)
	{
		hit = {};
		hit.id = id;
		char path[64];
		std::snprintf(path, sizeof(path), "/v2/items/%d", id);
		auto r = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (!r.ok || r.body.empty() || r.body[0] != '{')
		{
			hit.status = "Item not found on the official API.";
			return false;
		}
		hit.name = JsonStringAfterKey(r.body, "name", 0);
		hit.rarity = JsonStringAfterKey(r.body, "rarity", 0);
		hit.type = JsonStringAfterKey(r.body, "type", 0);
		long long lvl = JsonIntAfterKey(r.body, "level", 0);
		if (lvl >= 0)
		{
			char lb[24];
			std::snprintf(lb, sizeof(lb), "%lld", lvl);
			hit.level = lb;
		}
		if (hit.name.empty())
		{
			hit.status = "API returned an empty item.";
			return false;
		}

		std::snprintf(path, sizeof(path), "/v2/commerce/prices?ids=%d", id);
		auto pr = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (pr.ok)
		{
			size_t brace = pr.body.find('{');
			if (brace != std::string::npos)
			{
				size_t end = JsonObjectEnd(pr.body, brace);
				if (end != std::string::npos)
				{
					size_t buys = pr.body.find("\"buys\"", brace);
					size_t sells = pr.body.find("\"sells\"", brace);
					long long buy = -1, sell = -1;
					if (buys != std::string::npos && buys < end)
						buy = JsonIntAfterKey(pr.body, "unit_price", buys);
					if (sells != std::string::npos && sells < end)
						sell = JsonIntAfterKey(pr.body, "unit_price", sells);
					if (buy >= 0) hit.buy = buy;
					if (sell >= 0) hit.sell = sell;
					hit.hasPrices = (buy >= 0 || sell >= 0);
				}
			}
		}
		hit.ok = true;
		hit.status = "Ready.";
		return true;
	}

	void WikiNameSearch(const char* query, Hit& hit)
	{
		hit = {};
		hit.status = "Searching wiki…";
		std::string url =
			"https://wiki.guildwars2.com/api.php?action=query&list=search&srnamespace=0"
			"&srlimit=8&format=json&formatversion=2&srsearch=";
		url += UrlEncode(query);
		auto wr = Gw2Http::Get(url.c_str(), nullptr, kHttpTimeoutMs);
		if (!wr.ok)
		{
			hit.status = "Wiki search failed.";
			return;
		}

		/* "title":"Glob of Ectoplasm" */
		size_t p = 0;
		while (hit.nameHints.size() < 8 && p < wr.body.size())
		{
			size_t t = wr.body.find("\"title\"", p);
			if (t == std::string::npos) break;
			std::string title = JsonStringAfterKey(wr.body, "title", t);
			p = t + 7;
			if (title.empty()) continue;
			bool dup = false;
			for (const auto& h : hit.nameHints)
				if (h == title) { dup = true; break; }
			if (!dup) hit.nameHints.push_back(title);
		}
		if (hit.nameHints.empty())
		{
			hit.status = "No wiki results — try a chat code or item ID.";
			return;
		}

		/* Resolve first result's infobox id when possible. */
		std::string parseUrl =
			"https://wiki.guildwars2.com/api.php?action=parse&prop=wikitext&format=json"
			"&formatversion=2&page=";
		parseUrl += UrlEncode(hit.nameHints[0].c_str());
		auto pr = Gw2Http::Get(parseUrl.c_str(), nullptr, kHttpTimeoutMs);
		if (pr.ok)
		{
			std::string wt = JsonStringAfterKey(pr.body, "wikitext", 0);
			if (wt.empty())
			{
				/* formatversion 2 nests differently — look for raw wikitext string after key */
				size_t wk = pr.body.find("\"wikitext\"");
				if (wk != std::string::npos)
					wt = JsonStringAfterKey(pr.body, "wikitext", wk);
			}
			const int id = ExtractWikiItemId(wt);
			if (id > 0 && FillFromItemId(id, hit))
			{
				hit.nameHints.clear();
				return;
			}
		}
		hit.status = "Wiki hits — open a page, or paste a chat code / ID for full stats.";
	}

	DWORD WINAPI LookupProc(void* param)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		char q[192]{};
		std::snprintf(q, sizeof(q), "%s", static_cast<const char*>(param));
		Hit hit;
		const int id = ParseItemId(q);
		if (id > 0)
			FillFromItemId(id, hit);
		else if (q[0])
			WikiNameSearch(q, hit);
		else
			hit.status = "Paste a chat code, item ID, or name.";
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPending = std::move(hit);
			gReady = true;
			gBusy = false;
		}
		return 0;
	}

	char gThreadQuery[192] = {};

	void StartLookup()
	{
		if (!gQuery[0])
		{
			std::lock_guard<std::mutex> lock(gMu);
			gHit = {};
			gHit.status = "Paste a chat code, item ID, or name.";
			return;
		}
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		std::snprintf(gThreadQuery, sizeof(gThreadQuery), "%s", gQuery);
		{
			std::lock_guard<std::mutex> lock(gMu);
			gHit.status = "Looking up…";
		}
		gThread = CreateThread(nullptr, 0, LookupProc, gThreadQuery, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			std::lock_guard<std::mutex> lock(gMu);
			gHit.status = "Could not start lookup.";
		}
	}

	void Tick()
	{
		if (!gReady) return;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gReady) return;
		gHit = std::move(gPending);
		gPending = {};
		gReady = false;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
	}

	void OpenUrl(const char* url)
	{
		G::ShowWiki = true;
		Settings::SetDirty();
		if (BrowserTabs::OpenNewUrl("lookup", url) < 0)
			WikiBrowser::Navigate(url);
	}
}

void LookupPad::OpenAndLookup()
{
	G::ShowLookup = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
}

void LookupPad::RenderContents()
{
	Tick();
	Hit hit;
	{
		std::lock_guard<std::mutex> lock(gMu);
		hit = gHit;
	}

	ImGui::TextUnformatted("Item lookup");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Chat code (Shift+click), numeric ID, or item name. Official API + wiki — read-only.");
	ImGui::PopTextWrapPos();

	const float btnW = ImGui::CalcTextSize("Lookup").x + ImGui::GetStyle().FramePadding.x * 2.f + 16.f;
	float fieldW = ImGui::GetContentRegionAvail().x - btnW - ImGui::GetStyle().ItemSpacing.x;
	if (fieldW < 120.f) fieldW = 120.f;
	ImGui::SetNextItemWidth(fieldW);
	if (ImGui::InputTextWithHint("###gw2igh_lookup_q", "[&AgEAAAA=] / ID / name",
			gQuery, sizeof(gQuery), ImGuiInputTextFlags_EnterReturnsTrue))
		StartLookup();
	ImGui::SameLine();
	if (ImGui::Button("Lookup###gw2igh_lookup_go", ImVec2(btnW, 0.f)))
		StartLookup();

	if (gBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Loading…");
	else if (!hit.status.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", hit.status.c_str());

	ImGui::Separator();

	if (hit.ok)
	{
		ImGui::TextColored(RarityColor(hit.rarity), "%s", hit.name.c_str());
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "#%d", hit.id);
		char meta[160];
		std::snprintf(meta, sizeof(meta), "%s%s%s%s%s",
			hit.rarity.c_str(),
			hit.type.empty() ? "" : " · ",
			hit.type.c_str(),
			hit.level.empty() ? "" : " · Lv ",
			hit.level.c_str());
		ImGui::TextUnformatted(meta);

		if (hit.hasPrices)
		{
			char pl[128];
			std::snprintf(pl, sizeof(pl), "Buy %s  ·  Sell %s",
				FormatCoins(hit.buy).c_str(), FormatCoins(hit.sell).c_str());
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", pl);
		}
		else
			ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "No TP listings");

		if (ImGui::Button("Wiki###gw2igh_lookup_wiki"))
		{
			char url[384];
			std::snprintf(url, sizeof(url),
				"https://wiki.guildwars2.com/wiki/%s",
				WikiTitleToPath(hit.name).c_str());
			OpenUrl(url);
		}
		ImGui::SameLine();
		if (ImGui::Button("BLTC###gw2igh_lookup_bltc"))
		{
			char url[128];
			std::snprintf(url, sizeof(url), "https://www.gw2bltc.com/en/item/%d", hit.id);
			OpenUrl(url);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add to TP###gw2igh_lookup_tp"))
		{
			std::string csv = G::TpWatchIds;
			char idBuf[24];
			std::snprintf(idBuf, sizeof(idBuf), "%d", hit.id);
			bool already = false;
			{
				/* Token-aware check so 21 does not match 19721. */
				const char* p = csv.c_str();
				while (*p)
				{
					while (*p == ',' || *p == ' ') ++p;
					int v = 0;
					bool any = false;
					while (*p >= '0' && *p <= '9')
					{
						any = true;
						v = v * 10 + (*p - '0');
						++p;
					}
					if (any && v == hit.id) { already = true; break; }
					while (*p && *p != ',') ++p;
				}
			}
			if (!already)
			{
				if (!csv.empty() && csv.back() != ',') csv += ',';
				csv += idBuf;
				if (csv.size() < sizeof(G::TpWatchIds))
				{
					std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", csv.c_str());
					Settings::SetDirty();
				}
			}
			TpWatchPad::OpenAndRefresh();
		}
	}
	else if (!hit.nameHints.empty())
	{
		ImGui::TextUnformatted("Wiki results");
		for (size_t i = 0; i < hit.nameHints.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Selectable(hit.nameHints[i].c_str()))
			{
				char url[384];
				std::snprintf(url, sizeof(url),
					"https://wiki.guildwars2.com/wiki/%s",
					WikiTitleToPath(hit.nameHints[i]).c_str());
				OpenUrl(url);
			}
			ImGui::PopID();
		}
	}

}

bool LookupPad::Render()
{
	if (!G::ShowLookup)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 0.f), ImVec2(520.f, io.DisplaySize.y * 0.85f));
	ImGui::SetNextWindowSize(ImVec2(400.f, 0.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gPlaceOnce)
	{
		const float x = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x * 0.42f : 120.f;
		const float y = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y * 0.18f : 100.f;
		ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
		ImGui::SetNextWindowFocus();
		gPlaceOnce = false;
	}
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowLookup;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Item Lookup##GW2InGameHelperLookup", &open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowLookup = false;
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowLookup = false;
		Settings::SetDirty();
	}

	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
