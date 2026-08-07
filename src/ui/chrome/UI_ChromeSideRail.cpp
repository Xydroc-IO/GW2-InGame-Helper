#include "UI.h"
#include "UIInternal.h"

#include "AccountPad.h"
#include "AddonPaths.h"
#include "BrowserTabs.h"
#include "CompletionPad.h"
#include "DirectionCompass.h"
#include "EconomyPad.h"
#include "EventsPad.h"
#include "FarmingPad.h"
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
#include "TrailToolsPad.h"
#include "UiScale.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <windows.h>

#include <algorithm>
#include <string>

namespace UIDetail
{
	namespace
	{
		void RailSectionGap(bool labels, const char* title)
		{
			ImGui::Spacing();
			if (labels && title && title[0])
			{
				ImGui::TextColored(HelperTheme::GoldDim, "%s", title);
				ImGui::Separator();
			}
			else
			{
				const ImVec2 p = ImGui::GetCursorScreenPos();
				const float w = ImGui::GetContentRegionAvail().x;
				/* Soft native hairline — not a hard gold rule. */
				ImGui::GetWindowDrawList()->AddRectFilled(
					ImVec2(p.x + 4.f, p.y + 2.f),
					ImVec2(p.x + w - 4.f, p.y + 3.f),
					IM_COL32(161, 120, 56, 90));
				ImGui::Dummy(ImVec2(1.f, 6.f));
			}
		}

		/* Extra vertical gap so the icon stack lands on the helper bottom.
		   Cursor bump only — avoids an ImGui item (and its ItemSpacing). */
		void RailStretchGap(float stretch)
		{
			if (stretch > 0.5f)
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + stretch);
		}

		/* Must match Gw2Ui::RailToggle icon-only / labeled height. */
		float RailBtnHeight(float iconSz, bool labels, float framePadY)
		{
			if (labels)
			{
				const float padY = framePadY * 2.f;
				return iconSz + padY;
			}
			/* Icon dock: pad scales with icon so short windows can still fit. */
			const float padY = (std::max)(2.f, iconSz * 0.12f);
			return iconSz + padY;
		}

		float RailItemSpacing(float iconSz, bool labels)
		{
			if (iconSz < (labels ? 22.f : 28.f))
				return 1.f;
			if (iconSz < (labels ? 30.f : 40.f))
				return 2.f;
			return 4.f;
		}

		float RailFramePadY(float iconSz, bool labels)
		{
			if (labels)
				return iconSz >= 30.f ? 5.f : 3.f;
			return iconSz >= 40.f ? 4.f : 2.f;
		}

		float RailSectionHeight(float itemSp, bool labels)
		{
			/* Spacing() + hairline Dummy(6) — or labeled title + Separator. */
			if (labels)
				return itemSp + ImGui::GetTextLineHeight() + itemSp + 4.f + itemSp;
			return itemSp + 6.f + itemSp;
		}

		float RailPadY(bool labels)
		{
			/* Same inset top and bottom — keep Settings off the rail floor. */
			return labels ? 6.f : 8.f;
		}

		float RailPackedHeight(float iconSz, bool labels, float itemSp, float framePadY)
		{
			constexpr int kBtns = 16;
			constexpr int kGaps = 15;
			const float padY = RailPadY(labels);
			const float btnH = RailBtnHeight(iconSz, labels, framePadY);
			/* Two titled sections (TOOLS / COMPANIONS) + one hairline before Settings. */
			const float sectionH = labels
				? (2.f * RailSectionHeight(itemSp, true) + RailSectionHeight(itemSp, false))
				: (3.f * RailSectionHeight(itemSp, false));
			float h = padY * 2.f
				+ static_cast<float>(kBtns) * btnH
				+ static_cast<float>(kGaps) * itemSp
				+ sectionH;
			/* Labeled mode adds HELPER header above the first section. */
			if (labels)
				h += ImGui::GetTextLineHeight() + itemSp + 4.f + itemSp;
			return h;
		}

