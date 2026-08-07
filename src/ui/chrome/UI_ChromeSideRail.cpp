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
				ImGui::GetWindowDrawList()->AddRectFilled(
					ImVec2(p.x + 2.f, p.y + 2.f),
					ImVec2(p.x + w - 2.f, p.y + 3.5f),
					ImGui::GetColorU32(HelperTheme::GoldDim));
				ImGui::Dummy(ImVec2(1.f, 6.f));
			}
		}
	}

	/* Permanent icon dock locked to the left edge of the helper (separate window,
	   not nested). Height fits content — ends at the last icon. */
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
		float railW = 0.f;
		if (labels)
		{
			static const char* kRailLabels[] = {
				"HELPER",
				"Browse", "Ledger", "Sheets", "API Check",
				"TOOLS",
				"Account", "Compass", "Pathing", "Completion", "Farming",
				"Trail Tools", "Events", "Notes", "DPS Logs",
				"COMPANIONS",
				"Economy", "Instances",
				"Settings"
			};
			const int nLabels = static_cast<int>(sizeof(kRailLabels) / sizeof(kRailLabels[0]));
			railW = UiScale::FitSideRailWidth(kRailLabels, nLabels, 108.f, 200.f, 18.f);
		}
		else
			railW = UiScale::IconRailWidth(26.f);

		const ImVec2 helperMin = gUi.wikiMin;
		const ImVec2 helperMax = gUi.wikiMax;
		const float helperH = helperMax.y - helperMin.y;
		const float dockX = helperMin.x - railW;
		/* Vertically center against the helper; use last-frame dock height. */
		static float sDockH = 480.f;
		float dockY = helperMin.y + (helperH - sDockH) * 0.5f;
		if (dockY < helperMin.y)
			dockY = helperMin.y;
		if (helperH > 1.f && dockY + sDockH > helperMax.y)
			dockY = helperMax.y - sDockH;

		ImGui::SetNextWindowPos(ImVec2(dockX, dockY), ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(railW, 48.f), ImVec2(railW, 4000.f));
		ImGui::SetNextWindowBgAlpha(0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(labels ? 4.f : 2.f, 4.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f, 2.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(labels ? 4.f : 2.f, labels ? 3.f : 2.f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.026f, 0.018f, 0.96f * G::Opacity));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);

		const ImGuiWindowFlags dockFlags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoFocusOnAppearing;

		ImGui::Begin("##gw2igh_side_dock", nullptr, dockFlags);

		/* Gold edge seams on the plaque. */
		{
			const ImVec2 p0 = ImGui::GetWindowPos();
			const ImVec2 sz = ImGui::GetWindowSize();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImU32 seam = ImGui::GetColorU32(HelperTheme::GoldDim);
			dl->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p0.x + 1.5f, p0.y + sz.y), seam);
			dl->AddRectFilled(ImVec2(p0.x + sz.x - 1.5f, p0.y), ImVec2(p0.x + sz.x, p0.y + sz.y), seam);
			dl->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p0.x + sz.x, p0.y + 1.5f), seam);
			dl->AddRectFilled(ImVec2(p0.x, p0.y + sz.y - 1.5f), ImVec2(p0.x + sz.x, p0.y + sz.y), seam);
		}

		if (labels)
		{
			ImGui::TextColored(HelperTheme::GoldBright, "HELPER");
			ImGui::Separator();
		}

		if (PadNav::SideToggle("Browse###gw2igh_browse", false, static_cast<int>(Gw2Ui::Icon::BrowseInfo)))
			openSiteInActive("browse");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Browse sites - categories, favorites (current tab)");

		if (PadNav::SideToggle("Ledger###gw2igh_ledger", false, static_cast<int>(Gw2Ui::Icon::LedgerCoins)))
			openSiteNewTab("legvault");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("GW2 Legendary Ledger - owned / missing / craft tree");

		if (PadNav::SideToggle("Sheets###gw2igh_cheatsheets", false, static_cast<int>(Gw2Ui::Icon::SheetsBook)))
			openUrlNewTab("browse", "about:cheatsheets-hub");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Offline cheat sheets - food, fractals, squad tools, ...");

		if (PadNav::SideToggle("API Check###gw2igh_api_check", false, static_cast<int>(Gw2Ui::Icon::ApiHourglass)))
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

		RailSectionGap(labels, "TOOLS");

		if (PadNav::SideToggle("Account###gw2igh_account", G::ShowAccount, static_cast<int>(Gw2Ui::Icon::AccountSword)))
		{
			if (G::ShowAccount) { G::ShowAccount = false; Settings::SetDirty(); }
			else AccountPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Account - stash, vault, TP, item lookup\nDefault: Ctrl+Shift+A (Settings -> Keybinds)");

		if (PadNav::SideToggle("Compass###gw2igh_dircompass", G::ShowCompassPad, static_cast<int>(Gw2Ui::Icon::CompassRadar)))
		{
			if (G::ShowCompassPad) { G::ShowCompassPad = false; Settings::SetDirty(); }
			else DirectionCompass::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Direction compass - enable + letter size + radius\nDefault: Ctrl+Shift+O (Settings -> Keybinds)");

		if (PadNav::SideToggle("Pathing###gw2igh_pathing", G::ShowPathingGuides, static_cast<int>(Gw2Ui::Icon::PathingMap)))
		{
			if (G::ShowPathingGuides) { G::ShowPathingGuides = false; Settings::SetDirty(); }
			else PathingGuidesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Pathing - Tekkit + Lady Elyssa + Hero packs\nDefault: Ctrl+Shift+G (Settings -> Keybinds)");

		if (PadNav::SideToggle("Completion###gw2igh_completion", G::ShowCompletion, static_cast<int>(Gw2Ui::Icon::CompletePeak)))
		{
			if (G::ShowCompletion) { G::ShowCompletion = false; Settings::SetDirty(); }
			else CompletionPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Completion - checklist, Atlas, routes, GPS\nDefault: Ctrl+Shift+M (Settings -> Keybinds)");

		if (PadNav::SideToggle("Farming###gw2igh_farming", G::ShowFarming, static_cast<int>(Gw2Ui::Icon::FarmSack)))
		{
			if (G::ShowFarming) { G::ShowFarming = false; Settings::SetDirty(); }
			else FarmingPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Farming - run checklists + catch log\nDefault: Ctrl+Shift+R (Settings -> Keybinds)");

		if (PadNav::SideToggle("Trail Tools###gw2igh_trailtools", G::ShowTrailTools, static_cast<int>(Gw2Ui::Icon::TrailAnvil)))
		{
			if (G::ShowTrailTools) { G::ShowTrailTools = false; Settings::SetDirty(); }
			else TrailToolsPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Trail Tools - author packs\nDefault: Ctrl+Shift+B (Settings -> Keybinds)");

		if (PadNav::SideToggle("Events###gw2igh_events", G::ShowEvents, static_cast<int>(Gw2Ui::Icon::EventsMedal)))
		{
			if (G::ShowEvents) { G::ShowEvents = false; Settings::SetDirty(); }
			else EventsPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("World events - UTC timers + track list\nDefault: Ctrl+Shift+E (Settings -> Keybinds)");

		if (PadNav::SideToggle("Notes###gw2igh_notes", G::ShowNotes, static_cast<int>(Gw2Ui::Icon::NotesScroll)))
		{
			if (G::ShowNotes) { G::ShowNotes = false; Settings::SetDirty(); }
			else NotesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Snippets + Waypoints search\nDefault: Ctrl+Shift+N (Settings -> Keybinds)");

		if (PadNav::SideToggle("DPS Logs###gw2igh_logs", G::ShowLogManager, static_cast<int>(Gw2Ui::Icon::LogsSwords)))
		{
			if (G::ShowLogManager) { G::ShowLogManager = false; Settings::SetDirty(); }
			else LogManagerPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("DPS Logs - ArcDPS EVTC via Elite Insights\nDefault: Ctrl+Shift+L (Settings -> Keybinds)");

		RailSectionGap(labels, "COMPANIONS");

		if (PadNav::SideToggle("Economy###gw2igh_economy", G::ShowEconomy, static_cast<int>(Gw2Ui::Icon::EconStack)))
		{
			if (G::ShowEconomy) { G::ShowEconomy = false; Settings::SetDirty(); }
			else EconomyPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Economy - Flip Finder, charts, cart\nDefault: Ctrl+Shift+Y (Settings -> Keybinds)");

		if (PadNav::SideToggle("Instances###gw2igh_instances", G::ShowInstances, static_cast<int>(Gw2Ui::Icon::InstGate)))
		{
			if (G::ShowInstances) { G::ShowInstances = false; Settings::SetDirty(); }
			else InstancesPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Instances - story / fractal / raid / strike\nDefault: Ctrl+Shift+I (Settings -> Keybinds)");

		RailSectionGap(labels, nullptr);

		if (PadNav::SideToggle("Settings###gw2igh_settings", G::ShowSettings, static_cast<int>(Gw2Ui::Icon::SettingsGear)))
		{
			if (G::ShowSettings) { G::ShowSettings = false; Settings::SetDirty(); }
			else SettingsPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Settings - opacity, font, API key, Keybinds tab\nDefault: Ctrl+Shift+. ");

		const bool dockHover = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		sDockH = ImGui::GetWindowSize().y;
		if (sDockH < 48.f)
			sDockH = 48.f;
		ImGui::End();
		ImGui::PopStyleVar(6);
		ImGui::PopStyleColor(1);

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
