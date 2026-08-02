#include "UI.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "LivePanels.h"
#include "NotesPad.h"
#include "TpWatchPad.h"
#include "LookupPad.h"
#include "WalletPad.h"
#include "VaultPad.h"
#include "EventsPad.h"
#include "TekkitGuidesPad.h"
#include "CompassOverlay.h"
#include "WorldOverlay.h"
#include "Settings.h"
#include "Sites.h"
#include "SyncQr.h"
#include "WikiBrowser.h"
#include "WikiIpc.h"
#include "AddonPaths.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace
{
	float Clampf(float v, float lo, float hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	/* Saved multi-monitor coords can leave the helper off-screen after a
	   display change — looks like "addon won't open". Pull it back on-screen. */
	bool gForceHelperOnScreen = false;

	bool HelperGeomOffscreen(float dispW, float dispH)
	{
		if (dispW < 100.f || dispH < 100.f)
			return false;
		const float title = 48.f;
		if (G::WindowPosX > dispW - title)
			return true;
		if (G::WindowPosY > dispH - title)
			return true;
		if (G::WindowPosX + G::WindowWidth < title)
			return true;
		if (G::WindowPosY + title < 0.f)
			return true;
		return false;
	}

	void ClampHelperGeomToDisplay()
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float dw = io.DisplaySize.x;
		const float dh = io.DisplaySize.y;
		if (dw < 100.f || dh < 100.f)
			return;

		const float maxW = Clampf(dw * 0.92f, 320.f, dw);
		const float maxH = Clampf(dh * 0.92f, 240.f, dh);
		bool changed = false;
		if (G::WindowWidth > maxW + 0.5f || G::WindowWidth < 320.f)
		{
			G::WindowWidth = Clampf(G::WindowWidth, 320.f, maxW);
			changed = true;
		}
		if (G::WindowHeight > maxH + 0.5f || G::WindowHeight < 240.f)
		{
			G::WindowHeight = Clampf(G::WindowHeight, 240.f, maxH);
			changed = true;
		}

		if (HelperGeomOffscreen(dw, dh) || gForceHelperOnScreen)
		{
			G::WindowPosX = Clampf(dw * 0.08f, 24.f, dw - 120.f);
			G::WindowPosY = Clampf(dh * 0.10f, 24.f, dh - 120.f);
			G::HasSavedPos = true;
			gForceHelperOnScreen = true;
			changed = true;
		}
		else
		{
			const float nx = Clampf(G::WindowPosX, 0.f, dw - 48.f);
			const float ny = Clampf(G::WindowPosY, 0.f, dh - 48.f);
			if (std::fabs(nx - G::WindowPosX) > 0.5f || std::fabs(ny - G::WindowPosY) > 0.5f)
			{
				G::WindowPosX = nx;
				G::WindowPosY = ny;
				changed = true;
			}
		}
		if (changed)
			Settings::SetDirty();
	}

	bool gBrowserFocused = false;
	bool gOverBrowserPage = false; /* pointer over OSR page — keys belong to CEF */
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
		/* Do not clear gOverBrowserPage here — the render loop owns hover state.
		   Clearing it stole CEF focus while keys still reached the page (no caret). */
		WikiBrowser::FeedFocus(false);
	}

	void FocusBrowser()
	{
		if (gBrowserFocused)
			return;
		gBrowserFocused = true;
		WikiBrowser::FeedFocus(true);
	}

	void FocusBrowserForce()
	{
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

	/* NitroPay (gw2efficiency etc.) gates slots with matchMedia min-width up to
	   1840px / 1920px. OSR innerWidth is the ImGui panel, so a normal helper
	   window never unlocks desktop rails. Render those hosts at a desktop-sized
	   CEF view and scale into the panel (MapToCef already maps clicks). */
	bool HostWantsDesktopAdViewport(const char* url)
	{
		if (!url || !url[0])
			return false;
		static const char* kHosts[] = {
			"gw2efficiency.com",
			"snowcrows.com",
			"metabattle.com",
			"guildjen.com",
		};
		for (const char* host : kHosts)
		{
			if (std::strstr(url, host))
				return true;
		}
		return false;
	}

	void DesktopAdCefSize(float /*panelW*/, float panelH, float* outW, float* outH)
	{
		/* Full HD layout width so (min-width: 1840px) / footer / video queries pass. */
		*outW = static_cast<float>(kWikiFrameMaxW);
		float h = panelH;
		if (h < 900.f)
			h = 900.f;
		if (h > static_cast<float>(kWikiFrameMaxH))
			h = static_cast<float>(kWikiFrameMaxH);
		*outH = h;
	}

	static char sFilter[64] = {};
	static int sCategoryIndex = 0;
	static bool sSyncCategory = true;
	static bool sFocusFilter = false;
	static bool sShowFind = false;
	/* Set while the cursor is over one of our popups (Browse / More / tab menu).
	   Avoids ImGuiHoveredFlags_AnyWindow, which also matched Nexus windows. */
	static bool sHelperPopupHovered = false;

	void NoteHelperPopupHover()
	{
		if (ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows))
			sHelperPopupHovered = true;
	}
	static bool sFocusFind = false;
	static bool sRequestNewTabPicker = false;
	static char sFindQuery[128] = {};
	static bool sFindMatchCase = false;

	/* Visual-only Browse section headers (not Sites categories / not hubs). */
	const char* BrowseSection(const char* category, const char* id)
	{
		if (!category || !id || !id[0])
			return nullptr;

		if (std::strcmp(category, "Help") == 0)
		{
			if (std::strcmp(id, "home") == 0 || std::strcmp(id, "dak393_new_player") == 0)
				return "Getting Started";
			if (std::strcmp(id, "gw2official") == 0 || std::strcmp(id, "gw2news") == 0 ||
				std::strcmp(id, "gw2forums") == 0)
				return "ArenaNet";
			if (std::strcmp(id, "raidcore") == 0)
				return "Nexus";
			return "Other";
		}

		if (std::strcmp(category, "Search") == 0)
		{
			if (std::strcmp(id, "gemini") == 0)
				return "AI";
			return "Web Search";
		}
		if (std::strcmp(category, "Live") == 0)
		{
			if (std::strcmp(id, "live_dailies") == 0)
				return "Vault";
			if (std::strcmp(id, "live_news") == 0)
				return "News";
			if (std::strcmp(id, "live_fashion") == 0)
				return "Fashion";
			if (std::strcmp(id, "live_progress") == 0)
				return "Progress";
			return "Other";
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
			if (std::strncmp(id, "gw2app", 6) == 0)
				return "GW2.app";
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
				std::strcmp(id, "mb_raids_hub") == 0 || std::strcmp(id, "mb_intro_raiding") == 0 ||
				std::strncmp(id, "mb_rb_w", 7) == 0 || std::strncmp(id, "gj_w8_", 6) == 0 ||
				std::strcmp(id, "mb_w8_balrior") == 0 ||
				std::strcmp(id, "sc_raids_hub") == 0 || std::strcmp(id, "sc_intro_squads") == 0 ||
				std::strcmp(id, "sc_squad_roles") == 0 || std::strcmp(id, "sc_joining_squads") == 0 ||
				std::strncmp(id, "sc_w1_", 6) == 0 || std::strncmp(id, "sc_w2_", 6) == 0 ||
				std::strncmp(id, "sc_w3_", 6) == 0 || std::strncmp(id, "sc_w4_", 6) == 0 ||
				std::strncmp(id, "sc_w5_", 6) == 0 || std::strncmp(id, "sc_w6_", 6) == 0 ||
				std::strncmp(id, "sc_w7_", 6) == 0)
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
			if (std::strcmp(id, "crafts_hub") == 0 || std::strncmp(id, "crafts_", 7) == 0)
				return "Crafting";
			if (std::strcmp(id, "gw2tldr") == 0 || std::strcmp(id, "gw2tldr_raids") == 0 ||
				std::strcmp(id, "gw2tldr_fractals") == 0 || std::strcmp(id, "gw2tldr_dungeons") == 0)
				return "TLDR";
			if (std::strcmp(id, "fastfarming") == 0)
				return "Farming";
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
				std::strcmp(id, "sc_raid_rev") == 0 || std::strcmp(id, "sc_raid_war") == 0 ||
				std::strcmp(id, "mb_raid_builds") == 0 || std::strcmp(id, "mb_raid_ele") == 0 ||
				std::strcmp(id, "mb_raid_mes") == 0 || std::strcmp(id, "mb_raid_nec") == 0 ||
				std::strcmp(id, "mb_raid_eng") == 0 || std::strcmp(id, "mb_raid_ran") == 0 ||
				std::strcmp(id, "mb_raid_thf") == 0 || std::strcmp(id, "mb_raid_gua") == 0 ||
				std::strcmp(id, "mb_raid_rev") == 0 || std::strcmp(id, "mb_raid_war") == 0)
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

		if (std::strcmp(category, "Wiki") == 0)
		{
			if (std::strcmp(id, "wiki") == 0)
				return "Main";
			if (std::strcmp(id, "wiki_updates") == 0)
				return "News";
			if (std::strcmp(id, "wiki_legendaries") == 0 || std::strcmp(id, "wiki_mounts") == 0)
				return "Collections";
			/* All legendary equipment nests under Legendary Armory. */
			if (std::strcmp(id, "wiki_larmory_hub") == 0 ||
				std::strcmp(id, "wiki_larmor_hub") == 0 || std::strncmp(id, "wiki_larmor_", 12) == 0 ||
				std::strncmp(id, "wiki_laccessory_", 16) == 0 ||
				std::strncmp(id, "wiki_lamulet_", 13) == 0 ||
				std::strncmp(id, "wiki_lring_", 11) == 0 ||
				std::strncmp(id, "wiki_lback_", 11) == 0 ||
				std::strncmp(id, "wiki_lrune_", 11) == 0 ||
				std::strncmp(id, "wiki_lsigil_", 12) == 0 ||
				std::strncmp(id, "wiki_lrelic_", 12) == 0 ||
				std::strcmp(id, "wiki_leg_hub") == 0 || std::strncmp(id, "wiki_leg_", 9) == 0)
				return "Legendary Armory";
			if (std::strcmp(id, "wiki_cosmetic_infusions") == 0 ||
				std::strcmp(id, "gj_infusion_hub") == 0 || std::strncmp(id, "gj_infusion_", 12) == 0)
				return "Cosmetic Infusions";
			if (std::strncmp(id, "wiki_life_", 10) == 0)
				return "Lifestyle";
			if (std::strcmp(id, "wiki_craft_hub") == 0 || std::strncmp(id, "wiki_craft_", 11) == 0)
				return "Crafting";
			if (std::strcmp(id, "wiki_food_hub") == 0 || std::strncmp(id, "wiki_food_", 10) == 0)
				return "Food";
			if (std::strncmp(id, "wiki_util_", 10) == 0)
				return "Utility";
			if (std::strcmp(id, "wiki_mini_hub") == 0 || std::strncmp(id, "wiki_mini_", 10) == 0)
				return "Minis";
			if (std::strcmp(id, "wiki_rune_hub") == 0 || std::strncmp(id, "wiki_rune_", 10) == 0 ||
				std::strcmp(id, "wiki_relic_hub") == 0 || std::strncmp(id, "wiki_relic_", 11) == 0 ||
				std::strcmp(id, "wiki_sigil_hub") == 0 || std::strncmp(id, "wiki_sigil_", 11) == 0)
				return "Upgrades";
			if (std::strcmp(id, "wiki_afood_hub") == 0 || std::strcmp(id, "wiki_afood_gourmet") == 0 ||
				std::strncmp(id, "wiki_afood_", 11) == 0)
				return "Ascended Food";
			if (std::strcmp(id, "wiki_vault_easy") == 0)
				return "Wizards Vault";
			if (std::strcmp(id, "wiki_special_events") == 0 ||
				std::strncmp(id, "wiki_rush_", 10) == 0)
				return "Special Events";
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
			static const char* kSec[] = { "Getting Started", "ArenaNet", "Nexus", "Other" };
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Search") == 0)
		{
			static const char* kSec[] = { "Web Search", "AI" };
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Live") == 0)
		{
			static const char* kSec[] = { "Vault", "News", "Fashion", "Economy", "Progress", "Other" };
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
				"Account", "Overlay", "Timers", "Economy", "Logs / KP", "GW2.app", "Misc", "Other"
			};
			*outCount = sizeof(kSec) / sizeof(kSec[0]);
			return kSec;
		}
		if (std::strcmp(category, "Guides") == 0)
		{
			static const char* kSec[] = {
				"Living World", "Progress", "Mounts", "Fractals", "Raids",
				"Strikes", "Rifts", "PvP", "WvW", "Achievements",
				"Jumping Puzzles", "Crafting", "TLDR", "Farming", "Other"
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
		if (std::strcmp(category, "Wiki") == 0)
		{
			static const char* kSec[] = {
				"Main", "News", "Special Events", "Collections", "Legendary Armory",
				"Cosmetic Infusions", "Lifestyle", "Crafting", "Food", "Ascended Food",
				"Utility", "Minis", "Upgrades", "Wizards Vault", "Other"
			};
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
		if (std::strncmp(id, "sc_w1_", 6) == 0 || std::strcmp(id, "mb_rb_w1") == 0)
			return "W1 Spirit Vale";
		if (std::strncmp(id, "sc_w2_", 6) == 0 || std::strcmp(id, "mb_rb_w2") == 0)
			return "W2 Salvation Pass";
		if (std::strncmp(id, "sc_w3_", 6) == 0 || std::strcmp(id, "mb_rb_w3") == 0)
			return "W3 Stronghold";
		if (std::strncmp(id, "sc_w4_", 6) == 0 || std::strcmp(id, "mb_rb_w4") == 0)
			return "W4 Bastion";
		if (std::strncmp(id, "sc_w5_", 6) == 0 || std::strcmp(id, "mb_rb_w5") == 0)
			return "W5 Hall of Chains";
		if (std::strncmp(id, "sc_w6_", 6) == 0 || std::strcmp(id, "mb_rb_w6") == 0)
			return "W6 Mythwright";
		if (std::strncmp(id, "sc_w7_", 6) == 0 || std::strcmp(id, "mb_rb_w7") == 0)
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
			std::strncmp(id, "sc_w7_", 6) == 0 ||
			std::strcmp(id, "mb_raids_hub") == 0 || std::strcmp(id, "mb_intro_raiding") == 0 ||
			std::strncmp(id, "mb_rb_w", 7) == 0 || std::strncmp(id, "gj_w8_", 6) == 0 ||
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
		if (std::strncmp(id, "gj_ach_side_", 12) == 0)
			return "Side Stories";
		return nullptr;
	}

	/* Acquisition subsection under Wiki → Cosmetic Infusions (nullptr = hub). */
	const char* InfusionSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "wiki_cosmetic_infusions") == 0 ||
			std::strcmp(id, "gj_infusion_hub") == 0)
			return nullptr;
		if (std::strncmp(id, "gj_infusion_vault_", 18) == 0)
			return "Wizard's Vault";
		if (std::strncmp(id, "gj_infusion_forge_", 18) == 0)
			return "Mystic Forge";
		if (std::strncmp(id, "gj_infusion_ow_", 15) == 0)
			return "Open World";
		if (std::strncmp(id, "gj_infusion_inst_", 17) == 0)
			return "Instanced";
		if (std::strncmp(id, "gj_infusion_fest_", 17) == 0)
			return "Festival";
		if (std::strncmp(id, "gj_infusion_wvw_", 16) == 0)
			return "WvW";
		return nullptr;
	}

	/* Subsection under Wiki → Legendary Armory (nullptr = armory hub).
	   Exclusive id prefixes (do not overlap with Upgrades wiki_relic_/wiki_rune_/wiki_sigil_):
	     wiki_larmory_  wiki_larmor_  wiki_leg_  wiki_laccessory_
	     wiki_lamulet_  wiki_lring_   wiki_lback_ wiki_lrune_
	     wiki_lsigil_   wiki_lrelic_  */
	const char* LegendaryArmorySub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "wiki_larmory_hub") == 0)
			return nullptr;
		if (std::strcmp(id, "wiki_larmor_hub") == 0 || std::strncmp(id, "wiki_larmor_", 12) == 0)
			return "Legendary Armor";
		if (std::strcmp(id, "wiki_leg_hub") == 0 || std::strncmp(id, "wiki_leg_", 9) == 0)
			return "Legendary Weapons";
		if (std::strncmp(id, "wiki_laccessory_", 16) == 0)
			return "Legendary Accessory";
		if (std::strncmp(id, "wiki_lamulet_", 13) == 0)
			return "Legendary Amulet";
		if (std::strncmp(id, "wiki_lring_", 11) == 0)
			return "Legendary Rings";
		if (std::strncmp(id, "wiki_lback_", 11) == 0)
			return "Legendary Back Items";
		if (std::strncmp(id, "wiki_lrune_", 11) == 0 ||
			std::strncmp(id, "wiki_lsigil_", 12) == 0 ||
			std::strncmp(id, "wiki_lrelic_", 12) == 0)
			return "Legendary Upgrade Components";
		return nullptr;
	}

	/* Generation subsection under Wiki → Legendary Armory → Legendary Weapons (nullptr = hub). */
	const char* LegendaryWeaponSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "wiki_leg_hub") == 0)
			return nullptr;
		if (std::strncmp(id, "wiki_leg_g3v_", 13) == 0)
			return "Generation 3 Variants";
		if (std::strncmp(id, "wiki_leg_g1_", 12) == 0)
			return "Generation 1";
		if (std::strncmp(id, "wiki_leg_g2_", 12) == 0)
			return "Generation 2";
		if (std::strncmp(id, "wiki_leg_g3_", 12) == 0)
			return "Generation 3";
		if (std::strncmp(id, "wiki_leg_g4_", 12) == 0)
			return "Generation 4";
		return nullptr;
	}

	/* Dragon set under Wiki → Legendary Weapons → Generation 3 Variants. */
	const char* Gen3VariantDragon(const char* id)
	{
		if (!id || std::strncmp(id, "wiki_leg_g3v_", 13) != 0)
			return nullptr;
		if (std::strncmp(id, "wiki_leg_g3v_hub_", 17) == 0 ||
			std::strncmp(id, "wiki_leg_g3v_facet_", 19) == 0)
			return nullptr; /* overview rows drawn before dragon subs */
		if (std::strncmp(id, "wiki_leg_g3v_zhaitan_", 21) == 0)
			return "Zhaitan";
		if (std::strncmp(id, "wiki_leg_g3v_mordremoth_", 24) == 0)
			return "Mordremoth";
		if (std::strncmp(id, "wiki_leg_g3v_kralkatorrik_", 26) == 0)
			return "Kralkatorrik";
		if (std::strncmp(id, "wiki_leg_g3v_jormag_", 20) == 0)
			return "Jormag";
		if (std::strncmp(id, "wiki_leg_g3v_primordus_", 23) == 0)
			return "Primordus";
		if (std::strncmp(id, "wiki_leg_g3v_soo_won_", 21) == 0)
			return "Soo-Won";
		return nullptr;
	}

	/* Subsection under Wiki → Upgrades. */
	const char* UpgradesSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "wiki_rune_hub") == 0 || std::strncmp(id, "wiki_rune_", 10) == 0)
			return "Superior Runes";
		if (std::strcmp(id, "wiki_relic_hub") == 0 || std::strncmp(id, "wiki_relic_", 11) == 0)
			return "Relics";
		if (std::strcmp(id, "wiki_sigil_hub") == 0 || std::strncmp(id, "wiki_sigil_", 11) == 0)
			return "Superior Sigils";
		return nullptr;
	}

	/* Subsection under Wiki → Crafting (nullptr = hub). */
	const char* WikiCraftingSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "wiki_craft_hub") == 0)
			return nullptr;
		if (std::strncmp(id, "wiki_craft_disc_", 16) == 0)
			return "Disciplines";
		if (std::strncmp(id, "wiki_craft_rel_", 15) == 0)
			return "Related";
		return nullptr;
	}

	/* Attribute subsection under Wiki → Food / Ascended Food / Utility (nullptr = hub). */
	const char* FoodAttrSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "wiki_food_hub") == 0 ||
			std::strcmp(id, "wiki_afood_hub") == 0 ||
			std::strcmp(id, "wiki_afood_gourmet") == 0 ||
			std::strcmp(id, "wiki_util_hub") == 0 ||
			std::strcmp(id, "wiki_util_list") == 0 ||
			std::strcmp(id, "wiki_util_enhancement") == 0 ||
			std::strcmp(id, "wiki_util_slayer") == 0 ||
			std::strcmp(id, "wiki_util_lw_exp") == 0 ||
			std::strcmp(id, "wiki_util_festival") == 0)
			return nullptr;
		const char* p = nullptr;
		if (std::strncmp(id, "wiki_food_", 10) == 0)
			p = id + 10;
		else if (std::strncmp(id, "wiki_afood_", 11) == 0)
			p = id + 11;
		else if (std::strncmp(id, "wiki_util_", 10) == 0)
			p = id + 10;
		else
			return nullptr;
		if (std::strncmp(p, "power_", 6) == 0)
			return "Power";
		if (std::strncmp(p, "precision_", 10) == 0)
			return "Precision";
		if (std::strncmp(p, "toughness_", 10) == 0)
			return "Toughness";
		if (std::strncmp(p, "vitality_", 9) == 0)
			return "Vitality";
		if (std::strncmp(p, "concentration_", 14) == 0)
			return "Concentration";
		if (std::strncmp(p, "condition_damage_", 17) == 0)
			return "Condition Damage";
		if (std::strncmp(p, "expertise_", 10) == 0)
			return "Expertise";
		if (std::strncmp(p, "ferocity_", 9) == 0)
			return "Ferocity";
		if (std::strncmp(p, "healing_power_", 14) == 0)
			return "Healing Power";
		if (std::strncmp(p, "all_attributes_", 15) == 0)
			return "All Attributes";
		if (std::strncmp(p, "other_", 6) == 0)
			return "Other";
		return nullptr;
	}

	/* Subsection under Wiki → Minis (nullptr = hub). */
	const char* MinisSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "wiki_mini_hub") == 0)
			return nullptr;
		if (std::strncmp(id, "wiki_mini_", 10) != 0)
			return nullptr;
		const char* p = id + 10;
		if (std::strncmp(p, "sets_", 5) == 0)
			return "Sets 1 to 3";
		if (std::strncmp(p, "core_", 5) == 0)
			return "Core";
		if (std::strncmp(p, "pvp_", 4) == 0)
			return "PvP";
		if (std::strncmp(p, "wvw_", 4) == 0)
			return "WvW";
		if (std::strncmp(p, "hot_", 4) == 0)
			return "Heart of Thorns";
		if (std::strncmp(p, "raids_", 6) == 0)
			return "Raids";
		if (std::strncmp(p, "lws3_", 5) == 0)
			return "Living World Season 3";
		if (std::strncmp(p, "pof_", 4) == 0)
			return "Path of Fire";
		if (std::strncmp(p, "lws4_", 5) == 0)
			return "Living World Season 4";
		if (std::strncmp(p, "ibs_", 4) == 0)
			return "The Icebrood Saga";
		if (std::strncmp(p, "eod_", 4) == 0)
			return "End of Dragons";
		if (std::strncmp(p, "soto_", 5) == 0)
			return "Secrets of the Obscure";
		if (std::strncmp(p, "jw_", 3) == 0)
			return "Janthir Wilds";
		if (std::strncmp(p, "voe_", 4) == 0)
			return "Visions of Eternity";
		if (std::strncmp(p, "fest_lny_", 9) == 0)
			return "Festival Minis - Lunar New Year";
		if (std::strncmp(p, "fest_sab_", 9) == 0)
			return "Festival Minis - Super Adventure Box";
		if (std::strncmp(p, "fest_db_", 8) == 0)
			return "Festival Minis - Dragon Bash";
		if (std::strncmp(p, "fest_ffw_", 9) == 0)
			return "Festival Minis - Festival of the Four Winds";
		if (std::strncmp(p, "fest_hw_", 8) == 0)
			return "Festival Minis - Halloween";
		if (std::strncmp(p, "fest_ws_", 8) == 0)
			return "Festival Minis - Wintersday";
		if (std::strncmp(p, "gem_", 4) == 0)
			return "Gem Store/Black Lion";
		if (std::strncmp(p, "promo_", 6) == 0)
			return "Promotional Minis";
		if (std::strncmp(p, "unavailable_", 12) == 0)
			return "Unavailable";
		return nullptr;
	}

	/* Subsection under Guides → Crafting (nullptr = hub). */
	const char* GuidesCraftingSub(const char* id)
	{
		if (!id || !id[0])
			return nullptr;
		if (std::strcmp(id, "crafts_hub") == 0)
			return nullptr;
		if (std::strncmp(id, "crafts_n_", 9) == 0)
			return "Normal Guides";
		if (std::strncmp(id, "crafts_f_", 9) == 0)
			return "Fast Guides";
		if (std::strncmp(id, "crafts_400_", 11) == 0)
			return "400-500";
		if (std::strncmp(id, "crafts_s_", 9) == 0)
			return "Special";
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
		const bool pressed = ImGui::InvisibleButton("##gw2igh_star", size);
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

	/* Browse popup sized from the display — comfortable on 1080p, a bit roomier on 4K. */
	struct BrowsePopupLayout
	{
		float width;
		float height;
		float listH;
		float leftW;
	};

	BrowsePopupLayout CalcBrowsePopupLayout(bool withBanner, bool pickDefaultSite)
	{
		const ImGuiIO& io = ImGui::GetIO();
		const ImGuiStyle& st = ImGui::GetStyle();
		const float dispW = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x : 800.f;
		const float dispH = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y : 600.f;

		static float sCacheDispW = -1.f;
		static float sCacheDispH = -1.f;
		static float sCacheFont = -1.f;
		static bool sCacheBanner = false;
		static bool sCacheDefault = false;
		static BrowsePopupLayout sCache{};

		const float fontScale = ImGui::GetFontSize();
		if (sCacheDispW == dispW && sCacheDispH == dispH && sCacheFont == fontScale &&
			sCacheBanner == withBanner && sCacheDefault == pickDefaultSite)
			return sCache;

		/* Compact on 1080p (~540×390), a bit wider on 1440p/4K — never half the screen. */
		const float width = Clampf(dispW * 0.28f, 480.f, 680.f);
		const float maxOuter = Clampf(dispH * 0.36f, 300.f, 480.f);
		const float listMax = pickDefaultSite
			? Clampf(dispH * 0.20f, 160.f, 260.f)
			: Clampf(dispH * 0.24f, 180.f, 300.f);

		float chrome = st.WindowPadding.y * 2.f;
		if (withBanner)
			chrome += ImGui::GetTextLineHeightWithSpacing() * 2.f;
		chrome += ImGui::GetFrameHeightWithSpacing(); /* Search + filter */
		chrome += st.ItemSpacing.y; /* Spacing() */
		chrome += 1.f;             /* Separator */
		chrome += st.ItemSpacing.y;
		chrome += ImGui::GetTextLineHeightWithSpacing(); /* Created by */
		chrome += ImGui::GetTextLineHeight();            /* IGN | Discord */
		chrome += 8.f;

		const float listH = Clampf(maxOuter - chrome, 160.f, listMax);
		BrowsePopupLayout lay{};
		lay.width = width;
		lay.height = chrome + listH;
		lay.listH = listH;
		lay.leftW = Clampf(width * 0.26f, 140.f, 180.f);

		sCacheDispW = dispW;
		sCacheDispH = dispH;
		sCacheFont = fontScale;
		sCacheBanner = withBanner;
		sCacheDefault = pickDefaultSite;
		sCache = lay;
		return lay;
	}

	/* Dropdown-style picker: pin under the button that opened it (not a free window). */
	static constexpr ImGuiWindowFlags kBrowsePopupFlags =
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	ImVec2 CaptureAnchorBelowItem()
	{
		return ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2.f);
	}

	void PrepareBrowsePopup(ImVec2 anchor, const BrowsePopupLayout& lay)
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImVec2 pos = anchor;
		if (pos.x + lay.width > io.DisplaySize.x - 8.f)
			pos.x = Clampf(io.DisplaySize.x - lay.width - 8.f, 8.f, io.DisplaySize.x);
		if (pos.x < 8.f)
			pos.x = 8.f;
		/* Flip above the button when there isn't room below. */
		if (pos.y + lay.height > io.DisplaySize.y - 8.f)
			pos.y = Clampf(anchor.y - lay.height - ImGui::GetFrameHeight() - 6.f, 8.f, io.DisplaySize.y);

		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(lay.width, lay.height), ImGuiCond_Always);
	}

	ImVec2 sBrowseAnchor{};
	ImVec2 sNewTabBrowseAnchor{};
	ImVec2 sDefaultSiteBrowseAnchor{};

	void DrawBrowsePanelContents(bool navigateOnChange, bool* closePanel, bool pickDefaultSite = false, bool pickNewTab = false, float listHArg = -1.f, float leftWArg = -1.f)
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
		/* ### keeps a unique ID in the shared Nexus ImGui context without a visible label. */
		ImGui::InputTextWithHint("###gw2igh_site_filter", "Filter sites...", sFilter, sizeof(sFilter));

		const bool filtering = sFilter[0] != '\0';
		const bool showFavorites = (!filtering && !pickDefaultSite && sCategoryIndex == 0);
		const char* selectedCat = "";
		if (!filtering && !showFavorites)
		{
			const int catIdx = pickDefaultSite ? sCategoryIndex : (sCategoryIndex - 1);
			if (catIdx >= 0 && catIdx < static_cast<int>(catCount))
				selectedCat = cats[catIdx] ? cats[catIdx] : "";
		}

		const float listH = (listHArg > 0.f) ? listHArg : (pickDefaultSite ? 300.f : 320.f);
		const float leftW = (leftWArg > 0.f) ? leftWArg : 172.f;

		ImGui::BeginChild("##gw2igh_browse_cats", ImVec2(leftW, listH), true);
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

		ImGui::BeginChild("##gw2igh_browse_sites", ImVec2(0.f, listH), true);
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

		/* Cache site indices for the selected category (Wiki alone is 1000+). */
		static std::string sBrowseCatKey;
		static std::vector<int> sBrowseCatIdx;
		static std::string sSecBucketKey; /* invalidated with cat idx below */
		static std::vector<std::vector<int>> sSecBuckets;
		bool browseCatRebuilt = false;
		{
			const char* key = filtering ? "\x01" "filter" : (showFavorites ? "\x01" "fav" : selectedCat);
			if (sBrowseCatKey != key)
			{
				sBrowseCatKey = key;
				sBrowseCatIdx.clear();
				sSecBucketKey.clear(); /* force section re-bucket with fresh indices */
				browseCatRebuilt = true;
				if (!filtering && !showFavorites && selectedCat && selectedCat[0])
				{
					sBrowseCatIdx.reserve(512);
					for (int i = 0; i < static_cast<int>(siteCount); ++i)
					{
						const char* cat = sites[i].category ? sites[i].category : "";
						if (std::strcmp(cat, selectedCat) == 0)
							sBrowseCatIdx.push_back(i);
					}
				}
			}
		}

		auto DrawSiteRow = [&](int siteIndex, bool withCategoryPrefix) {
			if (siteIndex < 0 || siteIndex >= static_cast<int>(siteCount))
				return;
			const SiteDef& site = sites[siteIndex];
			ImGui::PushID(siteIndex);
			/* Keep star+label on one row without SameLine edge cases that can
			   leave ListClipper with ItemsHeight==0 under nested headers. */
			const float rowStartY = ImGui::GetCursorPosY();
			if (!pickDefaultSite)
			{
				DrawFavoriteStar(site.id);
				ImGui::SameLine(0.f, 4.f);
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
				std::snprintf(row, sizeof(row), "%s", site.label ? site.label : site.id ? site.id : "(site)");

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
			/* Guarantee the row advanced — empty labels / SameLine quirks must
			   not leave the cursor stuck (breaks clipper height measure). */
			if (ImGui::GetCursorPosY() <= rowStartY + 0.5f)
				ImGui::SetCursorPosY(rowStartY + ImGui::GetFrameHeightWithSpacing());
			if (ImGui::IsItemHovered())
			{
				if (pickNewTab)
					ImGui::SetTooltip("Open in a new tab");
				else if (!pickDefaultSite)
				{
					if (site.title && site.title[0] && site.label &&
						std::strcmp(site.title, site.label) != 0)
					{
						char tip[192];
						SanitizeForUi(tip, sizeof(tip), site.title);
						ImGui::SetTooltip("%s\nClick: this tab | Ctrl+click: new tab", tip);
					}
					else
						ImGui::SetTooltip("Click: this tab | Ctrl+click: new tab");
				}
				else if (site.title && site.title[0])
				{
					char tip[160];
					SanitizeForUi(tip, sizeof(tip), site.title);
					ImGui::SetTooltip("%s", tip);
				}
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

		/* Even-height rows + only submit visible ones (Browse lists can be 1000+).
		   Always pass an explicit row height. Auto-measure + favorite-star SameLine
		   under nested CollapsingHeaders can yield ItemsHeight==0 (assert-only in
		   ImGui 1.80), which then seeks by zero and the expanded section looks empty. */
		auto DrawClippedRows = [&](const std::vector<int>& idxs, bool withCategoryPrefix) {
			if (idxs.empty())
				return;
			const int n = static_cast<int>(idxs.size());
			const float rowH = ImGui::GetFrameHeightWithSpacing();
			if (n <= 96)
			{
				for (int i = 0; i < n; ++i)
					DrawSiteRow(idxs[static_cast<size_t>(i)], withCategoryPrefix);
				return;
			}
			ImGuiListClipper clipper;
			clipper.Begin(n, rowH);
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
					DrawSiteRow(idxs[static_cast<size_t>(i)], withCategoryPrefix);
			}
		};

		if (showFavorites)
		{
			static unsigned sFavGen = 0;
			static std::vector<int> sFavIdx;
			const unsigned gen = Sites::FavoritesGeneration();
			if (sFavGen != gen)
			{
				sFavGen = gen;
				sFavIdx.clear();
				const int favN = Sites::FavoriteCount();
				sFavIdx.reserve(static_cast<size_t>(favN));
				for (int f = 0; f < favN; ++f)
				{
					const int si = Sites::FavoriteSiteIndex(f);
					if (si >= 0)
						sFavIdx.push_back(si);
				}
			}
			DrawClippedRows(sFavIdx, true);
		}
		else if (filtering)
		{
			static char sFilterCache[128]{};
			static std::vector<int> sFilterMatches;
			if (std::strcmp(sFilterCache, sFilter) != 0)
			{
				std::snprintf(sFilterCache, sizeof(sFilterCache), "%s", sFilter);
				sFilterMatches.clear();
				sFilterMatches.reserve(64);
				for (int i = 0; i < static_cast<int>(siteCount); ++i)
				{
					if (!Sites::MatchesFilter(sites[i], sFilter))
						continue;
					sFilterMatches.push_back(i);
				}
			}
			DrawClippedRows(sFilterMatches, true);
			if (!sFilterMatches.empty())
			{
				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
				ImGui::Text("%d match%s", static_cast<int>(sFilterMatches.size()),
					sFilterMatches.size() == 1 ? "" : "es");
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
				/* One pass: bucket indices by top-level section (stable until category changes). */
				if (browseCatRebuilt || sSecBucketKey != selectedCat)
				{
					sSecBucketKey = selectedCat ? selectedCat : "";
					sSecBuckets.assign(secCount, {});
					for (int i : sBrowseCatIdx)
					{
						const char* sec = BrowseSection(selectedCat, sites[i].id);
						if (!sec)
							continue;
						for (size_t s = 0; s < secCount; ++s)
						{
							if (std::strcmp(sec, sections[s]) == 0)
							{
								sSecBuckets[s].push_back(i);
								break;
							}
						}
					}
				}
				/* Guard: section table size can never shrink under us, but don't read OOB. */
				if (sSecBuckets.size() < secCount)
					sSecBuckets.resize(secCount);
				for (size_t s = 0; s < secCount; ++s)
				{
					const char* section = sections[s];
					const std::vector<int>& secIdx = sSecBuckets[s];
					const int secSites = static_cast<int>(secIdx.size());
					if (secSites == 0)
						continue;
					anyInCategory = true;
					if (!BeginBrowseSection(selectedCat, section, secSites))
						continue;
					/* Guides → Raids only: nest Raid Wings / Raid Boss.
					   Builds → Raids is a flat SC + MetaBattle profession list — do not
					   reuse RaidsSub or the section expands empty (count 10, body 0). */
					if (std::strcmp(section, "Raids") == 0 &&
						std::strcmp(selectedCat, "Guides") == 0)
					{
						static const char* kRaidSubs[] = { "Raid Wings", "Raid Boss" };
						static const char* kWings[] = {
							"W1 Spirit Vale", "W2 Salvation Pass", "W3 Stronghold", "W4 Bastion",
							"W5 Hall of Chains", "W6 Mythwright", "W7 Ahdashim", "W8 Mount Balrior"
						};
						constexpr int kRaidSubN = 2;
						constexpr int kWingN = static_cast<int>(sizeof(kWings) / sizeof(kWings[0]));
						static std::string sRaidCacheKey;
						static std::vector<int> sRaidWings;
						static std::vector<int> sRaidBossHubs;
						static std::vector<std::vector<int>> sRaidBossByWing;
						if (sRaidCacheKey != selectedCat || browseCatRebuilt)
						{
							sRaidCacheKey = selectedCat;
							sRaidWings.clear();
							sRaidBossHubs.clear();
							sRaidBossByWing.assign(static_cast<size_t>(kWingN), {});
							for (int i : sBrowseCatIdx)
							{
								const char* s = RaidsSub(sites[i].id);
								if (!s)
									continue;
								if (std::strcmp(s, "Raid Wings") == 0)
								{
									sRaidWings.push_back(i);
									continue;
								}
								if (std::strcmp(s, "Raid Boss") != 0)
									continue;
								const char* w = RaidBossWing(sites[i].id);
								if (!w)
								{
									sRaidBossHubs.push_back(i);
									continue;
								}
								bool placed = false;
								for (int wi = 0; wi < kWingN; ++wi)
								{
									if (std::strcmp(w, kWings[wi]) == 0)
									{
										sRaidBossByWing[static_cast<size_t>(wi)].push_back(i);
										placed = true;
										break;
									}
								}
								if (!placed)
									sRaidBossHubs.push_back(i);
							}
						}
						ImGui::Indent(10.f);
						for (int rsi = 0; rsi < kRaidSubN; ++rsi)
						{
							const char* sub = kRaidSubs[rsi];
							if (std::strcmp(sub, "Raid Wings") == 0)
							{
								if (sRaidWings.empty())
									continue;
								if (!BeginBrowseSection("Raids", sub, static_cast<int>(sRaidWings.size())))
									continue;
								DrawClippedRows(sRaidWings, false);
								continue;
							}
							const int bossCount = static_cast<int>(sRaidBossHubs.size()) +
								[&]() {
									int n = 0;
									for (const auto& v : sRaidBossByWing)
										n += static_cast<int>(v.size());
									return n;
								}();
							if (bossCount == 0)
								continue;
							if (!BeginBrowseSection("Raids", sub, bossCount))
								continue;
							DrawClippedRows(sRaidBossHubs, false);
							ImGui::Indent(10.f);
							for (int wi = 0; wi < kWingN; ++wi)
							{
								if (sRaidBossByWing[static_cast<size_t>(wi)].empty())
									continue;
								if (!BeginBrowseSection("Raid Boss", kWings[wi],
										static_cast<int>(sRaidBossByWing[static_cast<size_t>(wi)].size())))
									continue;
								DrawClippedRows(sRaidBossByWing[static_cast<size_t>(wi)], false);
							}
							ImGui::Unindent(10.f);
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Legendary Armory") == 0)
					{
						static const char* kArmorySubs[] = {
							"Legendary Armor", "Legendary Weapons", "Legendary Accessory",
							"Legendary Amulet", "Legendary Rings", "Legendary Back Items",
							"Legendary Upgrade Components"
						};
						static const char* kLegSubs[] = {
							"Generation 1", "Generation 2", "Generation 3",
							"Generation 3 Variants", "Generation 4"
						};
						static const char* kDragons[] = {
							"Zhaitan", "Mordremoth", "Kralkatorrik",
							"Jormag", "Primordus", "Soo-Won"
						};
						constexpr int kArmN = static_cast<int>(sizeof(kArmorySubs) / sizeof(kArmorySubs[0]));
						constexpr int kLegN = static_cast<int>(sizeof(kLegSubs) / sizeof(kLegSubs[0]));
						constexpr int kDragonN = static_cast<int>(sizeof(kDragons) / sizeof(kDragons[0]));

						static std::string sArmCacheKey;
						static std::vector<int> sArmHubs;
						static std::vector<std::vector<int>> sArmBySub;
						static std::vector<int> sLegHubs;
						static std::vector<std::vector<int>> sLegByGen;
						static std::vector<int> sG3vHubs;
						static std::vector<std::vector<int>> sG3vByDragon;
						if (sArmCacheKey != selectedCat || browseCatRebuilt)
						{
							sArmCacheKey = selectedCat;
							sArmHubs.clear();
							sArmBySub.assign(static_cast<size_t>(kArmN), {});
							sLegHubs.clear();
							sLegByGen.assign(static_cast<size_t>(kLegN), {});
							sG3vHubs.clear();
							sG3vByDragon.assign(static_cast<size_t>(kDragonN), {});
							for (int i : secIdx)
							{
								const char* arm = LegendaryArmorySub(sites[i].id);
								if (!arm)
								{
									sArmHubs.push_back(i);
									continue;
								}
								int ai = -1;
								for (int a = 0; a < kArmN; ++a)
								{
									if (std::strcmp(arm, kArmorySubs[a]) == 0)
									{
										ai = a;
										break;
									}
								}
								if (ai < 0)
								{
									/* Unknown armory bucket — keep visible at hub level. */
									sArmHubs.push_back(i);
									continue;
								}
								sArmBySub[static_cast<size_t>(ai)].push_back(i);
								if (std::strcmp(arm, "Legendary Weapons") != 0)
									continue;
								const char* gen = LegendaryWeaponSub(sites[i].id);
								if (!gen)
								{
									sLegHubs.push_back(i);
									continue;
								}
								int gi = -1;
								for (int g = 0; g < kLegN; ++g)
								{
									if (std::strcmp(gen, kLegSubs[g]) == 0)
									{
										gi = g;
										break;
									}
								}
								if (gi < 0)
									continue;
								sLegByGen[static_cast<size_t>(gi)].push_back(i);
								if (std::strcmp(gen, "Generation 3 Variants") != 0)
									continue;
								const char* d = Gen3VariantDragon(sites[i].id);
								if (!d)
								{
									sG3vHubs.push_back(i);
									continue;
								}
								for (int di = 0; di < kDragonN; ++di)
								{
									if (std::strcmp(d, kDragons[di]) == 0)
									{
										sG3vByDragon[static_cast<size_t>(di)].push_back(i);
										break;
									}
								}
							}
						}

						DrawClippedRows(sArmHubs, false);
						ImGui::Indent(10.f);
						for (int ai = 0; ai < kArmN; ++ai)
						{
							const std::vector<int>& armIdx = sArmBySub[static_cast<size_t>(ai)];
							if (armIdx.empty())
								continue;
							if (!BeginBrowseSection("Legendary Armory", kArmorySubs[ai],
									static_cast<int>(armIdx.size())))
								continue;

							if (std::strcmp(kArmorySubs[ai], "Legendary Weapons") == 0)
							{
								DrawClippedRows(sLegHubs, false);
								ImGui::Indent(10.f);
								for (int gi = 0; gi < kLegN; ++gi)
								{
									const std::vector<int>& genIdx = sLegByGen[static_cast<size_t>(gi)];
									if (genIdx.empty())
										continue;
									if (!BeginBrowseSection("Legendary Weapons", kLegSubs[gi],
											static_cast<int>(genIdx.size())))
										continue;

									if (std::strcmp(kLegSubs[gi], "Generation 3 Variants") == 0)
									{
										DrawClippedRows(sG3vHubs, false);
										ImGui::Indent(10.f);
										for (int di = 0; di < kDragonN; ++di)
										{
											const std::vector<int>& dIdx =
												sG3vByDragon[static_cast<size_t>(di)];
											if (dIdx.empty())
												continue;
											if (!BeginBrowseSection("Generation 3 Variants", kDragons[di],
													static_cast<int>(dIdx.size())))
												continue;
											DrawClippedRows(dIdx, false);
										}
										ImGui::Unindent(10.f);
										continue;
									}

									DrawClippedRows(genIdx, false);
								}
								ImGui::Unindent(10.f);
								continue;
							}

							DrawClippedRows(armIdx, false);
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Cosmetic Infusions") == 0)
					{
						static const char* kInfSubs[] = {
							"Wizard's Vault", "Mystic Forge", "Open World",
							"Instanced", "Festival", "WvW"
						};
						for (int i : sBrowseCatIdx)
						{
							const SiteDef& site = sites[i];
							const char* sec = BrowseSection(selectedCat, site.id);
							if (!sec || std::strcmp(sec, "Cosmetic Infusions") != 0)
								continue;
							if (InfusionSub(site.id))
								continue;
							DrawSiteRow(i, false);
						}
						ImGui::Indent(10.f);
						for (const char* sub : kInfSubs)
						{
							int subCount = 0;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = InfusionSub(site.id);
								if (s && std::strcmp(s, sub) == 0)
									++subCount;
							}
							if (subCount == 0)
								continue;
							if (!BeginBrowseSection("Cosmetic Infusions", sub, subCount))
								continue;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = InfusionSub(site.id);
								if (!s || std::strcmp(s, sub) != 0)
									continue;
								DrawSiteRow(i, false);
							}
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Crafting") == 0 &&
						std::strcmp(selectedCat, "Wiki") == 0)
					{
						static const char* kCraftSubs[] = { "Disciplines", "Related" };
						for (int i : sBrowseCatIdx)
						{
							const SiteDef& site = sites[i];
							const char* sec = BrowseSection(selectedCat, site.id);
							if (!sec || std::strcmp(sec, "Crafting") != 0)
								continue;
							if (WikiCraftingSub(site.id))
								continue;
							DrawSiteRow(i, false);
						}
						ImGui::Indent(10.f);
						for (const char* sub : kCraftSubs)
						{
							int subCount = 0;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = WikiCraftingSub(site.id);
								if (s && std::strcmp(s, sub) == 0)
									++subCount;
							}
							if (subCount == 0)
								continue;
							if (!BeginBrowseSection("Crafting", sub, subCount))
								continue;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = WikiCraftingSub(site.id);
								if (!s || std::strcmp(s, sub) != 0)
									continue;
								DrawSiteRow(i, false);
							}
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Crafting") == 0 &&
						std::strcmp(selectedCat, "Guides") == 0)
					{
						static const char* kCraftSubs[] = {
							"Normal Guides", "Fast Guides", "400-500", "Special"
						};
						for (int i : sBrowseCatIdx)
						{
							const SiteDef& site = sites[i];
							const char* sec = BrowseSection(selectedCat, site.id);
							if (!sec || std::strcmp(sec, "Crafting") != 0)
								continue;
							if (GuidesCraftingSub(site.id))
								continue;
							DrawSiteRow(i, false);
						}
						ImGui::Indent(10.f);
						for (const char* sub : kCraftSubs)
						{
							int subCount = 0;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = GuidesCraftingSub(site.id);
								if (s && std::strcmp(s, sub) == 0)
									++subCount;
							}
							if (subCount == 0)
								continue;
							if (!BeginBrowseSection("Crafting", sub, subCount))
								continue;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = GuidesCraftingSub(site.id);
								if (!s || std::strcmp(s, sub) != 0)
									continue;
								DrawSiteRow(i, false);
							}
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Upgrades") == 0)
					{
						static const char* kUpgSubs[] = {
							"Superior Runes", "Relics", "Superior Sigils"
						};
						ImGui::Indent(10.f);
						for (const char* sub : kUpgSubs)
						{
							int subCount = 0;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = UpgradesSub(site.id);
								if (s && std::strcmp(s, sub) == 0)
									++subCount;
							}
							if (subCount == 0)
								continue;
							if (!BeginBrowseSection("Upgrades", sub, subCount))
								continue;
							for (int i : sBrowseCatIdx)
							{
								const SiteDef& site = sites[i];
								const char* s = UpgradesSub(site.id);
								if (!s || std::strcmp(s, sub) != 0)
									continue;
								DrawSiteRow(i, false);
							}
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Food") == 0 ||
						std::strcmp(section, "Ascended Food") == 0 ||
						std::strcmp(section, "Utility") == 0)
					{
						static const char* kFoodAttrs[] = {
							"Power", "Precision", "Toughness", "Vitality",
							"Concentration", "Condition Damage", "Expertise",
							"Ferocity", "Healing Power", "All Attributes", "Other"
						};
						constexpr int kAttrN = static_cast<int>(sizeof(kFoodAttrs) / sizeof(kFoodAttrs[0]));
						static std::string sFoodCacheKey;
						static std::vector<int> sFoodHubs;
						static std::vector<std::vector<int>> sFoodByAttr;
						const std::string foodKey = std::string(selectedCat) + "|" + section;
						if (sFoodCacheKey != foodKey || browseCatRebuilt)
						{
							sFoodCacheKey = foodKey;
							sFoodHubs.clear();
							sFoodByAttr.assign(static_cast<size_t>(kAttrN), {});
							sFoodHubs.reserve(8);
							for (int i : secIdx)
							{
								const char* a = FoodAttrSub(sites[i].id);
								if (!a)
								{
									sFoodHubs.push_back(i);
									continue;
								}
								bool placed = false;
								for (int ai = 0; ai < kAttrN; ++ai)
								{
									if (std::strcmp(a, kFoodAttrs[ai]) == 0)
									{
										sFoodByAttr[static_cast<size_t>(ai)].push_back(i);
										placed = true;
										break;
									}
								}
								if (!placed)
									sFoodHubs.push_back(i);
							}
						}
						DrawClippedRows(sFoodHubs, false);
						ImGui::Indent(10.f);
						for (int ai = 0; ai < kAttrN; ++ai)
						{
							if (sFoodByAttr[static_cast<size_t>(ai)].empty())
								continue;
							if (!BeginBrowseSection(section, kFoodAttrs[ai],
									static_cast<int>(sFoodByAttr[static_cast<size_t>(ai)].size())))
								continue;
							DrawClippedRows(sFoodByAttr[static_cast<size_t>(ai)], false);
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Minis") == 0)
					{
						static const char* kMiniSubs[] = {
							"Sets 1 to 3", "Core", "PvP", "WvW", "Heart of Thorns", "Raids",
							"Living World Season 3", "Path of Fire", "Living World Season 4",
							"The Icebrood Saga", "End of Dragons", "Secrets of the Obscure",
							"Janthir Wilds", "Visions of Eternity",
							"Festival Minis - Lunar New Year",
							"Festival Minis - Super Adventure Box",
							"Festival Minis - Dragon Bash",
							"Festival Minis - Festival of the Four Winds",
							"Festival Minis - Halloween",
							"Festival Minis - Wintersday",
							"Gem Store/Black Lion", "Promotional Minis", "Unavailable"
						};
						constexpr int kMiniN = static_cast<int>(sizeof(kMiniSubs) / sizeof(kMiniSubs[0]));
						static std::string sMiniCacheKey;
						static std::vector<int> sMiniHubs;
						static std::vector<std::vector<int>> sMiniBySub;
						if (sMiniCacheKey != selectedCat || browseCatRebuilt)
						{
							sMiniCacheKey = selectedCat;
							sMiniHubs.clear();
							sMiniBySub.assign(static_cast<size_t>(kMiniN), {});
							sMiniHubs.reserve(4);
							for (int i : secIdx)
							{
								const char* a = MinisSub(sites[i].id);
								if (!a)
								{
									sMiniHubs.push_back(i);
									continue;
								}
								bool placed = false;
								for (int si = 0; si < kMiniN; ++si)
								{
									if (std::strcmp(a, kMiniSubs[si]) == 0)
									{
										sMiniBySub[static_cast<size_t>(si)].push_back(i);
										placed = true;
										break;
									}
								}
								if (!placed)
									sMiniHubs.push_back(i);
							}
						}
						DrawClippedRows(sMiniHubs, false);
						ImGui::Indent(10.f);
						for (int si = 0; si < kMiniN; ++si)
						{
							if (sMiniBySub[static_cast<size_t>(si)].empty())
								continue;
							if (!BeginBrowseSection("Minis", kMiniSubs[si],
									static_cast<int>(sMiniBySub[static_cast<size_t>(si)].size())))
								continue;
							DrawClippedRows(sMiniBySub[static_cast<size_t>(si)], false);
						}
						ImGui::Unindent(10.f);
						continue;
					}
					if (std::strcmp(section, "Achievements") == 0)
					{
						static const char* kAchSubs[] = {
							"Living World", "Heart of Thorns", "Path of Fire", "End of Dragons",
							"Secrets of the Obscure", "Janthir Wilds", "Visions of Eternity",
							"Festivals", "Side Stories"
						};
						constexpr int kAchN = static_cast<int>(sizeof(kAchSubs) / sizeof(kAchSubs[0]));
						static std::string sAchCacheKey;
						static std::vector<int> sAchHubs;
						static std::vector<std::vector<int>> sAchBySub;
						if (sAchCacheKey != selectedCat || browseCatRebuilt)
						{
							sAchCacheKey = selectedCat;
							sAchHubs.clear();
							sAchBySub.assign(static_cast<size_t>(kAchN), {});
							for (int i : sBrowseCatIdx)
							{
								const char* sec = BrowseSection(selectedCat, sites[i].id);
								if (!sec || std::strcmp(sec, "Achievements") != 0)
									continue;
								const char* a = AchievementsSub(sites[i].id);
								if (!a)
								{
									sAchHubs.push_back(i);
									continue;
								}
								bool placed = false;
								for (int ai = 0; ai < kAchN; ++ai)
								{
									if (std::strcmp(a, kAchSubs[ai]) == 0)
									{
										sAchBySub[static_cast<size_t>(ai)].push_back(i);
										placed = true;
										break;
									}
								}
								if (!placed)
									sAchHubs.push_back(i);
							}
						}
						DrawClippedRows(sAchHubs, false);
						ImGui::Indent(10.f);
						for (int ai = 0; ai < kAchN; ++ai)
						{
							if (sAchBySub[static_cast<size_t>(ai)].empty())
								continue;
							if (!BeginBrowseSection("Achievements", kAchSubs[ai],
									static_cast<int>(sAchBySub[static_cast<size_t>(ai)].size())))
								continue;
							DrawClippedRows(sAchBySub[static_cast<size_t>(ai)], false);
						}
						ImGui::Unindent(10.f);
						continue;
					}
					DrawClippedRows(secIdx, false);
				}
			}
			else
			{
				anyInCategory = !sBrowseCatIdx.empty();
				DrawClippedRows(sBrowseCatIdx, false);
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

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::PushStyleColor(ImGuiCol_Text, kGoldMuted);
		ImGui::TextUnformatted("Created by Xydroc");
		ImGui::TextUnformatted("IGN - swift shadow kuda.5981 | Discord Name - xydroc");
		ImGui::PopStyleColor();
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
		ImGui::BeginChild("##gw2igh_tab_bar", ImVec2(0.f, ImGui::GetFrameHeightWithSpacing() + 4.f), false,
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

			if (ImGui::BeginPopupContextItem("##gw2igh_tab_ctx"))
			{
				if (ImGui::MenuItem(tab.pinned ? "Unpin" : "Pin"))
					BrowserTabs::TogglePin(i);
				if (ImGui::MenuItem("Close", nullptr, false, canClose))
					pendingClose = i;
				NoteHelperPopupHover();
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
			const bool plusClicked = ImGui::Button("+##gw2igh_new_tab");
			sNewTabBrowseAnchor = CaptureAnchorBelowItem();
			if (plusClicked || sRequestNewTabPicker)
			{
				sRequestNewTabPicker = false;
				sSyncCategory = true;
				sFocusFilter = true;
				ImGui::OpenPopup("##gw2igh_site_browse_newtab");
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Open site in a new tab (Ctrl+T)");
		}
		else
		{
			sRequestNewTabPicker = false;
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
			ImGui::Button("+##gw2igh_new_tab");
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Tab limit reached (8)");
		}

		ImGui::SameLine(0.f, 6.f);
		{
			const bool canRe = BrowserTabs::CanReopenClosed();
			if (canRe)
			{
				if (ImGui::SmallButton("^##gw2igh_reopen"))
					BrowserTabs::ReopenClosed();
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Reopen closed tab (Ctrl+Shift+T)");
			}
		}

		const BrowsePopupLayout browseLay = CalcBrowsePopupLayout(true, false);
		PrepareBrowsePopup(sNewTabBrowseAnchor, browseLay);
		if (ImGui::BeginPopup("##gw2igh_site_browse_newtab", kBrowsePopupFlags))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(true, &closePanel, false, true, browseLay.listH, browseLay.leftW);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			NoteHelperPopupHover();
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
		const char* raw = WikiBrowser::StatusCStr();
		if (!raw || !raw[0] || std::strcmp(raw, "Ready") == 0)
			return;

		const char* label = nullptr;
		ImVec4 col = kGoldMuted;
		if (std::strstr(raw, "Loading") ||
			std::strstr(raw, "Navigating") ||
			std::strstr(raw, "Launching") ||
			std::strstr(raw, "Creating"))
		{
			label = "Loading...";
			col = kGold;
		}
		else if (std::strstr(raw, "Closed") ||
			std::strstr(raw, "Hidden"))
		{
			return;
		}
		else if (std::strstr(raw, "fail") ||
			std::strstr(raw, "Fail") ||
			std::strstr(raw, "error") ||
			std::strstr(raw, "Error") ||
			std::strstr(raw, "not found") ||
			std::strstr(raw, "disabled"))
		{
			label = "Error - check Nexus log";
			col = kWarn;
		}
		else
		{
			/* Truncate long technical strings. */
			static char buf[48];
			const size_t n = std::strlen(raw);
			if (n > 40)
			{
				std::snprintf(buf, sizeof(buf), "%.37s...", raw);
				label = buf;
			}
			else
				label = raw;
			col = kGoldDim;
		}

		ImGui::SameLine();
		ImGui::TextColored(col, "%s", label);
	}

	void DrawMoreMenu()
	{
		if (ImGui::Button("...##gw2igh_more"))
			ImGui::OpenPopup("##gw2igh_toolbar_more");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("More actions");

		if (ImGui::BeginPopup("##gw2igh_toolbar_more"))
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
			ImGui::Separator();
			if (ImGui::MenuItem(G::ShowEvents ? "Hide Events panel" : "Show Events panel"))
			{
				if (G::ShowEvents)
				{
					G::ShowEvents = false;
					Settings::SetDirty();
				}
				else
					EventsPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowTekkitGuides ? "Hide Tekkit's Guides" : "Show Tekkit's Guides"))
			{
				if (G::ShowTekkitGuides)
				{
					G::ShowTekkitGuides = false;
					Settings::SetDirty();
				}
				else
					TekkitGuidesPad::Open();
			}
			NoteHelperPopupHover();
			ImGui::EndPopup();
		}
	}

	void DrawToolbar()
	{
		const SiteDef& active = Sites::Active();

		if (ImGui::Button("Browse###gw2igh_browse"))
		{
			sSyncCategory = true;
			sFocusFilter = true;
			ImGui::OpenPopup("##gw2igh_site_browse");
		}
		sBrowseAnchor = CaptureAnchorBelowItem();
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
		if (SoftButton("<###gw2igh_back", BrowserTabs::CanGoBack()))
			BrowserTabs::GoBack();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Back");
		ImGui::SameLine();
		if (SoftButton(">###gw2igh_fwd", BrowserTabs::CanGoForward()))
			BrowserTabs::GoForward();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Forward");
		ImGui::SameLine();
		if (ImGui::Button("Home###gw2igh_home"))
			BrowserTabs::GoHome();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Default landing site");
		ImGui::SameLine();
		if (ImGui::Button("Reload###gw2igh_reload"))
			BrowserTabs::Reload();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Reload");
		ImGui::PopStyleVar();

		ImGui::SameLine(0.f, 12.f);
		{
			float avail = ImGui::GetContentRegionAvail().x - 120.f;
			if (avail < 140.f) avail = 140.f;
			if (avail > 420.f) avail = 420.f;
			ImGui::SetNextItemWidth(avail);
		}
		if (ImGui::InputTextWithHint("###gw2igh_site_query", "Find in page...", G::LastQuery, sizeof(G::LastQuery),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if (G::LastQuery[0])
			{
				/* Enter = find on the current page (not Google). */
				sShowFind = true;
				std::snprintf(sFindQuery, sizeof(sFindQuery), "%s", G::LastQuery);
				WikiBrowser::Find(sFindQuery, true, sFindMatchCase, false);
				Settings::SetDirty();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Find text on this page. Enter = next match.\nUse Web for DuckDuckGo / site search.");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Web###gw2igh_web"))
		{
			if (G::LastQuery[0])
			{
				WikiBrowser::Search(G::LastQuery);
				Settings::SetDirty();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Search the active site (or DuckDuckGo).");

		ImGui::SameLine(0.f, 8.f);
		DrawMoreMenu();
		DrawStatusChip();

		/* Panel toggles on their own row — never clipped by Browse/search width. */
		if (ImGui::Button("Notes###gw2igh_notes"))
		{
			if (G::ShowNotes)
			{
				G::ShowNotes = false;
				Settings::SetDirty();
			}
			else
				NotesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Notes & clipboard helpers (waypoints, LFG, build codes)");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("TP###gw2igh_tp"))
		{
			if (G::ShowTpWatch)
			{
				G::ShowTpWatch = false;
				Settings::SetDirty();
			}
			else
				TpWatchPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Trading Post — delivery, watchlist, sell alerts (read-only)");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Events###gw2igh_events"))
		{
			if (G::ShowEvents)
			{
				G::ShowEvents = false;
				Settings::SetDirty();
			}
			else
				EventsPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("World events — UTC timers + track list");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Tekkit###gw2igh_tekkit"))
		{
			if (G::ShowTekkitGuides)
			{
				G::ShowTekkitGuides = false;
				Settings::SetDirty();
			}
			else
				TekkitGuidesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Tekkit's Guides — compass trails + category toggles (© Tekkit)");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Item###gw2igh_lookup"))
		{
			if (G::ShowLookup)
			{
				G::ShowLookup = false;
				Settings::SetDirty();
			}
			else
				LookupPad::OpenAndLookup();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Item lookup — chat code, ID, or name → wiki / BLTC");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Wallet###gw2igh_wallet"))
		{
			if (G::ShowWallet)
			{
				G::ShowWallet = false;
				Settings::SetDirty();
			}
			else
				WalletPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Wallet & materials snapshot (API key)");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Vault###gw2igh_vault"))
		{
			if (G::ShowVault)
			{
				G::ShowVault = false;
				Settings::SetDirty();
			}
			else
				VaultPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Dailies & Wizard’s Vault (same data as Browse → Live)");

		const BrowsePopupLayout browseLay = CalcBrowsePopupLayout(false, false);
		PrepareBrowsePopup(sBrowseAnchor, browseLay);
		if (ImGui::BeginPopup("##gw2igh_site_browse", kBrowsePopupFlags))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(true, &closePanel, false, false, browseLay.listH, browseLay.leftW);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			NoteHelperPopupHover();
			ImGui::EndPopup();
		}
	}


	void DrawDefaultSiteBrowse()
	{
		if (ImGui::Button("Choose default site...###gw2igh_choose_default"))
		{
			sSyncCategory = true;
			sFocusFilter = true;
			ImGui::OpenPopup("##gw2igh_default_site_browse");
		}
		sDefaultSiteBrowseAnchor = CaptureAnchorBelowItem();

		ImGui::SameLine();
		const SiteDef* def = SiteById(G::DefaultSiteId);
		if (def)
			ImGui::TextColored(kMuted, "%s - %s",
				def->category ? def->category : "",
				def->label ? def->label : "");
		else
			ImGui::TextColored(kMuted, "%s", G::DefaultSiteId);

		const BrowsePopupLayout browseLay = CalcBrowsePopupLayout(true, true);
		PrepareBrowsePopup(sDefaultSiteBrowseAnchor, browseLay);
		if (ImGui::BeginPopup("##gw2igh_default_site_browse", kBrowsePopupFlags))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(false, &closePanel, true, false, browseLay.listH, browseLay.leftW);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			NoteHelperPopupHover();
			ImGui::EndPopup();
		}
	}
}

bool UI_BlocksGameKeyboard()
{
	return gBlockGameKeyboard;
}

/* True while keystrokes should go to the CEF page (not ImGui filter/search/find). */
bool UI_BrowserKeyboardActive()
{
	/* Pointer must be on the OSR page — sticky gBrowserFocused alone stole
	   Browse/Find typing and game chat after leaving the page. */
	return G::ShowWiki && gOverBrowserPage;
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
	gBlockGameKeyboard = false;
	gPendingDefocus = true;
	UI_ResetKeyRouting();
}

void UI_Render()
{
	/* Always poll first — must run while the helper is closed too. */
	HelperHotkeys_Poll();
	WikiBrowser::Tick();
	/* Tekkit overlays — always, even with the browser closed. */
	CompassOverlay::Render();
	WorldOverlay::Render();
	/* URL-index warm: heavier when closed; light drip while open so Browse stays snappy. */
	if (!G::ShowWiki)
		Sites::TickWarmUrlKeys(96);
	else if (!Sites::UrlKeysReady())
		Sites::TickWarmUrlKeys(16);

	gBlockGameKeyboard = false;
	gBlockGameMouse = false;
	gOverBrowserPage = false;
	gWikiRectValid = false;
	sHelperPopupHovered = false;

	if (gPendingDefocus)
	{
		gPendingDefocus = false;
		ImGui::SetWindowFocus(nullptr);
	}

	static bool sWasOpen = false;
	if (!G::ShowWiki)
	{
		/* Do NOT clear WantTextInput / WantCaptureKeyboard here — those flags are
		   shared with Nexus. Wiping them every frame breaks the library search field
		   (keys fall through to GW2 hotkeys, e.g. G = guild). */

		if (sWasOpen)
		{
			BrowserTabs::PrepareSave();
			Settings::SetDirty();
			UI_ReleaseGameInput();
			sWasOpen = false;
		}
		BlurBrowser();
		WikiBrowser::SetVisible(false);

		/* Notes / TP / Lookup / Wallet can stay open while the helper browser is closed.
		   Only block GW2 while the pointer is over those windows — do not
		   force Capture*FromApp(false) every frame (breaks Nexus / open). */
		const bool notesHover = NotesPad::Render();
		const bool tpHover = TpWatchPad::Render();
		const bool lookupHover = LookupPad::Render();
		const bool walletHover = WalletPad::Render();
		const bool vaultHover = VaultPad::Render();
		const bool eventsHover = EventsPad::Render();
		const bool tekkitHover = TekkitGuidesPad::Render();
		if (notesHover || tpHover || lookupHover || walletHover || vaultHover || eventsHover || tekkitHover)
		{
			ImGuiIO& io = ImGui::GetIO();
			gBlockGameMouse = true;
			gBlockGameKeyboard = true;
			ImGui::CaptureMouseFromApp(true);
			if (io.WantTextInput)
				ImGui::CaptureKeyboardFromApp(true);
		}
		NotesPad::Save(false);
		Settings::Save(false);
		return;
	}

	if (!sWasOpen)
	{
		gForceHelperOnScreen = true; /* always verify visible on each open */
		BrowserTabs::NavigateActive();
		sWasOpen = true;
	}

	if (!G::HasSavedSize)
	{
		const ImGuiIO& dio = ImGui::GetIO();
		if (dio.DisplaySize.x > 100.f && dio.DisplaySize.y > 100.f)
		{
			/* First open: ~30% of the display (clamped so it stays usable). */
			G::WindowWidth = Clampf(dio.DisplaySize.x * 0.30f, 640.f, 1600.f);
			G::WindowHeight = Clampf(dio.DisplaySize.y * 0.30f, 420.f, 1000.f);
			G::HasSavedSize = true; /* apply once — ImGui FirstUseEver + persist */
			Settings::SetDirty();
		}
	}
	ClampHelperGeomToDisplay();
	const ImGuiCond geomCond = gForceHelperOnScreen ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
	ImGui::SetNextWindowSize(ImVec2(G::WindowWidth, G::WindowHeight), geomCond);
	ImGui::SetNextWindowPos(ImVec2(G::WindowPosX, G::WindowPosY), geomCond);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gForceHelperOnScreen)
		ImGui::SetNextWindowFocus();
	gForceHelperOnScreen = false;

	PushWikiTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);
	ImGui::SetNextWindowBgAlpha(G::Opacity);

	bool open = G::ShowWiki;
	if (!ImGui::Begin("In-Game Helper##GW2InGameHelper", &open,
		ImGuiWindowFlags_NoNavInputs))
	{
		/* Collapsed title bar — CEF must be was_hidden (0% viewability otherwise)
		   but keep the process alive so expand does not hitch. */
		const ImVec2 pos = ImGui::GetWindowPos();
		const ImVec2 winSize = ImGui::GetWindowSize();
		gWikiMin = pos;
		gWikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
		gWikiRectValid = true;
		BlurBrowser();
		WikiBrowser::SetVisible(false, /*keepProcessAlive=*/true);
		const bool mouseOver =
			ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		gBlockGameMouse = mouseOver;
		/* Collapsed bar: only eat keys while the pointer is on it. */
		gBlockGameKeyboard = mouseOver;
		if (mouseOver)
		{
			ImGui::GetIO().WantCaptureMouse = true;
			ImGui::CaptureMouseFromApp(true);
			ImGui::GetIO().WantCaptureKeyboard = true;
			ImGui::CaptureKeyboardFromApp(true);
		}
		ImGui::End();
		ImGui::PopStyleVar();
		PopWikiTheme();
		/* Still draw pads while the main window is collapsed. */
		const bool notesHover = NotesPad::Render();
		const bool tpHover = TpWatchPad::Render();
		const bool lookupHover = LookupPad::Render();
		const bool walletHover = WalletPad::Render();
		const bool vaultHover = VaultPad::Render();
		const bool eventsHover = EventsPad::Render();
		const bool tekkitHover = TekkitGuidesPad::Render();
		if (notesHover || tpHover || lookupHover || walletHover || vaultHover || eventsHover || tekkitHover)
		{
			gBlockGameMouse = true;
			gBlockGameKeyboard = true;
			ImGui::CaptureMouseFromApp(true);
			if (ImGui::GetIO().WantTextInput)
				ImGui::CaptureKeyboardFromApp(true);
		}
		NotesPad::Save(false);
		Settings::Save(false);
		return;
	}
	if (!open)
	{
		G::ShowWiki = false;
		Settings::SetDirty();
		WikiBrowser::SetVisible(false);
		UI_ReleaseGameInput();
	}

	ImGui::SetWindowFontScale(G::FontScale);

	BrowserTabs::EnsureDefault();
	BrowserTabs::Tick();

	ImGui::TextColored(kGold, "IN-GAME HELPER");
	ImGui::SameLine(0.f, 12.f);
	DrawToolbar();

	/* Tab / find hotkeys — use ImGuiIO (Nexus-filled KeysDown), not GetAsyncKeyState. */
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool typing = io.WantTextInput;
		const bool ctrl = io.KeyCtrl;
		const bool shift = io.KeyShift;
		const bool alt = io.KeyAlt;
		auto keyDown = [&](int vk) -> bool {
			return vk >= 0 && vk < IM_ARRAYSIZE(io.KeysDown) && io.KeysDown[vk];
		};
		const bool keyF = keyDown('F');
		const bool keyT = keyDown('T');
		const bool keyW = keyDown('W');
		const bool keyTab = ImGui::IsKeyDown(ImGuiKey_Tab);

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
		{
			sShowFind = true;
			sFocusFind = true;
		}
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
		if (sFocusFind)
		{
			ImGui::SetKeyboardFocusHere();
			sFocusFind = false;
		}
		const bool findEnter = ImGui::InputTextWithHint("###gw2igh_find_q", "Find in page...", sFindQuery, sizeof(sFindQuery),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		ImGui::Checkbox("Aa###gw2igh_find_case", &sFindMatchCase);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Match case");
		ImGui::SameLine();
		if (ImGui::Button("Next###gw2igh_find_next") || findEnter)
		{
			if (sFindQuery[0])
				WikiBrowser::Find(sFindQuery, true, sFindMatchCase, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Prev###gw2igh_find_prev"))
		{
			if (sFindQuery[0])
				WikiBrowser::Find(sFindQuery, false, sFindMatchCase, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear###gw2igh_find_clear"))
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
	ImGui::BeginChild("##gw2igh_wiki_osr_slot", avail, false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavInputs);

	const ImVec2 imageSize = ImGui::GetContentRegionAvail();
	const ImVec2 slotPos = ImGui::GetCursorScreenPos();
	const ImGuiIO& dio = ImGui::GetIO();
	const float dispW = dio.DisplaySize.x;
	const float dispH = dio.DisplaySize.y;
	/* Any overlap with the game display — fully off-screen is not viewable. */
	const bool slotOnScreen =
		dispW < 1.f || dispH < 1.f ||
		(slotPos.x + imageSize.x > 0.f && slotPos.y + imageSize.y > 0.f &&
			slotPos.x < dispW && slotPos.y < dispH);
	/* Hysteresis on size so drag-resize does not flap was_hidden (that can
	   interrupt ad / page timers). Opacity is NOT a gate — users still read
	   at low opacity; freezing CEF there would break content. */
	static bool sSlotLargeEnough = true;
	if (sSlotLargeEnough)
		sSlotLargeEnough = imageSize.x >= 36.f && imageSize.y >= 36.f;
	else
		sSlotLargeEnough = imageSize.x >= 48.f && imageSize.y >= 48.f;
	const bool pageViewable =
		G::ShowWiki && open && slotOnScreen && sSlotLargeEnough;
	if (pageViewable)
	{
		float cefW = imageSize.x;
		float cefH = imageSize.y;
		if (HostWantsDesktopAdViewport(WikiBrowser::CurrentUrlCStr()))
			DesktopAdCefSize(imageSize.x, imageSize.y, &cefW, &cefH);
		WikiBrowser::SetVisible(true);
		WikiBrowser::SetBounds(0.f, 0.f, cefW, cefH);
		WikiBrowser::PresentFrame();
	}
	else
	{
		/* Keep process — window may still be open (tiny / off-screen). */
		BlurBrowser();
		WikiBrowser::SetVisible(false, /*keepProcessAlive=*/true);
	}

	ImGuiIO& io = ImGui::GetIO();
	bool overPage = false;

	if (WikiBrowser::HasFrame())
	{
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		{
			float uvU = 1.f, uvV = 1.f;
			WikiBrowser::FrameUvMax(&uvU, &uvV);
			ImGui::Image(reinterpret_cast<ImTextureID>(WikiBrowser::FrameSrv()), imageSize,
				ImVec2(0.f, 0.f), ImVec2(uvU, uvV));
		}

		ImGui::SetCursorScreenPos(cursor);
		ImGui::InvisibleButton("##gw2igh_wiki_hit", imageSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
			ImGuiButtonFlags_MouseButtonMiddle);

		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		overPage = hovered || active;
		const unsigned mods = CefModsFromImGui(io);
		/* Shared across enter/leave so remainder isn't a different static. */
		static float sWheelAccX = 0.f;
		static float sWheelAccY = 0.f;

		if (overPage)
		{
			/* MediaWiki search / inputs need CEF focus without relying on click
			   alone — keep focus while the pointer is on the page. */
			FocusBrowser();

			const ImVec2 mouse = io.MousePos;
			const float localX = mouse.x - cursor.x;
			const float localY = mouse.y - cursor.y;
			int cx = 0, cy = 0;
			MapToCef(localX, localY, imageSize.x, imageSize.y, &cx, &cy);

			WikiBrowser::FeedMouseMove(cx, cy, false, mods);

			auto click = [&](ImGuiMouseButton btn, int cefBtn) {
				if (ImGui::IsMouseClicked(btn))
				{
					/* Re-assert focus on click so the text caret appears (OSR). */
					FocusBrowserForce();
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
				/* Accumulate fractional trackpad deltas so smooth scroll isn't
				   quantized away by int(wheel*120). Discrete notches still land as ±120. */
				sWheelAccX += io.MouseWheelH * 120.f;
				sWheelAccY += io.MouseWheel * 120.f;
				const int dx = static_cast<int>(sWheelAccX);
				const int dy = static_cast<int>(sWheelAccY);
				if (dx != 0 || dy != 0)
				{
					sWheelAccX -= static_cast<float>(dx);
					sWheelAccY -= static_cast<float>(dy);
					FocusBrowser();
					WikiBrowser::FeedMouseWheel(cx, cy, dx, dy, mods);
				}
			}
		}
		else
		{
			sWheelAccX = 0.f;
			sWheelAccY = 0.f;
		}
	}
	else
	{
		ImGui::Spacing();
		ImGui::TextColored(kGold, WikiBrowser::IsReady()
			? "Waiting for first paint..."
			: "Loading browser...");
		ImGui::TextWrapped("%s", WikiBrowser::StatusCStr());
		if (WikiBrowser::IsReady())
		{
			const char* why = WikiBrowser::PaintWaitReasonCStr();
			if (why && why[0])
			{
				ImGui::PushStyleColor(ImGuiCol_Text, kGoldMuted);
				ImGui::TextWrapped("%s", why);
				ImGui::PopStyleColor();
			}
		}
	}

	ImGui::EndChild();

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 winSize = ImGui::GetWindowSize();
	gWikiMin = pos;
	gWikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
	gWikiRectValid = true;

	const bool mouseOverWiki = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	/* Browse / More / tab menus are separate ImGui windows — use sHelperPopupHovered
	   (set while drawing those popups). Do not use AnyWindow: that matched Nexus UI
	   and stole keyboard from the library search field. */
	const bool mouseOverHelperUi = mouseOverWiki || sHelperPopupHovered;

	gOverBrowserPage = overPage;
	gBlockGameMouse = G::ShowWiki && mouseOverHelperUi;

	/* Snapshot ImGui's own text-input flag BEFORE we adjust capture for CEF.
	   Never force WantTextInput=false — that broke Browse/Find typing and also
	   wiped Nexus library search (shared ImGui context). */
	const bool imguiTyping = io.WantTextInput;

	/* Keys follow the pointer while the helper is open:
	   - over helper chrome/page/popups → addon (ImGui or CEF)
	   - over the game (chat, world) → GW2
	   Blur CEF when the cursor leaves so page focus cannot stick. */
	static bool sWasPointerOnHelper = false;
	if (!mouseOverHelperUi)
	{
		BlurBrowser();
		/* Only drop OUR CaptureKeyboardFromApp claim — leave WantTextInput alone
		   so Nexus / other addons keep receiving typed characters. */
		ImGui::CaptureKeyboardFromApp(false);
		if (sWasPointerOnHelper)
			UI_ResetKeyRouting();
		/* Avoid ImGui::SetWindowFocus(nullptr) here — it closed Browse popups. */	}
	else if (!overPage && ImGui::IsAnyItemActive())
		BlurBrowser(); /* Find/filter/etc. active — release CEF caret/focus */
	sWasPointerOnHelper = mouseOverHelperUi;

	gBlockGameKeyboard = G::ShowWiki && mouseOverHelperUi;

	if (overPage)
	{
		/* CEF page under the cursor — Nexus also gates on WantTextInput. */
		io.WantTextInput = true;
		io.WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}
	else if (mouseOverHelperUi && imguiTyping)
	{
		io.WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}

	if (gBlockGameMouse)
	{
		io.WantCaptureMouse = true;
		/* Sticky for next NewFrame — Nexus UiInput gates game clicks on this flag. */
		ImGui::CaptureMouseFromApp(true);
	}
	if (gBlockGameKeyboard)
	{
		io.WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}

	if (std::fabs(pos.x - G::WindowPosX) > 0.5f || std::fabs(pos.y - G::WindowPosY) > 0.5f ||
		std::fabs(winSize.x - G::WindowWidth) > 0.5f || std::fabs(winSize.y - G::WindowHeight) > 0.5f)
	{
		G::WindowPosX = pos.x;
		G::WindowPosY = pos.y;
		G::WindowWidth = winSize.x;
		G::WindowHeight = winSize.y;
		G::HasSavedPos = true;
		G::HasSavedSize = true;
		Settings::SetDirty();
	}

	ImGui::SetWindowFontScale(1.f);
	ImGui::End();
	ImGui::PopStyleVar();
	PopWikiTheme();

	const bool notesHover = NotesPad::Render();
	const bool tpHover = TpWatchPad::Render();
	const bool lookupHover = LookupPad::Render();
	const bool walletHover = WalletPad::Render();
	const bool vaultHover = VaultPad::Render();
	const bool eventsHover = EventsPad::Render();
	const bool tekkitHover = TekkitGuidesPad::Render();
	if (notesHover || tpHover || lookupHover || walletHover || vaultHover || eventsHover || tekkitHover)
	{
		/* Pointer on a pad — block GW2 only; do not force WantCapture* so
		   Nexus / other ImGui addons keep working when the cursor leaves. */
		gBlockGameMouse = true;
		gBlockGameKeyboard = true;
		ImGui::CaptureMouseFromApp(true);
		if (io.WantTextInput)
			ImGui::CaptureKeyboardFromApp(true);
	}
	NotesPad::Save(false);
	Settings::Save(false);
}

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
	if (ImGui::Checkbox("Show Notes window###gw2igh_shownotes", &G::ShowNotes))
	{
		if (G::ShowNotes)
			NotesPad::Open();
		Settings::SetDirty();
	}
	ImGui::TextColored(kMuted, "Waypoints, chat codes, builds, LFG snippets — Copy to clipboard.");
	if (ImGui::Checkbox("Show TP Watchlist###gw2igh_showtp", &G::ShowTpWatch))
	{
		if (G::ShowTpWatch)
			TpWatchPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(kMuted, "Add any item via chat code or ID — buy/sell prices (read-only).");
	if (ImGui::Checkbox("Show Item Lookup###gw2igh_showlookup", &G::ShowLookup))
	{
		if (G::ShowLookup)
			LookupPad::OpenAndLookup();
		Settings::SetDirty();
	}
	ImGui::TextColored(kMuted, "Chat code / ID / name — rarity, TP prices, wiki & BLTC links.");
	if (ImGui::Checkbox("Show Wallet & Mats###gw2igh_showwallet", &G::ShowWallet))
	{
		if (G::ShowWallet)
			WalletPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(kMuted, "Gold, mats, bank, shared, and per-toon bags — searchable. Free-floating.");
	if (ImGui::Checkbox("Show Dailies & Vault###gw2igh_showvault", &G::ShowVault))
	{
		if (G::ShowVault)
			VaultPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(kMuted, "Season + live Vault objectives. Same API as Browse → Live.");
	if (ImGui::Checkbox("Show World Events###gw2igh_showevents", &G::ShowEvents))
	{
		if (G::ShowEvents)
			EventsPad::OpenAndRefresh();
		Settings::SetDirty();
	}
	ImGui::TextColored(kMuted, "Full UTC catalog (bosses + metas). Track list + claim marks (progression).");
	if (ImGui::Checkbox("Show Tekkit's Guides###gw2igh_showtekkit", &G::ShowTekkitGuides))
	{
		if (G::ShowTekkitGuides)
			TekkitGuidesPad::Open();
		Settings::SetDirty();
	}
	ImGui::TextColored(kMuted,
		"Tekkit pack categories (persisted). Compass overlay uses read-only MumbleLink.");
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
		DrawDefaultSiteBrowse();
		ImGui::TextColored(kMuted, "Home button uses this. Also used when no tabs are saved yet.");
	}

	if (ImGui::SliderFloat("Opacity###gw2igh_opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::SliderFloat("Font scale###gw2igh_font", &G::FontScale, 0.75f, 2.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::Checkbox("Keep browser warm when closed###gw2igh_warm", &G::KeepHelperWarm))
		Settings::SetDirty();
	ImGui::TextColored(kMuted, "Faster reopen; uses more RAM while the helper is hidden.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("GW2 API key (Live panels)");
	ImGui::TextColored(kMuted,
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
		ImGui::TextColored(kMuted, "No key — Vault/Progress show public data until you add one.");
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
	ImGui::TextColored(kMuted,
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
		"Hotkeys: Ctrl+Shift+H open/close | Ctrl+T new tab | Ctrl+W close | "
		"Ctrl+Tab cycle | Ctrl+Shift+T reopen | Ctrl+F find");
	SyncQr::DrawOptionsSection();
	Settings::Save(false);
	ImGui::PopID();
}