		/*
		 * Pick the largest icon that fits dockH with no scrolling.
		 * Never floor above what fits — user must not scroll the nav on resize.
		 */
		float FitRailIconSize(float dockH, bool labels)
		{
			constexpr float kMaxIcon = 52.f;
			constexpr float kMinIcon = 12.f;
			const float maxIcon = labels ? 36.f : kMaxIcon;
			/* Extra bottom inset matching top WindowPadding (see DrawHelperSideRail). */
			const float bottomExtra = RailPadY(labels);
			for (float icon = maxIcon; icon >= kMinIcon - 0.5f; icon -= 1.f)
			{
				const float sz = (std::max)(kMinIcon, icon);
				const float itemSp = RailItemSpacing(sz, labels);
				const float fp = RailFramePadY(sz, labels);
				if (RailPackedHeight(sz, labels, itemSp, fp) + bottomExtra <= dockH + 0.5f)
					return sz;
			}
			return kMinIcon;
		}
	}

	/* Permanent left nav column (locked to helper; not a floating dock panel).
	   Outer gold edge on the left; open join into the helper body on the right. */
	float HelperSideRailWidth()
	{
		if (G::ShowRailLabels)
		{
			static const char* kRailLabels[] = {
				"HELPER",
				"Browse", "Ledger", "Sheets", "API Check",
				"TOOLS",
				"Account", "Compass", "Pathing", "Completion", "Farming",
				"Events", "Notes", "DPS Logs", "Trail Tools",
				"COMPANIONS",
				"Economy", "Instances",
				"Settings"
			};
			const int nLabels = static_cast<int>(sizeof(kRailLabels) / sizeof(kRailLabels[0]));
			return UiScale::FitSideRailWidth(kRailLabels, nLabels, 140.f, 300.f, 36.f);
		}
		return UiScale::IconRailWidth(52.f);
	}

	void DrawHelperSideRail()
	{
		if (!G::ShowWiki || !gUi.wikiRectValid)
			return;

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
		auto openUrlNewTab = [](const char* siteId, const char* url) {
			G::ShowWiki = true;
			Settings::SetDirty();
			if (!url || !url[0])
				return;
			if (BrowserTabs::OpenNewUrl(siteId && siteId[0] ? siteId : "browse", url) < 0)
				WikiBrowser::Navigate(url);
		};

		const bool labels = G::ShowRailLabels;
		const float railW = HelperSideRailWidth();

		const ImVec2 helperMin = gUi.wikiMin;
		const ImVec2 helperMax = gUi.wikiMax;
		const float dockX = helperMin.x - railW;
		/* Align under the pad title bar. */
		constexpr float kTitleBarH = 60.f;
		const float dockY = helperMin.y + kTitleBarH;
		float dockH = helperMax.y - dockY;
		if (dockH < 48.f)
			dockH = 48.f;

		/*
		 * Auto-size icons to the helper height — always fit, never scroll the nav.
		 * Tall windows: keep max icon + stretch gaps. Short: shrink icons + pad.
		 */
		constexpr int kStretchGaps = 15;
		const float iconSz = FitRailIconSize(dockH, labels);
		const float padY = RailPadY(labels);
		const float itemSp = RailItemSpacing(iconSz, labels);
		const float fp = RailFramePadY(iconSz, labels);
		const float packedH = RailPackedHeight(iconSz, labels, itemSp, fp);
		/* Same clear space under Settings as WindowPadding above Browse. */
		const float bottomExtra = padY;
		float stretch = 0.f;
		if (dockH > packedH + bottomExtra + 1.f)
			stretch = (dockH - packedH - bottomExtra) / static_cast<float>(kStretchGaps);
		stretch = static_cast<float>(static_cast<int>(stretch + 0.5f));

		ImGui::SetNextWindowPos(ImVec2(dockX, dockY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(railW, dockH), ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(railW, 48.f), ImVec2(railW, dockH + 8.f));
		ImGui::SetNextWindowBgAlpha(0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(labels ? 6.f : 4.f, padY));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.f, itemSp));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(labels ? 6.f : 4.f, fp));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);

		const ImGuiWindowFlags dockFlags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoFocusOnAppearing |
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
			const ImU32 under = IM_COL32(10, 8, 6, static_cast<int>(oa * 168.f + 0.5f));
			const ImU32 veil = IM_COL32(6, 4, 3, static_cast<int>(oa * 36.f + 0.5f));
			const ImU32 washCol = IM_COL32(255, 255, 255, static_cast<int>(oa * 205.f + 0.5f));
			const float helperW = (std::max)(120.f, gUi.wikiMax.x - gUi.wikiMin.x);
			const float helperX = gUi.wikiMin.x;

			railDl->PushClipRect(rp0, rp1, false);
			railDl->AddRectFilled(rp0, rp1, under);
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
			Texture_t* edge = Gw2UiDetail::GetChromeNamed("panel-edge");
			if (edge && edge->Resource)
			{
				const ImTextureID eid = reinterpret_cast<ImTextureID>(edge->Resource);
				const ImU32 joinCol = IM_COL32(255, 255, 255, static_cast<int>(oa * 140.f + 0.5f));
				constexpr float kJoin = 6.f;
				railDl->PushClipRect(
					ImVec2(rp1.x - kJoin - 1.f, rp0.y),
					ImVec2(rp1.x + 3.f, rp1.y),
					false);
				railDl->AddImageQuad(eid,
					ImVec2(rp1.x - kJoin, rp0.y),
					ImVec2(rp1.x + 2.f, rp0.y),
					ImVec2(rp1.x + 2.f, rp1.y),
					ImVec2(rp1.x - kJoin, rp1.y),
					ImVec2(0.f, 0.f), ImVec2(0.f, 1.f),
					ImVec2(1.f, 1.f), ImVec2(1.f, 0.f),
					joinCol);
				railDl->PopClipRect();
			}
		}

		if (labels)
		{
			ImGui::TextColored(HelperTheme::GoldBright, "HELPER");
			ImGui::Separator();
		}

		if (PadNav::SideToggle("Browse###gw2igh_browse", false, static_cast<int>(Gw2Ui::Icon::BrowseInfo), iconSz))
			openSiteInActive("browse");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Browse sites - categories, favorites (current tab)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Ledger###gw2igh_ledger", false, static_cast<int>(Gw2Ui::Icon::LedgerCoins), iconSz))
			openSiteNewTab("legvault");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("GW2 Legendary Ledger - owned / missing / craft tree");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Sheets###gw2igh_cheatsheets", false, static_cast<int>(Gw2Ui::Icon::SheetsBook), iconSz))
			openUrlNewTab("browse", "about:cheatsheets-hub");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Offline cheat sheets - food, fractals, squad tools, ...");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("API Check###gw2igh_api_check", false, static_cast<int>(Gw2Ui::Icon::ApiHourglass), iconSz))
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
			openUrlNewTab("browse", "about:gw2-api-check");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Probe official api.guildwars2.com endpoints (public + your key).\n"
				"Local page - not a third-party status site.");
		RailStretchGap(stretch);

		RailSectionGap(labels, "TOOLS");

		if (PadNav::SideToggle("Account###gw2igh_account", G::ShowAccount, static_cast<int>(Gw2Ui::Icon::AccountSword), iconSz))
		{
			if (G::ShowAccount) { G::ShowAccount = false; Settings::SetDirty(); }
			else AccountPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Account - stash, vault, TP, item lookup\nDefault: Ctrl+Shift+A (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Compass###gw2igh_dircompass", G::ShowCompassPad, static_cast<int>(Gw2Ui::Icon::CompassRadar), iconSz))
		{
			if (G::ShowCompassPad) { G::ShowCompassPad = false; Settings::SetDirty(); }
			else DirectionCompass::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Direction compass - enable + letter size + radius\nDefault: Ctrl+Shift+O (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Pathing###gw2igh_pathing", G::ShowPathingGuides, static_cast<int>(Gw2Ui::Icon::PathingMap), iconSz))
		{
			if (G::ShowPathingGuides) { G::ShowPathingGuides = false; Settings::SetDirty(); }
			else PathingGuidesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Pathing - Tekkit + Lady Elyssa + Hero packs\nDefault: Ctrl+Shift+G (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Completion###gw2igh_completion", G::ShowCompletion, static_cast<int>(Gw2Ui::Icon::CompletePeak), iconSz))
		{
			if (G::ShowCompletion) { G::ShowCompletion = false; Settings::SetDirty(); }
			else CompletionPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Completion - checklist, Atlas, routes, GPS\nDefault: Ctrl+Shift+M (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Farming###gw2igh_farming", G::ShowFarming, static_cast<int>(Gw2Ui::Icon::FarmSack), iconSz))
		{
			if (G::ShowFarming) { G::ShowFarming = false; Settings::SetDirty(); }
			else FarmingPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Farming - run checklists + catch log\nDefault: Ctrl+Shift+R (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Events###gw2igh_events", G::ShowEvents, static_cast<int>(Gw2Ui::Icon::EventsMedal), iconSz))
		{
			if (G::ShowEvents) { G::ShowEvents = false; Settings::SetDirty(); }
			else EventsPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("World events - UTC timers + track list\nDefault: Ctrl+Shift+E (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Notes###gw2igh_notes", G::ShowNotes, static_cast<int>(Gw2Ui::Icon::NotesScroll), iconSz))
		{
			if (G::ShowNotes) { G::ShowNotes = false; Settings::SetDirty(); }
			else NotesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Snippets + Waypoints search\nDefault: Ctrl+Shift+N (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("DPS Logs###gw2igh_logs", G::ShowLogManager, static_cast<int>(Gw2Ui::Icon::LogsSwords), iconSz))
		{
			if (G::ShowLogManager) { G::ShowLogManager = false; Settings::SetDirty(); }
			else LogManagerPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("DPS Logs - ArcDPS EVTC via Elite Insights\nDefault: Ctrl+Shift+L (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Trail Tools###gw2igh_trailtools", G::ShowTrailTools, static_cast<int>(Gw2Ui::Icon::TrailAnvil), iconSz))
		{
			if (G::ShowTrailTools) { G::ShowTrailTools = false; Settings::SetDirty(); }
			else TrailToolsPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Trail Tools - author packs\nDefault: Ctrl+Shift+B (Settings -> Keybinds)");
		RailStretchGap(stretch);

		RailSectionGap(labels, "COMPANIONS");

		if (PadNav::SideToggle("Economy###gw2igh_economy", G::ShowEconomy, static_cast<int>(Gw2Ui::Icon::EconStack), iconSz))
		{
			if (G::ShowEconomy) { G::ShowEconomy = false; Settings::SetDirty(); }
			else EconomyPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Economy - Flip Finder, charts, cart\nDefault: Ctrl+Shift+Y (Settings -> Keybinds)");
		RailStretchGap(stretch);

		if (PadNav::SideToggle("Instances###gw2igh_instances", G::ShowInstances, static_cast<int>(Gw2Ui::Icon::InstGate), iconSz))
		{
			if (G::ShowInstances) { G::ShowInstances = false; Settings::SetDirty(); }
			else InstancesPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Instances - story / fractal / raid / strike\nDefault: Ctrl+Shift+I (Settings -> Keybinds)");
		RailStretchGap(stretch);

		RailSectionGap(labels, nullptr);

		if (PadNav::SideToggle("Settings###gw2igh_settings", G::ShowSettings, static_cast<int>(Gw2Ui::Icon::SettingsGear), iconSz))
		{
			if (G::ShowSettings) { G::ShowSettings = false; Settings::SetDirty(); }
			else SettingsPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Settings - opacity, font, API key, Keybinds tab\nDefault: Ctrl+Shift+. ");

		/* Match top WindowPadding at the bottom — stretch must not land Settings flush. */
		if (bottomExtra > 0.5f)
			ImGui::Dummy(ImVec2(1.f, bottomExtra));

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
