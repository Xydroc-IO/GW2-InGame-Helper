#include "UI.h"
#include "UI_Browse.h"

#include "AddonPaths.h"
#include "AccountPad.h"
#include "EventsPad.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "LivePanels.h"
#include "LogManagerPad.h"
#include "LookupPad.h"
#include "NotesPad.h"
#include "Settings.h"
#include "Sites.h"
#include "SyncQr.h"
#include "TekkitGuidesPad.h"
#include "TpWatchPad.h"
#include "VaultPad.h"
#include "WalletPad.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <shellapi.h>

void UI_Options()
{
	/* Options panel is a shared Nexus window — namespace our widgets. */
	ImGui::PushID("GW2-InGame-Helper");
	ImGui::TextUnformatted("GW2 In-Game Helper");
	ImGui::Separator();
	if (ImGui::Checkbox("Show helper window###gw2igh_show", &G::ShowWiki))
	{
		if (!G::ShowWiki)
			UI_ReleaseGameInput();
		Settings::SetDirty();
	}
	if (ImGui::Checkbox("Show Notes & Waypoints###gw2igh_shownotes", &G::ShowNotes))
	{
		if (G::ShowNotes)
			NotesPad::Open();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted,
		"Snippets + Waypoints search (copy chat codes). Keybind: KB_HELPER_NOTES (CTRL+SHIFT+N).");
	if (ImGui::Checkbox("Show Account pad###gw2igh_showaccount", &G::ShowAccount))
	{
		if (G::ShowAccount)
			AccountPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted,
		"Stash, vault, TP, crafting, progress. Keybind: KB_HELPER_ACCOUNT (CTRL+SHIFT+A).");
	if (ImGui::Checkbox("Show TP Watchlist###gw2igh_showtp", &G::ShowTpWatch))
	{
		if (G::ShowTpWatch)
			TpWatchPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted, "Add any item via chat code or ID — buy/sell prices (read-only).");
	if (ImGui::Checkbox("Show Item Lookup###gw2igh_showlookup", &G::ShowLookup))
	{
		if (G::ShowLookup)
			LookupPad::OpenAndLookup();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted, "Chat code / ID / name — rarity, TP prices, wiki & BLTC links.");
	if (ImGui::Checkbox("Show Wallet & Mats###gw2igh_showwallet", &G::ShowWallet))
	{
		if (G::ShowWallet)
			WalletPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted, "Gold, mats, bank, shared, and per-toon bags — searchable. Free-floating.");
	if (ImGui::Checkbox("Show Dailies & Vault###gw2igh_showvault", &G::ShowVault))
	{
		if (G::ShowVault)
			VaultPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted, "Season + live Vault objectives (Account → Vault). Keybind: KB_HELPER_ACCOUNT / CTRL+SHIFT+A.");
	if (ImGui::Checkbox("Show World Events###gw2igh_showevents", &G::ShowEvents))
	{
		if (G::ShowEvents)
			EventsPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted,
		"UTC bosses + metas. Keybind: KB_HELPER_EVENTS (CTRL+SHIFT+E).");
	if (ImGui::Checkbox("Show DPS Logs###gw2igh_showlogs", &G::ShowLogManager))
	{
		if (G::ShowLogManager)
			LogManagerPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted,
		"ArcDPS EVTC browser. Auto-installs Elite Insights. Optional dps.report token.");
	if (ImGui::Checkbox("Show Tekkit's Guides###gw2igh_showtekkit", &G::ShowTekkitGuides))
	{
		if (G::ShowTekkitGuides)
			TekkitGuidesPad::Open();
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted,
		"Tekkit categories + compass. Keybind: KB_HELPER_TEKKIT (CTRL+SHIFT+G).");
	if (ImGui::Checkbox("Enable Tekkit trail system###gw2igh_tekkitmaster", &G::ShowTekkitTrails))
		Settings::SetDirty();
	if (ImGui::Checkbox("Draw over in-game compass###gw2igh_compass", &G::ShowCompassOverlay))
		Settings::SetDirty();
	if (ImGui::Checkbox("In-world GPS trails###gw2igh_worldgps", &G::ShowWorldTrails))
		Settings::SetDirty();

	size_t count = 0;
	Sites::All(&count);
	if (count > 0)
	{
		ImGui::TextUnformatted("Default landing site");
		UI_Browse_DrawDefaultSitePicker();
		ImGui::TextColored(HelperTheme::Muted, "Home button uses this. Also used when no tabs are saved yet.");
	}

	if (ImGui::SliderFloat("Opacity###gw2igh_opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::SliderFloat("Font scale###gw2igh_font", &G::FontScale, 0.75f, 2.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::Checkbox("Keep browser warm when closed###gw2igh_warm", &G::KeepHelperWarm))
		Settings::SetDirty();
	ImGui::TextColored(HelperTheme::Muted, "Faster reopen; uses more RAM while the helper is hidden.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("GW2 API key (Live panels)");
	ImGui::TextColored(HelperTheme::Muted,
		"Read-only key from account.arena.net. Scopes: account + progression (Vault); "
		"wallet (Wallet pad); inventories + unlocks + characters (mats / Legendaries); "
		"tradingpost (TP delivery box); progression (Vault + event claim marks). "
		"Stored only in this addon’s settings.ini — never shared or sent in QR.");
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::InputTextWithHint("###gw2igh_apikey", "Paste API key here…", G::Gw2ApiKey, sizeof(G::Gw2ApiKey),
			ImGuiInputTextFlags_Password | ImGuiInputTextFlags_AutoSelectAll))
	{
		Settings::SetDirty();
		LivePanels::InvalidateCaches(AddonPaths::DataDir());
	}
	if (G::Gw2ApiKey[0])
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "Key saved — Reload Live tabs to refresh.");
	else
		ImGui::TextColored(HelperTheme::Muted, "No key — Vault/Progress show public data until you add one.");
	if (ImGui::Button("Clear API key###gw2igh_apikey_clear"))
	{
		G::Gw2ApiKey[0] = 0;
		Settings::SetDirty();
		LivePanels::InvalidateCaches(AddonPaths::DataDir());
	}
	ImGui::SameLine();
	if (ImGui::Button("Create key on account.arena.net###gw2igh_apikey_web"))
		ShellExecuteA(nullptr, "open", "https://account.arena.net/applications", nullptr, nullptr, SW_SHOWNORMAL);

	ImGui::Spacing();
	ImGui::TextUnformatted("TP panel (yours)");
	ImGui::TextColored(HelperTheme::Muted,
		"Watchlist: paste a chat code [&…] (Shift+click in game) or numeric ID — no key. "
		"Sell alerts (sell ≤ target) are set in the TP pad. "
		"Delivery box needs an API key with tradingpost. Toolbar TP button.");
	auto appendTpId = [&](int id) {
		if (id <= 0) return;
		std::vector<int> ids;
		const char* p = G::TpWatchIds;
		while (*p)
		{
			while (*p == ' ' || *p == ',') ++p;
			if (!*p) break;
			int v = 0;
			bool any = false;
			while (*p >= '0' && *p <= '9') { any = true; v = v * 10 + (*p - '0'); ++p; }
			if (any && v > 0)
			{
				bool dup = false;
				for (int x : ids) if (x == v) { dup = true; break; }
				if (!dup) ids.push_back(v);
			}
			while (*p && *p != ',' && !(*p >= '0' && *p <= '9')) ++p;
		}
		bool dup = false;
		for (int x : ids) if (x == id) { dup = true; break; }
		if (!dup && ids.size() < 120)
			ids.push_back(id);
		std::string csv;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) csv += ',';
			csv += std::to_string(ids[i]);
		}
		std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", csv.c_str());
		Settings::SetDirty();
		LivePanels::InvalidateTpCache(AddonPaths::DataDir());
	};
	/* Decode [&base64] item chat links (type 0x02). */
	auto parseItemInput = [](const char* text) -> int {
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
	};
	static char sTpAddId[128] = {};
	ImGui::SetNextItemWidth(-80.f);
	ImGui::InputTextWithHint("###gw2igh_tp_add", "[&AgEAAAA=] or item ID", sTpAddId, sizeof(sTpAddId));
	ImGui::SameLine();
	if (ImGui::Button("Add###gw2igh_tp_add_btn") && sTpAddId[0])
	{
		appendTpId(parseItemInput(sTpAddId));
		sTpAddId[0] = 0;
	}
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::InputTextWithHint("###gw2igh_tpwatch", "Saved IDs: 19721,24295,…", G::TpWatchIds, sizeof(G::TpWatchIds)))
	{
		Settings::SetDirty();
		LivePanels::InvalidateTpCache(AddonPaths::DataDir());
	}

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Browse / ... menu for Find / Copy / Open Ext. Right-click tabs to pin. "
		"Window size and position are saved automatically.");
	ImGui::TextWrapped(
		"Hotkeys (rebind in Nexus → Keybinds): Ctrl+Shift+H helper | "
		"Ctrl+Shift+A Account | Ctrl+Shift+G Tekkit's Guides | "
		"Ctrl+Shift+E Events | Ctrl+Shift+N Notes & Waypoints. "
		"In helper: Ctrl+T new tab | Ctrl+W close | Ctrl+Tab cycle | Ctrl+F find.");
	SyncQr::DrawOptionsSection();
	Settings::Save(false);
	ImGui::PopID();
}

