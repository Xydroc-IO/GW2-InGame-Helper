#include "UI.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

#include <windows.h>
#include <shellapi.h>

namespace
{
	bool gBrowserFocused = false;
	bool gBlockGameKeyboard = false;
	bool gBlockGameMouse = false;
	bool gWikiRectValid = false;
	ImVec2 gWikiMin{};
	ImVec2 gWikiMax{};
	bool gPendingDefocus = false;

	const ImVec4 kGold(0.941f, 0.776f, 0.353f, 1.f);       /* #f0c65a */
	const ImVec4 kGoldBright(1.f, 0.878f, 0.541f, 1.f);     /* #ffe08a */
	const ImVec4 kGoldDim(0.788f, 0.635f, 0.153f, 1.f);     /* #c9a227 */
	const ImVec4 kGoldMuted(0.75f, 0.62f, 0.32f, 0.88f);
	const ImVec4 kMuted(0.659f, 0.682f, 0.722f, 1.f);       /* #a8aeb8 */
	const ImVec4 kBg(0.024f, 0.027f, 0.039f, 1.f);          /* #06070a */
	const ImVec4 kPanel(0.071f, 0.078f, 0.102f, 1.f);       /* #12141a */
	const ImVec4 kBorder(0.353f, 0.290f, 0.157f, 0.95f);    /* #5a4a28 */
	const ImVec4 kTabActive(0.36f, 0.28f, 0.12f, 1.f);
	const ImVec4 kTabIdle(0.10f, 0.09f, 0.07f, 1.f);
	const ImVec4 kWarn(0.90f, 0.55f, 0.28f, 1.f);

