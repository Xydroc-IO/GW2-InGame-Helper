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
	void DrawHelperSideRail()
	{
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

		/* Width fit must list every rail row (incl. section headers). */
		static const char* kRailLabels[] = {
			"IN-GAME HELPER",
			"Browse", "Ledger", "Sheets", "API Check",
			"Account", "Compass", "Pathing", "Completion", "Farming",
			"Trail Tools", "Events", "Notes", "DPS Logs",
			"Companions",
			"Economy", "Instances",
			"Settings"
		};
		const int nLabels = static_cast<int>(sizeof(kRailLabels) / sizeof(kRailLabels[0]));
		const float railW = UiScale::FitSideRailWidth(kRailLabels, nLabels, 88.f, 160.f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 4.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.f, 2.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 3.f));
		ImGui::BeginChild("###gw2igh_helper_rail", ImVec2(railW, 0.f), true,
			ImGuiWindowFlags_NavFlattened);

		if (Gw2Ui::Image(Gw2Ui::Icon::Hero, 18.f))
			ImGui::SameLine(0.f, 6.f);
		ImGui::TextColored(HelperTheme::GoldBright, "HELPER");
		ImGui::Separator();

		if (PadNav::SideToggle("Browse###gw2igh_browse", false, static_cast<int>(Gw2Ui::Icon::Help)))
			openSiteNewTab("browse");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Browse sites - categories, favorites, open in a new tab");

		if (PadNav::SideToggle("Ledger###gw2igh_ledger", false, static_cast<int>(Gw2Ui::Icon::Achievements)))
			openSiteNewTab("legvault");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("GW2 Legendary Ledger - owned / missing / craft tree");

		if (PadNav::SideToggle("Sheets###gw2igh_cheatsheets", false, static_cast<int>(Gw2Ui::Icon::Story)))
			openUrlNewTab("browse", "about:cheatsheets-hub");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Offline cheat sheets - food, fractals, squad tools, ...");

		if (PadNav::SideToggle("API Check###gw2igh_api_check", false, static_cast<int>(Gw2Ui::Icon::Alert)))
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

		ImGui::Spacing();
		ImGui::TextColored(HelperTheme::GoldDim, "TOOLS");
		ImGui::Separator();

		if (PadNav::SideToggle("Account###gw2igh_account", G::ShowAccount, static_cast<int>(Gw2Ui::Icon::Hero)))
		{
			if (G::ShowAccount) { G::ShowAccount = false; Settings::SetDirty(); }
			else AccountPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Account - stash, vault, TP, item lookup\nDefault: Ctrl+Shift+A (Settings -> Keybinds)");

		if (PadNav::SideToggle("Compass###gw2igh_dircompass", G::ShowCompassPad, static_cast<int>(Gw2Ui::Icon::Map)))
		{
			if (G::ShowCompassPad) { G::ShowCompassPad = false; Settings::SetDirty(); }
			else DirectionCompass::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Direction compass - enable + letter size + radius\nDefault: Ctrl+Shift+O (Settings -> Keybinds)");

		if (PadNav::SideToggle("Pathing###gw2igh_pathing", G::ShowPathingGuides, static_cast<int>(Gw2Ui::Icon::Map)))
		{
			if (G::ShowPathingGuides) { G::ShowPathingGuides = false; Settings::SetDirty(); }
			else PathingGuidesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Pathing - Tekkit + Lady Elyssa + Hero packs\nDefault: Ctrl+Shift+G (Settings -> Keybinds)");

		if (PadNav::SideToggle("Completion###gw2igh_completion", G::ShowCompletion, static_cast<int>(Gw2Ui::Icon::Achievements)))
		{
			if (G::ShowCompletion) { G::ShowCompletion = false; Settings::SetDirty(); }
			else CompletionPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Completion - checklist, Atlas, routes, GPS\nDefault: Ctrl+Shift+M (Settings -> Keybinds)");

		if (PadNav::SideToggle("Farming###gw2igh_farming", G::ShowFarming, static_cast<int>(Gw2Ui::Icon::Bag)))
		{
			if (G::ShowFarming) { G::ShowFarming = false; Settings::SetDirty(); }
			else FarmingPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Farming - run checklists + catch log\nDefault: Ctrl+Shift+R (Settings -> Keybinds)");

		if (PadNav::SideToggle("Trail Tools###gw2igh_trailtools", G::ShowTrailTools, static_cast<int>(Gw2Ui::Icon::Options)))
		{
			if (G::ShowTrailTools) { G::ShowTrailTools = false; Settings::SetDirty(); }
			else TrailToolsPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Trail Tools - author packs\nDefault: Ctrl+Shift+B (Settings -> Keybinds)");

		if (PadNav::SideToggle("Events###gw2igh_events", G::ShowEvents, static_cast<int>(Gw2Ui::Icon::Mail)))
		{
			if (G::ShowEvents) { G::ShowEvents = false; Settings::SetDirty(); }
			else EventsPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("World events - UTC timers + track list\nDefault: Ctrl+Shift+E (Settings -> Keybinds)");

		if (PadNav::SideToggle("Notes###gw2igh_notes", G::ShowNotes, static_cast<int>(Gw2Ui::Icon::Story)))
		{
			if (G::ShowNotes) { G::ShowNotes = false; Settings::SetDirty(); }
			else NotesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Snippets + Waypoints search\nDefault: Ctrl+Shift+N (Settings -> Keybinds)");

		if (PadNav::SideToggle("DPS Logs###gw2igh_logs", G::ShowLogManager, static_cast<int>(Gw2Ui::Icon::PvP)))
		{
			if (G::ShowLogManager) { G::ShowLogManager = false; Settings::SetDirty(); }
			else LogManagerPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("DPS Logs - ArcDPS EVTC via Elite Insights\nDefault: Ctrl+Shift+L (Settings -> Keybinds)");

		ImGui::Spacing();
		ImGui::TextColored(HelperTheme::GoldDim, "COMPANIONS");
		ImGui::Separator();

		if (PadNav::SideToggle("Economy###gw2igh_economy", G::ShowEconomy, static_cast<int>(Gw2Ui::Icon::GoldCoins)))
		{
			if (G::ShowEconomy) { G::ShowEconomy = false; Settings::SetDirty(); }
			else EconomyPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Economy - Flip Finder, charts, cart\nDefault: Ctrl+Shift+Y (Settings -> Keybinds)");

		if (PadNav::SideToggle("Instances###gw2igh_instances", G::ShowInstances, static_cast<int>(Gw2Ui::Icon::Squad)))
		{
			if (G::ShowInstances) { G::ShowInstances = false; Settings::SetDirty(); }
			else InstancesPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Instances - story / fractal / raid / strike\nDefault: Ctrl+Shift+I (Settings -> Keybinds)");

		ImGui::Spacing();
		ImGui::Separator();

		if (PadNav::SideToggle("Settings###gw2igh_settings", G::ShowSettings, static_cast<int>(Gw2Ui::Icon::Options)))
		{
			if (G::ShowSettings) { G::ShowSettings = false; Settings::SetDirty(); }
			else SettingsPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Settings - opacity, font, API key, Keybinds tab\nDefault: Ctrl+Shift+. ");

		ImGui::EndChild();
		ImGui::PopStyleVar(3);
		ImGui::SameLine(0.f, 6.f);
	}
} // namespace UIDetail
