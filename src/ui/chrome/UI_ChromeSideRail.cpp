#include "UI.h"
#include "UIInternal.h"
#include "UI_ChromeSideRailInternal.h"

#include "AccountPad.h"
#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "CompletionPad.h"
#include "CraftingPad.h"
#include "DirectionCompass.h"
#include "EconomyPad.h"
#include "EventsPad.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "Gw2UiInternal.h"
#include "HelperTheme.h"
#include "InstancesPad.h"
#include "LogManagerPad.h"
#include "NotesPad.h"
#include "PadNav.h"
#include "PathingGuidesPad.h"
#include "Settings.h"
#include "SettingsPad.h"
#include "VaultPad.h"
#include "WalletPad.h"
#include "WatchCapture.h"
#include "WatchPad.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

#include <windows.h>

#include <algorithm>
#include <string>

namespace UIDetail
{
	void DrawHelperSideRail()
	{
		if (!G::ShowWiki || !gUi.wikiRectValid)
		{
			G::SideRailW = 0.f;
			return;
		}

		auto openSiteInActive = [](const char* siteId) {
			G::ShowWiki = true;
			Settings::SetDirty();
			if (!siteId || !siteId[0])
				return;
			BrowserTabs::OpenInActive(siteId, true);
		};
		auto openSiteNewTab = [](const char* siteId) {
			G::ShowWiki = true;
			Settings::SetDirty();
			if (!siteId || !siteId[0])
				return;
			if (BrowserTabs::OpenNew(siteId, true) < 0)
				BrowserTabs::OpenInActive(siteId, true);
		};
		auto openUrlInActive = [](const char* siteId, const char* url) {
			G::ShowWiki = true;
			Settings::SetDirty();
			if (!url || !url[0])
				return;
			BrowserTabs::OpenUrlInActive(siteId && siteId[0] ? siteId : "browse", url);
		};
		auto openUrlNewTab = [](const char* siteId, const char* url) {
			G::ShowWiki = true;
			Settings::SetDirty();
			if (!url || !url[0])
				return;
			if (BrowserTabs::OpenNewUrl(siteId && siteId[0] ? siteId : "browse", url) < 0)
				BrowserTabs::OpenUrlInActive(siteId && siteId[0] ? siteId : "browse", url);
		};
		auto fireSite = [&](const char* siteId, bool newTab) {
			if (WinePadOpen::Soft())
			{
				if (newTab)
					WinePadOpen::QueueRailSiteNewTab(siteId);
				else
					WinePadOpen::QueueRailSiteActive(siteId);
			}
			else if (newTab)
				openSiteNewTab(siteId);
			else
				openSiteInActive(siteId);
		};
		auto fireUrl = [&](const char* siteId, const char* url, bool newTab) {
			if (WinePadOpen::Soft())
			{
				if (newTab)
					WinePadOpen::QueueRailUrlNewTab(siteId, url);
				else
					WinePadOpen::QueueRailUrlActive(siteId, url);
			}
			else if (newTab)
				openUrlNewTab(siteId, url);
			else
				openUrlInActive(siteId, url);
		};

		const bool labels = false; /* icon dock only — hover for names */
		/* Prefer width stamped during title draw so the strip stays flush with dockX. */
		float railW = G::SideRailW;
		if (railW < 40.f)
			railW = HelperSideRailWidth();
		G::SideRailW = railW;

		const ImVec2 helperMin = gUi.wikiMin;
		const ImVec2 helperMax = gUi.wikiMax;
		const float dockX = helperMin.x - railW;
		/* 1px into the helper body so bilinear image edges / exclusive clip
		   cannot leave a hairline of the game between nav and pad. Title
		   leftExtend still uses railW (not this overlap). */
		/* Align under the pad title bar (must match DrawPadTitleBar kTitleH).
		   Extra clearance so the title crest does not cover the first rail icon. */
		constexpr float kTitleBarH = 50.f;
		constexpr float kCrestClearance = 24.f;
		const float dockY = helperMin.y + kTitleBarH + kCrestClearance;
		float dockH = helperMax.y - dockY;
		if (dockH < 48.f)
			dockH = 48.f;

		/*
		 * Auto-size icons to the helper height — always fit, never scroll the nav.
		 * Leftover dock height expands each button (FramePadding) so the stack
		 * reaches the bottom — no empty strip and no blank gaps between rows.
		 */
		const float iconSz = SideRail::FitIconSize(dockH, labels);
		const float padY = SideRail::PadY(labels);
		const float itemSp = SideRail::ItemSpacing(iconSz, labels);
		const float baseFp = SideRail::FramePadY(iconSz, labels);
		const float fp = SideRail::FillFramePadY(dockH, iconSz, labels, itemSp, baseFp);

		constexpr float kSeamOverlap = 1.f;
		ImGui::SetNextWindowPos(ImVec2(dockX, dockY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(railW + kSeamOverlap, dockH), ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(railW + kSeamOverlap, 48.f), ImVec2(railW + kSeamOverlap, dockH + 8.f));
		ImGui::SetNextWindowBgAlpha(0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, padY));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.f, itemSp));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, fp));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);

		const ImGuiWindowFlags dockFlags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::Begin("##gw2igh_side_dock", nullptr, dockFlags);

		/* Hero plate behind icons — translucent wash matching helper; stay inside the rail. */
		{
			ImDrawList* railDl = ImGui::GetWindowDrawList();
			const ImVec2 rp0 = ImGui::GetWindowPos();
			const ImVec2 rp1(rp0.x + ImGui::GetWindowSize().x, rp0.y + ImGui::GetWindowSize().y);
			const float oa = G::Opacity < 0.f ? 0.f : (G::Opacity > 1.f ? 1.f : G::Opacity);
			Texture_t* wash = Gw2UiDetail::GetChromeNamed("panel-wash");
			if (!wash || !wash->Resource)
				wash = Gw2UiDetail::GetChromeTex(static_cast<int>(Gw2Ui::Icon::PanelFillAlt));
			/* Match PaintPadChrome translucent underpaint / wash / veil. */
			const ImU32 under = IM_COL32(10, 8, 6, static_cast<int>(oa * 220.f + 0.5f));
			const ImU32 veil = IM_COL32(6, 4, 3, static_cast<int>(oa * 55.f + 0.5f));
			const ImU32 washCol = IM_COL32(255, 255, 255, static_cast<int>(oa * 205.f + 0.5f));
			const float helperW = (std::max)(120.f, gUi.wikiMax.x - gUi.wikiMin.x);
			const float helperX = gUi.wikiMin.x;

			railDl->PushClipRect(rp0, rp1, false);
			railDl->AddRectFilled(rp0, rp1, under);
			/* Deeper grain under the wash (curated hero plate). */
			if (Texture_t* hero = Gw2UiDetail::GetChromeNamed("hero-plate"))
			{
				if (hero->Resource)
				{
					const ImU32 heroCol = IM_COL32(255, 255, 255, static_cast<int>(oa * 90.f + 0.5f));
					railDl->AddImage(reinterpret_cast<ImTextureID>(hero->Resource),
						rp0, rp1, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), heroCol);
				}
			}
			if (wash && wash->Resource)
			{
				const float u0 = (rp0.x - helperX) / helperW;
				const float u1 = (rp1.x - helperX) / helperW;
				railDl->AddImage(reinterpret_cast<ImTextureID>(wash->Resource),
					rp0, rp1, ImVec2(u0, 0.f), ImVec2(u1, 1.f), washCol);
				railDl->AddRectFilled(rp0, rp1, veil);
			}
			railDl->PopClipRect();

			/* Outer left + bottom of nav. Soft join only (not a full outer fringe). */
			Gw2UiDetail::PaintHeroRim(railDl, rp0, rp1, oa,
				/*omitLeft=*/false, /*omitRight=*/true, /*omitTop=*/true, /*omitBottom=*/false);

			/* Plaque corners — bottom only (top sat under the title crest). */
			if (Texture_t* corner = Gw2UiDetail::GetChromeNamed("plaque-corner"))
			{
				if (corner->Resource)
				{
					const ImTextureID cid = reinterpret_cast<ImTextureID>(corner->Resource);
					const float cs = SideRail::kCornerCap;
					const ImU32 ccol = IM_COL32(255, 255, 255, static_cast<int>(oa * 200.f + 0.5f));
					railDl->AddImage(cid,
						ImVec2(rp0.x - 1.f, rp1.y - cs + 1.f),
						ImVec2(rp0.x - 1.f + cs, rp1.y + 1.f),
						ImVec2(0.f, 1.f), ImVec2(1.f, 0.f), ccol);
				}
			}
		}

		{
			const bool hit = PadNav::SideToggle("Browse###gw2igh_browse", false, static_cast<int>(Gw2Ui::Icon::BrowseInfo), iconSz);
			const bool newTab = SideRail::ItemWantsNewTab();
			if (hit || newTab)
				fireSite("browse", newTab);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Browse sites - categories, favorites\nClick: this tab · Ctrl+click / middle-click: new tab");

		{
			const bool hit = PadNav::SideToggle("Wiki###gw2igh_wiki", false, static_cast<int>(Gw2Ui::Icon::SheetsBook), iconSz);
			const bool newTab = SideRail::ItemWantsNewTab();
			if (hit || newTab)
				fireUrl("browse", "about:browse-cat-wiki", newTab);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Wiki category - Main Page, updates, legendaries, festivals, …\nClick: this tab · Ctrl+click / middle-click: new tab");

		{
			const bool hit = PadNav::SideToggle("Sheets###gw2igh_cheatsheets", false, static_cast<int>(Gw2Ui::Icon::Wiki), iconSz);
			const bool newTab = SideRail::ItemWantsNewTab();
			if (hit || newTab)
				fireUrl("browse", "about:cheatsheets-hub", newTab);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Offline cheat sheets - food, fractals, squad tools, ...\nClick: this tab · Ctrl+click / middle-click: new tab");

		SideRail::SectionGap(false, "TOOLS");

		if (PadNav::SideToggle("Compass###gw2igh_dircompass", G::ShowCompassPad, static_cast<int>(Gw2Ui::Icon::CompassRadar), iconSz))
		{
			if (G::ShowCompassPad) { G::ShowCompassPad = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&DirectionCompass::Open, "Compass");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Direction compass - enable + letter size + radius\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+O (Settings -> Keybinds)");

		if (PadNav::SideToggle("Pathing###gw2igh_pathing", G::ShowPathingGuides, static_cast<int>(Gw2Ui::Icon::PathingMap), iconSz))
		{
			if (G::ShowPathingGuides) { G::ShowPathingGuides = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&PathingGuidesPad::Open, "Pathing");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Pathing - Tekkit + Lady Elyssa + Hero packs\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+G (Settings -> Keybinds)");

		if (PadNav::SideToggle("Vault###gw2igh_vault", G::ShowVault, static_cast<int>(Gw2Ui::Icon::VaultStar), iconSz))
		{
			if (G::ShowVault) { G::ShowVault = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&VaultPad::OpenAndRefresh, "Vault");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Vault - dailies & Wizard's Vault\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+V (Settings -> Keybinds)");

		if (PadNav::SideToggle("Events###gw2igh_events", G::ShowEvents, static_cast<int>(Gw2Ui::Icon::EventsMedal), iconSz))
		{
			if (G::ShowEvents) { G::ShowEvents = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&EventsPad::OpenAndRefresh, "Events");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"World events - UTC timers + track list\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+E (Settings -> Keybinds)");

		if (PadNav::SideToggle("Instances###gw2igh_instances", G::ShowInstances, static_cast<int>(Gw2Ui::Icon::InstGate), iconSz))
		{
			if (G::ShowInstances) { G::ShowInstances = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&InstancesPad::OpenAndRefresh, "Instances");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Instances - story / fractal / raid / strike\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+I (Settings -> Keybinds)");

		if (PadNav::SideToggle("Economy###gw2igh_economy", G::ShowEconomy, static_cast<int>(Gw2Ui::Icon::EconStack), iconSz))
		{
			if (G::ShowEconomy) { G::ShowEconomy = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&EconomyPad::OpenAndRefresh, "Economy");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Economy - Flip Finder, charts, cart, trading\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+Y (Settings -> Keybinds)");

		if (PadNav::SideToggle("Crafting###gw2igh_crafting", G::ShowCrafting, static_cast<int>(Gw2Ui::Icon::Key), iconSz))
		{
			if (G::ShowCrafting) { G::ShowCrafting = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&CraftingPad::OpenAndRefresh, "Crafting");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Crafting - plan, known recipes, browse, craft cart\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+K (Settings -> Keybinds)");

		if (PadNav::SideToggle("Achievements###gw2igh_achievements",
			G::ShowAchievements,
			static_cast<int>(Gw2Ui::Icon::Achievements), iconSz))
		{
			if (G::ShowAchievements) { G::ShowAchievements = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&CompletionPad::OpenAchievements, "Achievements");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Achievements - catalog pack + account API sync\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+Q (Settings -> Keybinds)");

		if (PadNav::SideToggle("Notes###gw2igh_notes", G::ShowNotes, static_cast<int>(Gw2Ui::Icon::NotesScroll), iconSz))
		{
			if (G::ShowNotes) { G::ShowNotes = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&NotesPad::Open, "Notes");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Snippets + Waypoints search\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+N (Settings -> Keybinds)");

		if (PadNav::SideToggle("DPS Logs###gw2igh_logs", G::ShowLogManager, static_cast<int>(Gw2Ui::Icon::LogsSwords), iconSz))
		{
			if (G::ShowLogManager) { G::ShowLogManager = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&LogManagerPad::OpenAndRefresh, "DPS Logs");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"DPS Logs - ArcDPS EVTC via Elite Insights\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+L (Settings -> Keybinds)");

		if (PadNav::SideToggle("Stash###gw2igh_stash", G::ShowWallet, static_cast<int>(Gw2Ui::Icon::Inventory), iconSz))
		{
			if (G::ShowWallet) { G::ShowWallet = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&WalletPad::OpenAndRefresh, "Stash");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Stash - wallet, materials, bank, shared, bags\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+U (Settings -> Keybinds)");

		if (PadNav::SideToggle("Account###gw2igh_account", G::ShowAccount, static_cast<int>(Gw2Ui::Icon::AccountSword), iconSz))
		{
			if (G::ShowAccount) { G::ShowAccount = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&AccountPad::OpenAndRefresh, "Account");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Account - progress, unlocks, history\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+A (Settings -> Keybinds)");

		if (PadNav::SideToggle("Watch###gw2igh_watch",
			G::ShowWatch || G::ShowWatchMirror, static_cast<int>(Gw2Ui::Icon::WatchView), iconSz))
		{
			WatchPad::ToggleControl();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Watch - Start/Stop control\n"
				"Click again while Mirror is up to Soft-stop\n"
				"(stream stops in ~0.1s; Mirror closes shortly after)\n"
				"Default: Ctrl+Shift+W (Settings -> Keybinds)");

		{
			const bool hit = PadNav::SideToggle("API Check###gw2igh_api_check", false, static_cast<int>(Gw2Ui::Icon::ApiHourglass), iconSz);
			const bool newTab = SideRail::ItemWantsNewTab();
			if (hit || newTab)
			{
				if (!WinePadOpen::Soft())
				{
					const std::wstring dir = AddonPaths::DataDir();
					if (!dir.empty())
					{
						auto kill = [&](const wchar_t* ext) {
							std::wstring p = dir;
							if (!p.empty() && p.back() != L'\\' && p.back() != L'/')
								p.push_back(L'\\');
							p += L"gw2-api-check";
							p += ext;
							DeleteFileW(p.c_str());
						};
						kill(L".html");
						kill(L".ver");
						kill(L".ok");
					}
				}
				fireUrl("browse", "about:gw2-api-check", newTab);
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Probe official api.guildwars2.com endpoints (public + your key).\n"
				"Local page - not a third-party status site.");

		if (PadNav::SideToggle("Settings###gw2igh_settings", G::ShowSettings, static_cast<int>(Gw2Ui::Icon::SettingsGear), iconSz))
		{
			if (G::ShowSettings) { G::ShowSettings = false; Settings::SetDirty(); }
			else WinePadOpen::SoftOpen(&SettingsPad::Open, "Settings");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Settings - opacity, font, API key, Keybinds tab\n"
				"Wine: soft-open (deferred a few frames)\n"
				"Default: Ctrl+Shift+. ");

		const bool dockHover = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		ImGui::PopStyleVar(6);

		if (dockHover)
		{
			gUi.blockGameMouse = true;
			gUi.blockGameKeyboard = true;
			ImGuiIO& io = ImGui::GetIO();
			io.WantCaptureMouse = true;
			io.WantCaptureKeyboard = true;
			ImGui::CaptureMouseFromApp(true);
			ImGui::CaptureKeyboardFromApp(true);
		}
	}
} // namespace UIDetail