	void PushWikiTheme()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.941f, 0.949f, 0.961f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, kMuted);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, kBg);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.047f, 0.051f, 0.067f, 0.92f));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.059f, 0.078f, 0.98f));
		ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.078f, 0.086f, 0.110f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.12f, 0.07f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.16f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.055f, 0.047f, 0.031f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.12f, 0.10f, 0.055f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.04f, 0.04f, 0.05f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, kPanel);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.04f, 0.04f, 0.05f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.35f, 0.28f, 0.14f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.55f, 0.42f, 0.18f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, kGold);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, kGold);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.75f, 0.58f, 0.22f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, kGoldBright);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.11f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.22f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.30f, 0.12f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.18f, 0.09f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.35f, 0.28f, 0.12f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.45f, 0.35f, 0.14f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.36f, 0.16f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, kGold);
		ImGui::PushStyleColor(ImGuiCol_SeparatorActive, kGoldBright);
		ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(0.45f, 0.36f, 0.16f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, kGold);
		ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, kGoldBright);
		ImGui::PushStyleColor(ImGuiCol_Tab, kTabIdle);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.30f, 0.24f, 0.11f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabActive, kTabActive);
		ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0.08f, 0.08f, 0.09f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0.22f, 0.18f, 0.09f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.55f, 0.42f, 0.15f, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_NavHighlight, kGold);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 12.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.f, 5.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 7.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.f);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
	}

	void PopWikiTheme()
	{
		ImGui::PopStyleVar(13);
		ImGui::PopStyleColor(40);
	}

	bool SoftButton(const char* label, bool enabled)
	{
		if (!enabled)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
			ImGui::Button(label);
			ImGui::PopStyleVar();
			return false;
		}
		return ImGui::Button(label);
	}

	void BlurBrowser()
	{
		if (!gBrowserFocused)
			return;
		gBrowserFocused = false;
		gBlockGameKeyboard = false;
		WikiBrowser::FeedFocus(false);
	}

	void FocusBrowser()
	{
		if (gBrowserFocused)
			return;
		gBrowserFocused = true;
		WikiBrowser::FeedFocus(true);
	}

	unsigned CefModsFromImGui(const ImGuiIO& io)
	{
		unsigned m = 0;
		if (io.KeyShift) m |= 1u << 1;
		if (io.KeyCtrl)  m |= 1u << 2;
		if (io.KeyAlt)   m |= 1u << 3;
		if (io.MouseDown[0]) m |= 1u << 4;
		if (io.MouseDown[2]) m |= 1u << 6;
		if (io.MouseDown[1]) m |= 1u << 5;
		return m;
	}

	void MapToCef(float localX, float localY, float drawW, float drawH, int* outX, int* outY)
	{
		const int fw = WikiBrowser::FrameWidth();
		const int fh = WikiBrowser::FrameHeight();
		if (fw <= 0 || fh <= 0 || drawW < 1.f || drawH < 1.f)
		{
			*outX = static_cast<int>(localX);
			*outY = static_cast<int>(localY);
			return;
		}
		int x = static_cast<int>(localX * (static_cast<float>(fw) / drawW));
		int y = static_cast<int>(localY * (static_cast<float>(fh) / drawH));
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x >= fw) x = fw - 1;
		if (y >= fh) y = fh - 1;
		*outX = x;
		*outY = y;
	}

	static char sFilter[64] = {};
	static int sCategoryIndex = 0;
	static bool sSyncCategory = true;
	static bool sFocusFilter = false;
	static bool sShowFind = false;
	static bool sRequestNewTabPicker = false;
	static char sFindQuery[128] = {};
	static bool sFindMatchCase = false;

	/* Visual-only Browse section headers (not Sites categories / not hubs). */
	const char* BrowseSection(const char* category, const char* id)
	{
		if (!category || !id || !id[0])
			return nullptr;

		if (std::strcmp(category, "Help") == 0)
			return "Getting Started";

		if (std::strcmp(category, "Search") == 0)
		{
			if (std::strcmp(id, "gemini") == 0)
				return "AI";
			return "Web Search";
		}
		if (std::strcmp(category, "Cheat Sheets") == 0)
		{
			if (std::strcmp(id, "raidfood") == 0 || std::strcmp(id, "raidutils") == 0 ||
				std::strcmp(id, "homegarden") == 0 || std::strcmp(id, "ascendedstart") == 0)
				return "Prep";
			if (std::strcmp(id, "sigilsrunes") == 0 || std::strcmp(id, "relics") == 0)
				return "Gear";
			if (std::strcmp(id, "booncheck") == 0 || std::strcmp(id, "squadtmpl") == 0 ||
				std::strcmp(id, "stabcleanse") == 0 || std::strcmp(id, "ccdefiance") == 0 ||
				std::strcmp(id, "portalspulls") == 0)
				return "Squad";
			if (std::strcmp(id, "fractalcons") == 0 || std::strcmp(id, "fractalcm") == 0)
				return "Fractals";
			if (std::strcmp(id, "raidwings") == 0 || std::strcmp(id, "strikes") == 0)
				return "Encounters";
			if (std::strcmp(id, "ubersaio") == 0 || std::strcmp(id, "dailyweekly") == 0 ||
				std::strcmp(id, "currencysinks") == 0 || std::strcmp(id, "matconv") == 0 ||
				std::strcmp(id, "legpaths") == 0 || std::strcmp(id, "mounts") == 0 ||
				std::strcmp(id, "homestead") == 0)
				return "Account";
			if (std::strcmp(id, "wvwcons") == 0)
				return "WvW";
			return "Other";
		}

		if (std::strcmp(category, "Tools") == 0)
		{
			if (std::strcmp(id, "gw2efficiency") == 0 || std::strcmp(id, "gw2eff_legendaries") == 0)
				return "Account";
			if (std::strcmp(id, "blishhud") == 0)
				return "Overlay";
			if (std::strcmp(id, "gw2timer_events") == 0 || std::strcmp(id, "gw2timer") == 0 ||
				std::strcmp(id, "gw2tldr_metas") == 0)
				return "Timers";
			if (std::strcmp(id, "gw2crafts") == 0 || std::strcmp(id, "gw2bltc") == 0 ||
				std::strcmp(id, "gw2treasures") == 0)
				return "Economy";
			if (std::strcmp(id, "killproof") == 0 || std::strcmp(id, "wingman") == 0 ||
				std::strcmp(id, "hs_arcdps") == 0)
				return "Logs / KP";
			if (std::strcmp(id, "gw2mb") == 0 || std::strcmp(id, "peuresearch") == 0)
				return "Misc";
			return "Other";
		}

		if (std::strcmp(category, "Guides") == 0)
		{
			if (std::strcmp(id, "guildjen") == 0 || std::strcmp(id, "guildjen_lw") == 0)
				return "Living World";
			if (std::strcmp(id, "mb_leveling") == 0 || std::strcmp(id, "mb_gold") == 0 ||
				std::strcmp(id, "gj_new_player") == 0 || std::strcmp(id, "gj_gold") == 0 ||
				std::strcmp(id, "gj_gem_store") == 0 || std::strcmp(id, "gj_wizards_vault") == 0)
				return "Progress";
			if (std::strcmp(id, "mb_griffon") == 0 || std::strcmp(id, "mb_skyscale") == 0 ||
				std::strcmp(id, "gj_roller_beetle") == 0 || std::strcmp(id, "gj_siege_turtle") == 0)
				return "Mounts";
			if (std::strcmp(id, "mb_intro_fractals") == 0 || std::strcmp(id, "mukluk_fractals") == 0 ||
				std::strcmp(id, "gj_fractals_hub") == 0 || std::strcmp(id, "gj_fractals_beginner") == 0 ||
				std::strncmp(id, "gj_frac_", 8) == 0)
				return "Fractals";
			if (std::strcmp(id, "gj_raid_guides") == 0 || std::strcmp(id, "gj_intro_raiding") == 0 ||
				std::strcmp(id, "gj_rw1") == 0 || std::strcmp(id, "gj_rw2") == 0 ||
				std::strcmp(id, "gj_rw3") == 0 || std::strcmp(id, "gj_rw4") == 0 ||
				std::strcmp(id, "gj_rw5") == 0 || std::strcmp(id, "gj_rw6") == 0 ||
				std::strcmp(id, "gj_rw7") == 0 || std::strcmp(id, "gj_rw8") == 0 ||
				std::strcmp(id, "sc_raids_hub") == 0 || std::strcmp(id, "sc_intro_squads") == 0 ||
				std::strcmp(id, "sc_squad_roles") == 0 || std::strcmp(id, "sc_joining_squads") == 0 ||
				std::strncmp(id, "sc_w1_", 6) == 0 || std::strncmp(id, "sc_w2_", 6) == 0 ||
				std::strncmp(id, "sc_w3_", 6) == 0 || std::strncmp(id, "sc_w4_", 6) == 0 ||
				std::strncmp(id, "sc_w5_", 6) == 0 || std::strncmp(id, "sc_w6_", 6) == 0 ||
				std::strncmp(id, "sc_w7_", 6) == 0 || std::strncmp(id, "gj_w8_", 6) == 0 ||
				std::strcmp(id, "mb_w8_balrior") == 0)
				return "Raids";
			if (std::strcmp(id, "mb_mai_trin") == 0 || std::strcmp(id, "mb_boneskinner") == 0 ||
				std::strcmp(id, "mb_cold_war") == 0 || std::strcmp(id, "mb_cosmic_obs") == 0 ||
				std::strcmp(id, "mb_forging_steel") == 0 || std::strcmp(id, "mb_fraenir") == 0 ||
				std::strcmp(id, "mb_icebrood") == 0 || std::strcmp(id, "mb_kaineng") == 0 ||
				std::strcmp(id, "mb_lions_court") == 0 || std::strcmp(id, "mb_cerus") == 0 ||
				std::strcmp(id, "mb_voice_claw") == 0 || std::strcmp(id, "mb_whisper") == 0 ||
				std::strcmp(id, "mb_ankka") == 0 || std::strcmp(id, "gj_harvest_temple") == 0)
				return "Strikes";
			if (std::strcmp(id, "gj_rifts") == 0)
				return "Rifts";
			if (std::strcmp(id, "mb_pvp_guides") == 0 || std::strcmp(id, "gj_pvp_hub") == 0 ||
				std::strcmp(id, "gj_pvp_beginner") == 0)
				return "PvP";
			if (std::strcmp(id, "mb_wvw_guides") == 0 || std::strcmp(id, "gj_wvw_beginner") == 0)
				return "WvW";
			if (std::strncmp(id, "gj_ach_", 7) == 0)
				return "Achievements";
			if (std::strcmp(id, "gj_jp_hub") == 0 || std::strncmp(id, "gj_jp_", 6) == 0)
				return "Jumping Puzzles";
			if (std::strcmp(id, "gw2tldr") == 0 || std::strcmp(id, "gw2tldr_raids") == 0 ||
				std::strcmp(id, "gw2tldr_fractals") == 0 || std::strcmp(id, "gw2tldr_dungeons") == 0)
				return "TLDR";
			return "Other";
		}

		if (std::strcmp(category, "Discord") == 0)
		{
			if (std::strcmp(id, "discord_official") == 0 || std::strcmp(id, "discord_community") == 0 ||
				std::strcmp(id, "discord_central") == 0)
				return "Community";
			if (std::strcmp(id, "discord_snowcrows") == 0 || std::strcmp(id, "discord_metabattle") == 0 ||
				std::strcmp(id, "discord_guildjen") == 0 || std::strcmp(id, "discord_mukluk") == 0 ||
				std::strcmp(id, "discord_aw2") == 0 || std::strcmp(id, "discord_skein") == 0)
				return "Builds / Sites";
			if (std::strcmp(id, "discord_fractal") == 0 || std::strcmp(id, "discord_raidacademy") == 0 ||
				std::strcmp(id, "discord_uni") == 0 || std::strcmp(id, "discord_crossroads") == 0 ||
				std::strcmp(id, "discord_rti") == 0)
				return "Training";
			if (std::strcmp(id, "discord_pvp") == 0)
				return "PvP";
			if (std::strcmp(id, "discord_wvw_na") == 0 || std::strcmp(id, "discord_wvw_eu") == 0)
				return "WvW";
			if (std::strcmp(id, "discord_fastfarming") == 0 || std::strcmp(id, "discord_overflow") == 0)
				return "Farming / Trade";
			if (std::strcmp(id, "discord_raidcore") == 0)
				return "Addons";
			return "Other";
		}

		if (std::strcmp(category, "Builds") == 0)
		{
			if (std::strcmp(id, "snowcrows") == 0 || std::strcmp(id, "sc_raid_ele") == 0 ||
				std::strcmp(id, "sc_raid_mes") == 0 || std::strcmp(id, "sc_raid_nec") == 0 ||
				std::strcmp(id, "sc_raid_eng") == 0 || std::strcmp(id, "sc_raid_ran") == 0 ||
				std::strcmp(id, "sc_raid_thf") == 0 || std::strcmp(id, "sc_raid_gua") == 0 ||
				std::strcmp(id, "sc_raid_rev") == 0 || std::strcmp(id, "sc_raid_war") == 0)
				return "Raids";
			if (std::strcmp(id, "sc_accessibuilds") == 0 || std::strcmp(id, "aw2help") == 0)
				return "AccessiBuilds";
			if (std::strcmp(id, "metabattle") == 0 || std::strcmp(id, "metabattle_ow") == 0 ||
				std::strcmp(id, "sc_open_world") == 0)
				return "Open World / General";
			if (std::strcmp(id, "metabattle_pvp") == 0 || std::strcmp(id, "sc_pvp") == 0)
				return "PvP";
			if (std::strcmp(id, "metabattle_wvw") == 0 || std::strcmp(id, "sc_wvw") == 0)
				return "WvW";
			if (std::strcmp(id, "gw2skills") == 0)
				return "Editor";
			return "Other";
		}

		if (std::strcmp(category, "Farming") == 0)
			return "Community";

		if (std::strcmp(category, "Wiki") == 0)
		{
			if (std::strcmp(id, "wiki") == 0)
				return "Main";
			if (std::strcmp(id, "wiki_updates") == 0)
				return "News";
			if (std::strcmp(id, "wiki_legendaries") == 0 || std::strcmp(id, "wiki_mounts") == 0)
				return "Collections";
			if (std::strcmp(id, "wiki_vault_easy") == 0)
				return "Wizards Vault";
			return "Other";
		}

		if (std::strcmp(category, "Official") == 0)
		{
			if (std::strcmp(id, "gw2official") == 0 || std::strcmp(id, "gw2news") == 0 ||
				std::strcmp(id, "gw2forums") == 0)
				return "ArenaNet";
			if (std::strcmp(id, "raidcore") == 0)
				return "Nexus";
			return "Other";
		}

		return nullptr; /* no sections for this category */
	}

	const char* const* BrowseSectionsForCategory(const char* category, size_t* outCount)
	{
		if (!category || !outCount)
			return nullptr;
		if (std::strcmp(category, "Help") == 0)
		{
			static const char* kSec[] = { "Getting Started" };
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Search") == 0)
		{
			static const char* kSec[] = { "Web Search", "AI" };
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Cheat Sheets") == 0)
		{
			static const char* kSec[] = {
				"Prep", "Gear", "Squad", "Fractals", "Encounters", "Account", "WvW", "Other"
			};
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Tools") == 0)
		{
			static const char* kSec[] = {
				"Account", "Overlay", "Timers", "Economy", "Logs / KP", "Misc", "Other"
			};
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Guides") == 0)
		{
			static const char* kSec[] = {
				"Living World", "Progress", "Mounts", "Fractals", "Raids",
				"Strikes", "Rifts", "PvP", "WvW", "Achievements",
				"Jumping Puzzles", "TLDR", "Other"
			};
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Discord") == 0)
		{
			static const char* kSec[] = {
				"Community", "Builds / Sites", "Training", "PvP", "WvW", "Farming / Trade", "Addons", "Other"
			};
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Builds") == 0)
		{
			static const char* kSec[] = {
				"Raids", "AccessiBuilds", "Open World / General", "PvP", "WvW", "Editor", "Other"
			};
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Farming") == 0)
		{
			static const char* kSec[] = { "Community" };
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Wiki") == 0)
		{
			static const char* kSec[] = { "Main", "News", "Collections", "Wizards Vault", "Other" };
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Official") == 0)
		{
			static const char* kSec[] = { "ArenaNet", "Nexus", "Other" };
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		*outCount = 0;
		return nullptr;
	}

	/* Open Browse sections only — missing key means collapsed (default). */
	std::unordered_set<std::string> gBrowseOpen;

	std::string BrowseSectionKey(const char* category, const char* section)
	{
		std::string k;
		k.reserve(64);
		k += category && category[0] ? category : "_";
		k += '|';
		k += section && section[0] ? section : "_";
		return k;
	}

	bool BrowseSectionIsOpen(const char* category, const char* section)
	{
		return gBrowseOpen.find(BrowseSectionKey(category, section)) != gBrowseOpen.end();
	}

	void BrowseSectionSetOpen(const char* category, const char* section, bool open)
	{
		const std::string key = BrowseSectionKey(category, section);
		const bool was = gBrowseOpen.find(key) != gBrowseOpen.end();
		if (open == was)
			return;
		if (open)
			gBrowseOpen.insert(key);
		else
			gBrowseOpen.erase(key);
		Settings::SetDirty();
	}

	/* Wing subsection under Guides → Raids → Raid Boss (nullptr = overview / prep). */
	const char* RaidBossWing(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strncmp(id, "sc_w1_", 6) == 0)
			return "W1 Spirit Vale";
		if (std::strncmp(id, "sc_w2_", 6) == 0)
			return "W2 Salvation Pass";
		if (std::strncmp(id, "sc_w3_", 6) == 0)
			return "W3 Stronghold";
		if (std::strncmp(id, "sc_w4_", 6) == 0)
			return "W4 Bastion";
		if (std::strncmp(id, "sc_w5_", 6) == 0)
			return "W5 Hall of Chains";
		if (std::strncmp(id, "sc_w6_", 6) == 0)
			return "W6 Mythwright";
		if (std::strncmp(id, "sc_w7_", 6) == 0)
			return "W7 Ahdashim";
		if (std::strncmp(id, "gj_w8_", 6) == 0 || std::strcmp(id, "mb_w8_balrior") == 0)
			return "W8 Mount Balrior";
		return nullptr;
	}

	/* Raid Wings vs Raid Boss under Guides → Raids. */
	const char* RaidsSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "gj_raid_guides") == 0 || std::strcmp(id, "gj_intro_raiding") == 0 ||
			std::strcmp(id, "gj_rw1") == 0 || std::strcmp(id, "gj_rw2") == 0 ||
			std::strcmp(id, "gj_rw3") == 0 || std::strcmp(id, "gj_rw4") == 0 ||
			std::strcmp(id, "gj_rw5") == 0 || std::strcmp(id, "gj_rw6") == 0 ||
			std::strcmp(id, "gj_rw7") == 0 || std::strcmp(id, "gj_rw8") == 0)
			return "Raid Wings";
		if (std::strcmp(id, "sc_raids_hub") == 0 || std::strcmp(id, "sc_intro_squads") == 0 ||
			std::strcmp(id, "sc_squad_roles") == 0 || std::strcmp(id, "sc_joining_squads") == 0 ||
			std::strncmp(id, "sc_w1_", 6) == 0 || std::strncmp(id, "sc_w2_", 6) == 0 ||
			std::strncmp(id, "sc_w3_", 6) == 0 || std::strncmp(id, "sc_w4_", 6) == 0 ||
			std::strncmp(id, "sc_w5_", 6) == 0 || std::strncmp(id, "sc_w6_", 6) == 0 ||
			std::strncmp(id, "sc_w7_", 6) == 0 || std::strncmp(id, "gj_w8_", 6) == 0 ||
			std::strcmp(id, "mb_w8_balrior") == 0)
			return "Raid Boss";
		return nullptr;
	}

	/* Expansion subsection under Guides → Achievements (nullptr = hub overview). */
	const char* AchievementsSub(const char* id)
	{
		if (!id || std::strncmp(id, "gj_ach_", 7) != 0)
			return nullptr;
		if (std::strcmp(id, "gj_ach_hub") == 0)
			return nullptr;
		if (std::strncmp(id, "gj_ach_lw_", 10) == 0)
			return "Living World";
		if (std::strncmp(id, "gj_ach_hot_", 11) == 0)
			return "Heart of Thorns";
		if (std::strncmp(id, "gj_ach_pof_", 11) == 0)
			return "Path of Fire";
		if (std::strncmp(id, "gj_ach_eod_", 11) == 0)
			return "End of Dragons";
		if (std::strncmp(id, "gj_ach_soto_", 12) == 0)
			return "Secrets of the Obscure";
		if (std::strncmp(id, "gj_ach_jw_", 10) == 0)
			return "Janthir Wilds";
		if (std::strncmp(id, "gj_ach_voe_", 11) == 0)
			return "Visions of Eternity";
		if (std::strncmp(id, "gj_ach_fest_", 12) == 0)
			return "Festivals";
		return nullptr;
	}

	/* Returns true when the section body should be drawn. Defaults collapsed;
	   open state is restored from settings and saved when the user toggles. */
	bool BeginBrowseSection(const char* category, const char* section, int count)
	{
		if (!section || !section[0])
			return true;
		char label[160];
		std::snprintf(label, sizeof(label), "%s (%d)###bsec_%s_%s",
			section, count,
			category && category[0] ? category : "_",
			section);
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, kGoldDim);
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.10f, 0.055f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.18f, 0.09f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.32f, 0.26f, 0.12f, 1.f));
		ImGui::SetNextItemOpen(BrowseSectionIsOpen(category, section), ImGuiCond_Once);
		const bool open = ImGui::CollapsingHeader(label);
		ImGui::PopStyleColor(4);
		if (ImGui::IsItemToggledOpen())
			BrowseSectionSetOpen(category, section, open);
		return open;
	}

	void ActivateSiteIndex(int index, bool navigate, bool newTab)
	{
		if (index < 0)
			return;
		size_t siteCount = 0;
		const SiteDef* sites = Sites::All(&siteCount);
		if (!sites || index >= static_cast<int>(siteCount) || !sites[index].id)
			return;

		if (newTab)
		{
			if (BrowserTabs::OpenNew(sites[index].id, navigate) < 0 && navigate)
				BrowserTabs::OpenInActive(sites[index].id, navigate);
		}
		else
			BrowserTabs::OpenInActive(sites[index].id, navigate);
	}

	void SetDefaultSiteIndex(int index)
	{
		if (index < 0)
			return;
		size_t siteCount = 0;
		const SiteDef* sites = Sites::All(&siteCount);
		if (!sites || index >= static_cast<int>(siteCount) || !sites[index].id)
			return;
		std::snprintf(G::DefaultSiteId, sizeof(G::DefaultSiteId), "%s", sites[index].id);
		Settings::SetDirty();
		Settings::Save(true); /* Options can change while the helper is closed */
	}

	const SiteDef* SiteById(const char* id)
	{
		const int idx = Sites::IndexOfId(id);
		if (idx < 0)
			return nullptr;
		size_t n = 0;
		const SiteDef* sites = Sites::All(&n);
		if (!sites || idx >= static_cast<int>(n))
			return nullptr;
		return &sites[idx];
	}

	/* ProggyClean lacks · — … etc. Keep ImGui labels ASCII-only. */
	void SanitizeForUi(char* dst, size_t dstLen, const char* src)
	{
		if (!dst || dstLen == 0)
			return;
		dst[0] = 0;
		if (!src)
			return;
		size_t o = 0;
		for (size_t i = 0; src[i] && o + 1 < dstLen; )
		{
			const unsigned char c = static_cast<unsigned char>(src[i]);
			if (c < 0x80)
			{
				dst[o++] = static_cast<char>(c);
				++i;
				continue;
			}
			/* UTF-8 em/en dash → '-' */
			if ((c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
					(static_cast<unsigned char>(src[i + 2]) == 0x94 ||
						static_cast<unsigned char>(src[i + 2]) == 0x93)))
			{
				dst[o++] = '-';
				i += 3;
				continue;
			}
			/* middle dot · */
			if (c == 0xC2 && static_cast<unsigned char>(src[i + 1]) == 0xB7)
			{
				dst[o++] = '-';
				i += 2;
				continue;
			}
			/* ellipsis … */
			if (c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
				static_cast<unsigned char>(src[i + 2]) == 0xA6)
			{
				if (o + 3 < dstLen)
				{
					dst[o++] = '.';
					dst[o++] = '.';
					dst[o++] = '.';
				}
				i += 3;
				continue;
			}
			/* skip other multibyte sequences */
			if ((c & 0xE0) == 0xC0) i += 2;
			else if ((c & 0xF0) == 0xE0) i += 3;
			else if ((c & 0xF8) == 0xF0) i += 4;
			else ++i;
		}
		dst[o] = 0;
	}
	void DrawStarShape(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, bool filled)
	{
		ImVec2 pts[10];
		for (int i = 0; i < 10; ++i)
		{
			const float a = -3.14159265f * 0.5f + static_cast<float>(i) * 3.14159265f / 5.f;
			const float r = (i & 1) ? radius * 0.42f : radius;
			pts[i] = ImVec2(center.x + std::cos(a) * r, center.y + std::sin(a) * r);
		}
		if (filled)
			dl->AddConvexPolyFilled(pts, 10, col);
		else
			dl->AddPolyline(pts, 10, col, true, 1.6f);
	}

	bool FavoriteToggleButton(const char* id, bool favorited, bool smallBtn)
	{
		ImGui::PushID(id);
		const float h = smallBtn ? ImGui::GetFrameHeight() * 0.85f : ImGui::GetFrameHeight();
		const ImVec2 size(h, h);
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const bool pressed = ImGui::InvisibleButton("##star", size);
		const ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
		const float radius = size.x * 0.32f;
		const bool hovered = ImGui::IsItemHovered();
		ImU32 col;
		if (favorited)
			col = ImGui::GetColorU32(hovered ? ImVec4(1.f, 0.85f, 0.35f, 1.f) : kGold);
		else
			col = ImGui::GetColorU32(hovered ? ImVec4(0.85f, 0.88f, 0.92f, 1.f) : kMuted);
		DrawStarShape(dl, center, radius, col, favorited);
		if (hovered)
			ImGui::SetTooltip(favorited ? "Remove from Favorites" : "Add to Favorites");
		ImGui::PopID();
		return pressed;
	}

	void DrawFavoriteStar(const char* siteId)
	{
		if (!siteId || !siteId[0])
			return;
		const bool fav = Sites::IsFavorite(siteId);
		if (FavoriteToggleButton("row", fav, true))
			Sites::ToggleFavorite(siteId);
	}

	void DrawBrowsePanelContents(bool navigateOnChange, bool* closePanel, bool pickDefaultSite = false, bool pickNewTab = false)
	{
		size_t siteCount = 0;
		const SiteDef* sites = Sites::All(&siteCount);
		size_t catCount = 0;
		const char* const* cats = Sites::Categories(&catCount);
		if (!sites || siteCount == 0 || !cats || catCount == 0)
			return;

		/* Index 0 = virtual Favorites (browse / new-tab); categories follow. */
		const int totalCats = pickDefaultSite
			? static_cast<int>(catCount)
			: static_cast<int>(catCount) + 1;

		if (sSyncCategory)
		{
			sSyncCategory = false;
			const char* focusId = pickDefaultSite ? G::DefaultSiteId : Sites::ActiveId();
			const SiteDef* focus = SiteById(focusId);
			if (!pickDefaultSite && Sites::IsFavorite(focusId))
				sCategoryIndex = 0;
			else
			{
				const char* activeCat = (focus && focus->category) ? focus->category : "";
				sCategoryIndex = pickDefaultSite ? 0 : 1;
				for (int i = 0; i < static_cast<int>(catCount); ++i)
				{
					if (std::strcmp(cats[i] ? cats[i] : "", activeCat) == 0)
					{
						sCategoryIndex = pickDefaultSite ? i : (i + 1);
						break;
					}
				}
			}
		}
		if (sCategoryIndex < 0 || sCategoryIndex >= totalCats)
			sCategoryIndex = 0;

		if (pickDefaultSite)
		{
			ImGui::TextColored(kGold, "Default landing site");
			ImGui::TextColored(kMuted, "Home button - and when no tabs are saved yet.");
		}
		else if (pickNewTab)
		{
			ImGui::TextColored(kGold, "Open in new tab");
			ImGui::TextColored(kMuted, "Pick a site to open beside your current tabs.");
		}

		ImGui::TextColored(kGold, "Search");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.f);
		if (sFocusFilter)
		{
			ImGui::SetKeyboardFocusHere();
			sFocusFilter = false;
		}
		ImGui::InputTextWithHint("##site_filter", "Filter sites...", sFilter, sizeof(sFilter));

		const bool filtering = sFilter[0] != '\0';
		const bool showFavorites = (!filtering && !pickDefaultSite && sCategoryIndex == 0);
		const char* selectedCat = "";
		if (!filtering && !showFavorites)
		{
			const int catIdx = pickDefaultSite ? sCategoryIndex : (sCategoryIndex - 1);
			if (catIdx >= 0 && catIdx < static_cast<int>(catCount))
				selectedCat = cats[catIdx] ? cats[catIdx] : "";
		}

		const float listH = pickDefaultSite ? 300.f : 320.f;
		const float leftW = 172.f;

		ImGui::BeginChild("##browse_cats", ImVec2(leftW, listH), true);
		ImGui::PushStyleColor(ImGuiCol_Text, kGold);
		ImGui::TextUnformatted("Categories");
		ImGui::PopStyleColor();
		ImGui::Separator();

		if (!pickDefaultSite)
		{
			char favLabel[64];
			std::snprintf(favLabel, sizeof(favLabel), "Favorites (%d)", Sites::FavoriteCount());
			if (ImGui::Selectable(favLabel, sCategoryIndex == 0))
			{
				sCategoryIndex = 0;
				sFilter[0] = '\0';
			}
		}
		for (int i = 0; i < static_cast<int>(catCount); ++i)
		{
			const char* cat = cats[i] ? cats[i] : "";
			const int uiIndex = pickDefaultSite ? i : (i + 1);
			const bool selected = (uiIndex == sCategoryIndex);
			char label[96];
			std::snprintf(label, sizeof(label), "%s (%d)", cat, Sites::CountInCategory(cat));
			if (selected)
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.32f, 0.26f, 0.12f, 0.95f));
			if (ImGui::Selectable(label, selected))
			{
				sCategoryIndex = uiIndex;
				sFilter[0] = '\0';
			}
			if (selected)
				ImGui::PopStyleColor();
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##browse_sites", ImVec2(0.f, listH), true);
		ImGui::PushStyleColor(ImGuiCol_Text, kGold);
		if (filtering)
			ImGui::TextUnformatted("Matching sites");
		else if (showFavorites)
			ImGui::TextUnformatted("Favorites");
		else
			ImGui::TextUnformatted(selectedCat);
		ImGui::PopStyleColor();
		ImGui::Separator();

		const int current = pickDefaultSite
			? Sites::IndexOfId(G::DefaultSiteId)
			: Sites::ActiveIndex();
		int shown = 0;

		auto DrawSiteRow = [&](int siteIndex, bool withCategoryPrefix) {
			if (siteIndex < 0 || siteIndex >= static_cast<int>(siteCount))
				return;
			const SiteDef& site = sites[siteIndex];
			ImGui::PushID(siteIndex);
			if (!pickDefaultSite)
			{
				DrawFavoriteStar(site.id);
				ImGui::SameLine();
			}
			char row[160];
			if (withCategoryPrefix)
			{
				char safe[160];
				char tmp[160];
				std::snprintf(tmp, sizeof(tmp), "%s - %s",
					site.category ? site.category : "",
					site.label ? site.label : "");
				SanitizeForUi(safe, sizeof(safe), tmp);
				std::snprintf(row, sizeof(row), "%s", safe);
			}
			else
				std::snprintf(row, sizeof(row), "%s", site.label ? site.label : "");

			const bool selected = (siteIndex == current);
			const bool ctrl = ImGui::GetIO().KeyCtrl;
			if (ImGui::Selectable(row, selected))
			{
				if (pickDefaultSite)
					SetDefaultSiteIndex(siteIndex);
				else if (pickNewTab)
					ActivateSiteIndex(siteIndex, true, true);
				else
					ActivateSiteIndex(siteIndex, navigateOnChange, ctrl);
				if (closePanel)
					*closePanel = true;
				sSyncCategory = true;
			}
			if (ImGui::IsItemHovered())
			{
				if (pickNewTab)
					ImGui::SetTooltip("Open in a new tab");
				else if (!pickDefaultSite)
					ImGui::SetTooltip("Click: this tab | Ctrl+click: new tab");
				else if (site.title && site.title[0])
				{
					char tip[160];
					SanitizeForUi(tip, sizeof(tip), site.title);
					ImGui::SetTooltip("%s", tip);
				}
			}

			/* ASCII subtitle when title adds detail beyond the label. */
			if (!withCategoryPrefix && site.title && site.title[0] && site.label &&
				std::strcmp(site.title, site.label) != 0)
			{
				char sanitized[160];
				SanitizeForUi(sanitized, sizeof(sanitized), site.title);
				const char* src = sanitized;
				const char* dash = std::strstr(sanitized, " - ");
				if (dash)
					src = dash + 3;
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
				char shortSub[72];
				std::snprintf(shortSub, sizeof(shortSub), "%.48s", src);
				ImGui::TextUnformatted(shortSub);
				ImGui::PopStyleColor();
			}

			/* Drag-reorder favorites */
			if (showFavorites && !pickDefaultSite && !pickNewTab)
			{
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					const int favSlot = [&]() {
						const int favN = Sites::FavoriteCount();
						for (int f = 0; f < favN; ++f)
						{
							if (Sites::FavoriteSiteIndex(f) == siteIndex)
								return f;
						}
						return -1;
					}();
					ImGui::SetDragDropPayload("FAV_REORDER", &favSlot, sizeof(favSlot));
					ImGui::TextUnformatted(site.label ? site.label : "Favorite");
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FAV_REORDER"))
					{
						const int from = *static_cast<const int*>(payload->Data);
						const int favN = Sites::FavoriteCount();
						int to = -1;
						for (int f = 0; f < favN; ++f)
						{
							if (Sites::FavoriteSiteIndex(f) == siteIndex)
							{
								to = f;
								break;
							}
						}
						if (from >= 0 && to >= 0)
							Sites::MoveFavorite(from, to);
					}
					ImGui::EndDragDropTarget();
				}
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
			ImGui::PopID();
			++shown;
		};

		if (showFavorites)
		{
			const int favN = Sites::FavoriteCount();
			for (int f = 0; f < favN; ++f)
				DrawSiteRow(Sites::FavoriteSiteIndex(f), true);
		}
		else if (filtering)
		{
			for (int i = 0; i < static_cast<int>(siteCount); ++i)
			{
				if (!Sites::MatchesFilter(sites[i], sFilter))
					continue;
				DrawSiteRow(i, true);
			}
			if (shown > 0)
			{
				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
				ImGui::Text("%d match%s", shown, shown == 1 ? "" : "es");
				ImGui::PopStyleColor();
			}
		}
		else
		{
			size_t secCount = 0;
			const char* const* sections = BrowseSectionsForCategory(selectedCat, &secCount);
			bool anyInCategory = false;
			if (sections && secCount > 0)
			{
				for (size_t s = 0; s < secCount; ++s)
				{
					const char* section = sections[s];
					int secSites = 0;
					for (int i = 0; i < static_cast<int>(siteCount); ++i)
					{
						const SiteDef& site = sites[i];
						if (!site.category || std::strcmp(site.category, selectedCat) != 0)
							continue;
						const char* sec = BrowseSection(selectedCat, site.id);
						if (sec && std::strcmp(sec, section) == 0)
							++secSites;
					}
					if (secSites == 0)
						continue;
					anyInCategory = true;
					if (!BeginBrowseSection(selectedCat, section, secSites))
						continue;
					/* Raids: Raid Wings + Raid Boss (wings nested under Raid Boss). */
					if (std::strcmp(section, "Raids") == 0)
					{
						static const char* kRaidSubs[] = { "Raid Wings", "Raid Boss" };
						static const char* kWings[] = {
							"W1 Spirit Vale", "W2 Salvation Pass", "W3 Stronghold", "W4 Bastion",
							"W5 Hall of Chains", "W6 Mythwright", "W7 Ahdashim", "W8 Mount Balrior"
						};
						ImGui::Indent(10.f);
						for (const char* sub : kRaidSubs)
						{
							int subCount = 0;
							for (int i = 0; i < static_cast<int>(siteCount); ++i)
							{
								const SiteDef& site = sites[i];
								if (!site.category || std::strcmp(site.category, selectedCat) != 0)
									continue;
								const char* s = RaidsSub(site.id);
								if (s && std::strcmp(s, sub) == 0)
									++subCount;
							}
							if (subCount == 0)
								continue;
							if (!BeginBrowseSection("Raids", sub, subCount))
								continue;

							if (std::strcmp(sub, "Raid Wings") == 0)
							{
								for (int i = 0; i < static_cast<int>(siteCount); ++i)
								{
									const SiteDef& site = sites[i];
									if (!site.category || std::strcmp(site.category, selectedCat) != 0)
										continue;
									const char* s = RaidsSub(site.id);
									if (!s || std::strcmp(s, "Raid Wings") != 0)
										continue;
									DrawSiteRow(i, false);
								}
								continue;
							}

							/* Raid Boss: prep/overview, then wing subsections. */
							for (int i = 0; i < static_cast<int>(siteCount); ++i)
							{
								const SiteDef& site = sites[i];
								if (!site.category || std::strcmp(site.category, selectedCat) != 0)
									continue;
								const char* s = RaidsSub(site.id);
								if (!s || std::strcmp(s, "Raid Boss") != 0)
									continue;
								if (RaidBossWing(site.id))
									continue;
								DrawSiteRow(i, false);
							}
							ImGui::Indent(10.f);
							for (const char* wing : kWings)
							{
								int wingCount = 0;
								for (int i = 0; i < static_cast<int>(siteCount); ++i)
								{
									const SiteDef& site = sites[i];
									if (!site.category || std::strcmp(site.category, selectedCat) != 0)
										continue;
									const char* w = RaidBossWing(site.id);
									if (w && std::strcmp(w, wing) == 0)
										++wingCount;
								}
								if (wingCount == 0)
									continue;
								if (!BeginBrowseSection("Raid Boss", wing, wingCount))
									continue;
								for (int i = 0; i < static_cast<int>(siteCount); ++i)
								{
									const SiteDef& site = sites[i];
									if (!site.category || std::strcmp(site.category, selectedCat) != 0)
										continue;
									const char* w = RaidBossWing(site.id);
									if (!w || std::strcmp(w, wing) != 0)
										continue;
									DrawSiteRow(i, false);
								}
							}
							ImGui::Unindent(10.f);
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Achievements") == 0)
					{
						static const char* kAchSubs[] = {
							"Living World", "Heart of Thorns", "Path of Fire", "End of Dragons",
							"Secrets of the Obscure", "Janthir Wilds", "Visions of Eternity", "Festivals"
						};
						for (int i = 0; i < static_cast<int>(siteCount); ++i)
						{
							const SiteDef& site = sites[i];
							if (!site.category || std::strcmp(site.category, selectedCat) != 0)
								continue;
							const char* sec = BrowseSection(selectedCat, site.id);
							if (!sec || std::strcmp(sec, "Achievements") != 0)
								continue;
							if (AchievementsSub(site.id))
								continue;
							DrawSiteRow(i, false);
						}
						ImGui::Indent(10.f);
						for (const char* sub : kAchSubs)
						{
							int subCount = 0;
							for (int i = 0; i < static_cast<int>(siteCount); ++i)
							{
								const SiteDef& site = sites[i];
								if (!site.category || std::strcmp(site.category, selectedCat) != 0)
									continue;
								const char* s = AchievementsSub(site.id);
								if (s && std::strcmp(s, sub) == 0)
									++subCount;
							}
							if (subCount == 0)
								continue;
							if (!BeginBrowseSection("Achievements", sub, subCount))
								continue;
							for (int i = 0; i < static_cast<int>(siteCount); ++i)
							{
								const SiteDef& site = sites[i];
								if (!site.category || std::strcmp(site.category, selectedCat) != 0)
									continue;
								const char* s = AchievementsSub(site.id);
								if (!s || std::strcmp(s, sub) != 0)
									continue;
								DrawSiteRow(i, false);
							}
						}
						ImGui::Unindent(10.f);
						continue;
					}
					for (int i = 0; i < static_cast<int>(siteCount); ++i)
					{
						const SiteDef& site = sites[i];
						if (!site.category || std::strcmp(site.category, selectedCat) != 0)
							continue;
						const char* sec = BrowseSection(selectedCat, site.id);
						if (!sec || std::strcmp(sec, section) != 0)
							continue;
						DrawSiteRow(i, false);
					}
				}
			}
			else
			{
				for (int i = 0; i < static_cast<int>(siteCount); ++i)
				{
					const SiteDef& site = sites[i];
					const char* cat = site.category ? site.category : "";
					if (std::strcmp(cat, selectedCat) != 0)
						continue;
					anyInCategory = true;
					DrawSiteRow(i, false);
				}
			}
			/* All sections collapsed still means the category has sites. */
			if (shown == 0 && anyInCategory)
				shown = 1;
		}

		if (shown == 0)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
			if (filtering)
				ImGui::TextUnformatted("No matches.");
			else if (showFavorites)
				ImGui::TextUnformatted("No favorites yet. Click the star next to a site.");
			else
				ImGui::TextUnformatted("No sites in this category.");
			ImGui::PopStyleColor();
		}
		ImGui::EndChild();
	}

	void DrawTabBar()
	{
		BrowserTabs::EnsureDefault();
		const int n = BrowserTabs::Count();
		const int active = BrowserTabs::ActiveIndex();
		int pendingClose = -1;

		/* One widget per tab: "Title  x". Separate title+x buttons were easy to
		   mis-hit (last tab's x clipped / click landed on the previous x). */
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 4.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.f, 4.f));
		ImGui::BeginChild("##tab_bar", ImVec2(0.f, ImGui::GetFrameHeightWithSpacing() + 4.f), false,
			ImGuiWindowFlags_HorizontalScrollbar);

		const float closeZone = ImGui::CalcTextSize("  x").x + ImGui::GetStyle().FramePadding.x;

		for (int i = 0; i < n; ++i)
		{
			ImGui::PushID(i);
			const BrowserTabs::Tab& tab = BrowserTabs::At(i);
			const bool selected = (i == active);
			const bool canClose = (n > 1 && !tab.pinned);

			char label[96];
			const char* title = tab.title[0] ? tab.title : "Tab";
			if (canClose)
				std::snprintf(label, sizeof(label), "%s  x###tab%d", title, i);
			else if (n > 1 && tab.pinned)
				std::snprintf(label, sizeof(label), "%s  ·###tab%d", title, i);
			else
				std::snprintf(label, sizeof(label), "%s###tab%d", title, i);

			if (selected)
				ImGui::PushStyleColor(ImGuiCol_Button, kTabActive);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, kTabIdle);
			const bool pressed = ImGui::Button(label);
			ImGui::PopStyleColor();

			const ImVec2 rmin = ImGui::GetItemRectMin();
			const ImVec2 rmax = ImGui::GetItemRectMax();

			if (tab.pinned)
			{
				ImGui::GetWindowDrawList()->AddCircleFilled(
					ImVec2(rmin.x + 5.f, rmin.y + 5.f),
					2.6f,
					ImGui::GetColorU32(kGold));
			}
			if (selected)
			{
				ImGui::GetWindowDrawList()->AddRectFilled(
					ImVec2(rmin.x, rmax.y - 2.f),
					ImVec2(rmax.x, rmax.y),
					ImGui::GetColorU32(kGold));
			}

			if (pressed)
			{
				const float mx = ImGui::GetIO().MousePos.x;
				if (canClose && mx >= (rmax.x - closeZone))
					pendingClose = i;
				else
					BrowserTabs::Activate(i);
			}

			if (ImGui::BeginPopupContextItem("##tab_ctx"))
			{
				if (ImGui::MenuItem(tab.pinned ? "Unpin" : "Pin"))
					BrowserTabs::TogglePin(i);
				if (ImGui::MenuItem("Close", nullptr, false, canClose))
					pendingClose = i;
				ImGui::EndPopup();
			}

			if (ImGui::IsItemHovered())
			{
				if (tab.pinned)
					ImGui::SetTooltip("%s (pinned — unpin to close)", title);
				else if (canClose)
				{
					const float mx = ImGui::GetIO().MousePos.x;
					if (mx >= (rmax.x - closeZone))
						ImGui::SetTooltip("Close tab");
				}
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && canClose)
					pendingClose = i;
			}

			ImGui::SameLine();
			ImGui::PopID();
		}

		const bool canAdd = (n < BrowserTabs::kMaxTabs);
		if (canAdd)
		{
			if (ImGui::Button("+##new_tab") || sRequestNewTabPicker)
			{
				sRequestNewTabPicker = false;
				sSyncCategory = true;
				sFocusFilter = true;
				ImGui::OpenPopup("##site_browse_newtab");
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Open site in a new tab (Ctrl+T)");
		}
		else
		{
			sRequestNewTabPicker = false;
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
			ImGui::Button("+##new_tab");
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Tab limit reached (8)");
		}

		ImGui::SameLine(0.f, 6.f);
		{
			const bool canRe = BrowserTabs::CanReopenClosed();
			if (canRe)
			{
				if (ImGui::SmallButton("^##reopen"))
					BrowserTabs::ReopenClosed();
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Reopen closed tab (Ctrl+Shift+T)");
			}
		}

		ImGui::SetNextWindowSize(ImVec2(640.f, 420.f), ImGuiCond_Always);
		if (ImGui::BeginPopup("##site_browse_newtab"))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(true, &closePanel, false, true);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::EndChild();
		ImGui::PopStyleVar(2);

		if (pendingClose >= 0)
			BrowserTabs::Close(pendingClose);
	}

	void CopyCurrentUrl()
	{
		std::string copy = WikiBrowser::CurrentUrl();
		if (copy.empty() || copy.rfind("about:", 0) == 0 || copy.rfind("file:", 0) == 0)
			copy = Sites::ResolveUrl(Sites::Active());
		if (copy.empty() || copy.rfind("about:", 0) == 0 || copy.rfind("file:", 0) == 0)
			return;
		if (!OpenClipboard(nullptr))
			return;
		EmptyClipboard();
		const SIZE_T bytes = copy.size() + 1;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (mem)
		{
			void* locked = GlobalLock(mem);
			if (locked)
			{
				std::memcpy(locked, copy.c_str(), bytes);
				GlobalUnlock(mem);
				SetClipboardData(CF_TEXT, mem);
			}
		}
		CloseClipboard();
	}

	void OpenCurrentExternal()
	{
		std::string openUrl = WikiBrowser::CurrentUrl();
		if (openUrl.empty() || openUrl.rfind("about:", 0) == 0 || openUrl.rfind("file:", 0) == 0)
			openUrl = Sites::ResolveUrl(Sites::Active());
		if (!openUrl.empty() &&
			(openUrl.rfind("http://", 0) == 0 || openUrl.rfind("https://", 0) == 0))
		{
			ShellExecuteA(nullptr, "open", openUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	void DuplicateActiveTab()
	{
		if (BrowserTabs::Count() >= BrowserTabs::kMaxTabs)
			return;
		std::string u = WikiBrowser::CurrentUrl();
		if (u.empty() || u.rfind("about:", 0) == 0 || u.rfind("file:", 0) == 0)
			u = Sites::ResolveUrl(Sites::Active());
		BrowserTabs::OpenNewUrl(Sites::ActiveId(), u);
	}

	/* Friendly status — muted gold; hide Ready / closed noise. */
	void DrawStatusChip()
	{
		const std::string raw = WikiBrowser::Status();
		if (raw.empty() || raw == "Ready")
			return;

		const char* label = nullptr;
		ImVec4 col = kGoldMuted;
		if (raw.find("Loading") != std::string::npos ||
			raw.find("Navigating") != std::string::npos ||
			raw.find("Launching") != std::string::npos ||
			raw.find("Creating") != std::string::npos)
		{
			label = "Loading...";
			col = kGold;
		}
		else if (raw.find("Closed") != std::string::npos ||
			raw.find("Hidden") != std::string::npos)
		{
			return;
		}
		else if (raw.find("fail") != std::string::npos ||
			raw.find("Fail") != std::string::npos ||
			raw.find("error") != std::string::npos ||
			raw.find("Error") != std::string::npos ||
			raw.find("not found") != std::string::npos ||
			raw.find("disabled") != std::string::npos)
		{
			label = "Error - check Nexus log";
			col = kWarn;
		}
		else
		{
			/* Truncate long technical strings. */
			static char buf[48];
			if (raw.size() > 40)
			{
				std::snprintf(buf, sizeof(buf), "%.37s...", raw.c_str());
				label = buf;
			}
			else
				label = raw.c_str();
			col = kGoldDim;
		}

		ImGui::SameLine();
		ImGui::TextColored(col, "%s", label);
	}

	void DrawMoreMenu()
	{
		if (ImGui::Button("...##more"))
			ImGui::OpenPopup("##toolbar_more");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("More actions");

		if (ImGui::BeginPopup("##toolbar_more"))
		{
			if (ImGui::MenuItem(sShowFind ? "Hide find" : "Find in page", "Ctrl+F"))
				sShowFind = !sShowFind;
			if (ImGui::MenuItem("Copy URL"))
				CopyCurrentUrl();
			if (ImGui::MenuItem("Open externally"))
				OpenCurrentExternal();
			ImGui::Separator();
			{
				const bool canNew = BrowserTabs::Count() < BrowserTabs::kMaxTabs;
				if (ImGui::MenuItem("New tab (duplicate)", nullptr, false, canNew))
					DuplicateActiveTab();
			}
			{
				const int ai = BrowserTabs::ActiveIndex();
				const bool pinned = BrowserTabs::At(ai).pinned;
				if (ImGui::MenuItem(pinned ? "Unpin tab" : "Pin tab"))
					BrowserTabs::TogglePin(ai);
			}
			{
				const bool canRe = BrowserTabs::CanReopenClosed();
				if (ImGui::MenuItem("Reopen closed tab", "Ctrl+Shift+T", false, canRe))
					BrowserTabs::ReopenClosed();
			}
			ImGui::EndPopup();
		}
	}

	void DrawToolbar()
	{
		const SiteDef& active = Sites::Active();

		if (ImGui::Button("Browse"))
		{
			sSyncCategory = true;
			sFocusFilter = true;
			ImGui::OpenPopup("##site_browse");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s - %s",
				active.category ? active.category : "",
				active.label ? active.label : "");

		ImGui::SameLine(0.f, 4.f);
		{
			const bool fav = Sites::IsFavorite(Sites::ActiveId());
			if (FavoriteToggleButton("toolbar", fav, false))
				Sites::ToggleFavorite(Sites::ActiveId());
		}

		/* Compact nav cluster */
		ImGui::SameLine(0.f, 12.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f, 6.f));
		if (SoftButton("<", BrowserTabs::CanGoBack()))
			BrowserTabs::GoBack();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Back");
		ImGui::SameLine();
		if (SoftButton(">", BrowserTabs::CanGoForward()))
			BrowserTabs::GoForward();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Forward");
		ImGui::SameLine();
		if (ImGui::Button("Home"))
			BrowserTabs::GoHome();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Default landing site");
		ImGui::SameLine();
		if (ImGui::Button("Reload"))
			BrowserTabs::Reload();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Reload");
		ImGui::PopStyleVar();

		ImGui::SameLine(0.f, 12.f);
		{
			float avail = ImGui::GetContentRegionAvail().x - 90.f;
			if (avail < 140.f) avail = 140.f;
			if (avail > 280.f) avail = 280.f;
			ImGui::SetNextItemWidth(avail);
		}
		if (ImGui::InputTextWithHint("##site_query", "Search...", G::LastQuery, sizeof(G::LastQuery),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if (G::LastQuery[0])
			{
				WikiBrowser::Search(G::LastQuery);
				Settings::SetDirty();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Search this site (Wiki / Google). Enter to go.");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Go"))
		{
			if (G::LastQuery[0])
			{
				WikiBrowser::Search(G::LastQuery);
				Settings::SetDirty();
			}
		}

		ImGui::SameLine(0.f, 8.f);
		DrawMoreMenu();
		DrawStatusChip();

		ImGui::SetNextWindowSize(ImVec2(640.f, 420.f), ImGuiCond_Always);
		if (ImGui::BeginPopup("##site_browse"))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(true, &closePanel);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}


	void DrawDefaultSiteBrowse()
	{
		if (ImGui::Button("Choose default site..."))
		{
			sSyncCategory = true;
			sFocusFilter = true;
			ImGui::OpenPopup("##default_site_browse");
		}

		ImGui::SameLine();
		const SiteDef* def = SiteById(G::DefaultSiteId);
		if (def)
			ImGui::TextColored(kMuted, "%s - %s",
				def->category ? def->category : "",
				def->label ? def->label : "");
		else
			ImGui::TextColored(kMuted, "%s", G::DefaultSiteId);

		ImGui::SetNextWindowSize(ImVec2(640.f, 420.f), ImGuiCond_Always);
		if (ImGui::BeginPopup("##default_site_browse"))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(false, &closePanel, true);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}
}

bool UI_BlocksGameKeyboard()
{
	return gBlockGameKeyboard;
}

void UI_ParseBrowseOpen(const char* val)
{
	gBrowseOpen.clear();
	if (!val || !val[0])
		return;
	const char* p = val;
	while (*p)
	{
		while (*p == ';' || *p == ' ')
			++p;
		if (!*p)
			break;
		const char* start = p;
		while (*p && *p != ';')
			++p;
		std::string key(start, p);
		while (!key.empty() && (key.back() == ' ' || key.back() == '\r' || key.back() == '\n'))
			key.pop_back();
		if (!key.empty() && key.find('|') != std::string::npos)
			gBrowseOpen.insert(std::move(key));
	}
}

void UI_WriteBrowseOpen(FILE* f)
{
	if (!f)
		return;
	std::fputs("BrowseOpen=", f);
	bool first = true;
	for (const std::string& key : gBrowseOpen)
	{
		if (key.find('|') == std::string::npos)
			continue;
		if (!first)
			std::fputc(';', f);
		first = false;
		std::fputs(key.c_str(), f);
	}
	std::fputc('\n', f);
}

bool UI_BlocksGameMouse()
{
	return gBlockGameMouse;
}

bool UI_IsPointerOverWiki(int clientX, int clientY)
{
	if (!G::ShowWiki || !gWikiRectValid)
		return false;
	const float x = static_cast<float>(clientX);
	const float y = static_cast<float>(clientY);
	return x >= gWikiMin.x && y >= gWikiMin.y && x < gWikiMax.x && y < gWikiMax.y;
}

void UI_ReleaseGameInput()
{
	BlurBrowser();
	gBlockGameMouse = false;
	gPendingDefocus = true;
}

void UI_Render()
{
	/* Always poll first — must run while the helper is closed too. */
	HelperHotkeys_Poll();

	gBlockGameKeyboard = false;
	gBlockGameMouse = false;
	gWikiRectValid = false;

	if (gPendingDefocus)
	{
		gPendingDefocus = false;
		ImGui::SetWindowFocus(nullptr);
	}

	static bool sWasOpen = false;
	if (!G::ShowWiki)
	{
		if (sWasOpen)
		{
			BrowserTabs::PrepareSave();
			Settings::SetDirty();
			Settings::Save(true);
			sWasOpen = false;
		}
		BlurBrowser();
		WikiBrowser::SetVisible(false);
		return;
	}

	if (!sWasOpen)
	{
		BrowserTabs::NavigateActive();
		sWasOpen = true;
	}

	ImGui::SetNextWindowSize(ImVec2(G::WindowWidth, G::WindowHeight), ImGuiCond_FirstUseEver);
	if (G::HasSavedPos)
		ImGui::SetNextWindowPos(ImVec2(G::WindowPosX, G::WindowPosY), ImGuiCond_FirstUseEver);

	PushWikiTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);
	ImGui::SetNextWindowBgAlpha(G::Opacity);

	bool open = G::ShowWiki;
	if (!ImGui::Begin("In-Game Helper##GW2InGameHelper", &open))
	{
		WikiBrowser::SetVisible(false);
		ImGui::End();
		ImGui::PopStyleVar();
		PopWikiTheme();
		return;
	}
	if (!open)
	{
		G::ShowWiki = false;
		Settings::SetDirty();
		WikiBrowser::SetVisible(false);
		BlurBrowser();
	}

	ImGui::SetWindowFontScale(G::FontScale);

	BrowserTabs::EnsureDefault();
	BrowserTabs::Tick();

	ImGui::TextColored(kGold, "IN-GAME HELPER");
	ImGui::SameLine(0.f, 12.f);
	DrawToolbar();

	/* Tab / find hotkeys — skip while typing in ImGui fields. */
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool typing = io.WantTextInput;
		const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
		const bool keyF = (GetAsyncKeyState('F') & 0x8000) != 0;
		const bool keyT = (GetAsyncKeyState('T') & 0x8000) != 0;
		const bool keyW = (GetAsyncKeyState('W') & 0x8000) != 0;
		const bool keyTab = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;

		static bool sCtrlFWasDown = false;
		static bool sCtrlTWasDown = false;
		static bool sCtrlWWasDown = false;
		static bool sCtrlTabWasDown = false;

		const bool ctrlF = !typing && ctrl && !shift && !alt && keyF;
		const bool ctrlT = !typing && ctrl && !shift && !alt && keyT;
		const bool ctrlW = !typing && ctrl && !shift && !alt && keyW;
		const bool ctrlShiftT = !typing && ctrl && shift && !alt && keyT;
		const bool ctrlTab = !typing && ctrl && !alt && keyTab;

		if (ctrlF && !sCtrlFWasDown)
			sShowFind = true;
		if (ctrlT && !sCtrlTWasDown)
		{
			sRequestNewTabPicker = true;
			sFocusFilter = true;
		}
		if (ctrlW && !sCtrlWWasDown)
		{
			const int ai = BrowserTabs::ActiveIndex();
			if (BrowserTabs::Count() > 1 && !BrowserTabs::At(ai).pinned)
				BrowserTabs::Close(ai);
		}
		if (ctrlShiftT && !sCtrlTWasDown && BrowserTabs::CanReopenClosed())
			BrowserTabs::ReopenClosed();
		if (ctrlTab && !sCtrlTabWasDown)
		{
			const int n = BrowserTabs::Count();
			if (n > 1)
			{
				int next = BrowserTabs::ActiveIndex() + (shift ? -1 : 1);
				if (next < 0) next = n - 1;
				if (next >= n) next = 0;
				BrowserTabs::Activate(next);
			}
		}

		sCtrlFWasDown = ctrlF;
		sCtrlTWasDown = ctrlT || ctrlShiftT;
		sCtrlWWasDown = ctrlW;
		sCtrlTabWasDown = ctrlTab;

		if (sShowFind && ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			sShowFind = false;
			WikiBrowser::StopFind(true);
		}
	}

	if (sShowFind)
	{
		ImGui::TextColored(kGoldDim, "Find");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220.f);
		const bool findEnter = ImGui::InputTextWithHint("##find_q", "Find in page...", sFindQuery, sizeof(sFindQuery),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		ImGui::Checkbox("Aa", &sFindMatchCase);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Match case");
		ImGui::SameLine();
		if (ImGui::Button("Next") || findEnter)
		{
			if (sFindQuery[0])
				WikiBrowser::Find(sFindQuery, true, sFindMatchCase, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Prev"))
		{
			if (sFindQuery[0])
				WikiBrowser::Find(sFindQuery, false, sFindMatchCase, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			WikiBrowser::StopFind(true);
			sFindQuery[0] = 0;
		}
		ImGui::SameLine();
		const uint32_t fc = WikiBrowser::FindCount();
		const uint32_t fo = WikiBrowser::FindOrdinal();
		if (fc > 0)
			ImGui::TextColored(kGoldMuted, "%u / %u", fo, fc);
		else if (sFindQuery[0])
			ImGui::TextColored(kGoldMuted, "No matches");
	}

	DrawTabBar();

	const char* url = WikiBrowser::CurrentUrlCStr();
	if (url && url[0] && std::strncmp(url, "about:", 6) != 0)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, kGoldMuted);
		ImGui::TextUnformatted(url);
		ImGui::PopStyleColor();
	}

	ImGui::Separator();

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::BeginChild("##wiki_osr_slot", avail, false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavInputs);

	const ImVec2 imageSize = ImGui::GetContentRegionAvail();
	if (G::ShowWiki && imageSize.x > 40.f && imageSize.y > 40.f)
	{
		WikiBrowser::SetVisible(true);
		WikiBrowser::SetBounds(0.f, 0.f, imageSize.x, imageSize.y);
		WikiBrowser::PresentFrame();
	}
	else
		WikiBrowser::SetVisible(false);

	ImGuiIO& io = ImGui::GetIO();
	bool overPage = false;

	if (WikiBrowser::HasFrame())
	{
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::Image(reinterpret_cast<ImTextureID>(WikiBrowser::FrameSrv()), imageSize);

		ImGui::SetCursorScreenPos(cursor);
		ImGui::InvisibleButton("##wiki_hit", imageSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
			ImGuiButtonFlags_MouseButtonMiddle);

		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		overPage = hovered || active;
		const unsigned mods = CefModsFromImGui(io);

		if (overPage)
		{
			const ImVec2 mouse = io.MousePos;
			const float localX = mouse.x - cursor.x;
			const float localY = mouse.y - cursor.y;
			int cx = 0, cy = 0;
			MapToCef(localX, localY, imageSize.x, imageSize.y, &cx, &cy);

			WikiBrowser::FeedMouseMove(cx, cy, false, mods);

			auto click = [&](ImGuiMouseButton btn, int cefBtn) {
				if (ImGui::IsMouseClicked(btn))
				{
					FocusBrowser();
					WikiBrowser::FeedMouseClick(cx, cy, cefBtn, false,
						ImGui::IsMouseDoubleClicked(btn) ? 2 : 1, mods);
				}
				if (ImGui::IsMouseReleased(btn))
					WikiBrowser::FeedMouseClick(cx, cy, cefBtn, true, 1, mods);
			};
			click(ImGuiMouseButton_Left, 0);
			click(ImGuiMouseButton_Right, 2);
			click(ImGuiMouseButton_Middle, 1);

			if (io.MouseWheel != 0.f || io.MouseWheelH != 0.f)
			{
				WikiBrowser::FeedMouseWheel(cx, cy,
					static_cast<int>(io.MouseWheelH * 120.f),
					static_cast<int>(io.MouseWheel * 120.f),
					mods);
			}
		}
	}
	else
	{
		ImGui::Spacing();
		ImGui::TextColored(kGold, WikiBrowser::IsReady()
			? "Waiting for first paint..."
			: "Loading browser...");
		ImGui::TextWrapped("%s", WikiBrowser::Status().c_str());
	}

	ImGui::EndChild();

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 winSize = ImGui::GetWindowSize();
	gWikiMin = pos;
	gWikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
	gWikiRectValid = true;

	const bool mouseOverWiki = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

	gBlockGameMouse = G::ShowWiki && (overPage || mouseOverWiki);
	gBlockGameKeyboard = G::ShowWiki && gBrowserFocused;

	if (gBlockGameMouse)
		io.WantCaptureMouse = true;
	if (gBlockGameKeyboard)
		io.WantCaptureKeyboard = true;

	if (std::fabs(pos.x - G::WindowPosX) > 0.5f || std::fabs(pos.y - G::WindowPosY) > 0.5f ||
		std::fabs(winSize.x - G::WindowWidth) > 0.5f || std::fabs(winSize.y - G::WindowHeight) > 0.5f)
	{
		G::WindowPosX = pos.x;
		G::WindowPosY = pos.y;
		G::WindowWidth = winSize.x;
		G::WindowHeight = winSize.y;
		G::HasSavedPos = true;
		Settings::SetDirty();
	}

	ImGui::SetWindowFontScale(1.f);
	ImGui::End();
	ImGui::PopStyleVar();
	PopWikiTheme();
	Settings::Save(false);
}

void UI_Options()
{
	ImGui::TextUnformatted("GW2 In-Game Helper");
	ImGui::Separator();
	if (ImGui::Checkbox("Show helper window", &G::ShowWiki))
		Settings::SetDirty();

	size_t count = 0;
	Sites::All(&count);
	if (count > 0)
	{
		ImGui::TextUnformatted("Default landing site");
		DrawDefaultSiteBrowse();
		ImGui::TextColored(kMuted, "Home button uses this. Also used when no tabs are saved yet.");
	}

	if (ImGui::SliderFloat("Opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::SliderFloat("Font scale", &G::FontScale, 0.75f, 2.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::Checkbox("Keep browser warm when closed", &G::KeepHelperWarm))
		Settings::SetDirty();
	ImGui::TextColored(kMuted, "Faster reopen; uses more RAM while the helper is hidden.");

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Browse / ... menu for Find / Copy / Open Ext. Right-click tabs to pin. "
		"Window size and position are saved automatically.");
	ImGui::TextWrapped(
		"Hotkeys: Ctrl+Shift+H open/close | Ctrl+T new tab | Ctrl+W close | "
		"Ctrl+Tab cycle | Ctrl+Shift+T reopen | Ctrl+F find");
}
