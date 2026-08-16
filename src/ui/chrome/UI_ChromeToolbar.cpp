#include "UI.h"
#include "UIInternal.h"
#include "UI_Browse.h"

#include "BrowserTabs.h"
#include "CompletionPad.h"
#include "DirectionCompass.h"
#include "EventsPad.h"
#include "CraftingPad.h"
#include "FarmingPad.h"
#include "Globals.h"
#include "LogManagerPad.h"
#include "PathingGuidesPad.h"
#include "Settings.h"
#include "WalletPad.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

namespace UIDetail
{
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
				if (G::ShowEvents) { G::ShowEvents = false; Settings::SetDirty(); }
				else EventsPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowLogManager ? "Hide DPS Logs" : "Show DPS Logs"))
			{
				if (G::ShowLogManager) { G::ShowLogManager = false; Settings::SetDirty(); }
				else LogManagerPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowPathingGuides ? "Hide Pathing" : "Show Pathing"))
			{
				if (G::ShowPathingGuides) { G::ShowPathingGuides = false; Settings::SetDirty(); }
				else PathingGuidesPad::Open();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(G::ShowAchievements ? "Hide Achievements" : "Show Achievements"))
			{
				if (G::ShowAchievements) { G::ShowAchievements = false; Settings::SetDirty(); }
				else CompletionPad::OpenAchievements();
			}
			if (ImGui::MenuItem(G::ShowWallet ? "Hide Stash" : "Show Stash"))
			{
				if (G::ShowWallet) { G::ShowWallet = false; Settings::SetDirty(); }
				else WalletPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowCrafting ? "Hide Crafting" : "Show Crafting"))
			{
				if (G::ShowCrafting) { G::ShowCrafting = false; Settings::SetDirty(); }
				else CraftingPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowFarming ? "Hide Farming" : "Show Farming"))
			{
				if (G::ShowFarming) { G::ShowFarming = false; Settings::SetDirty(); }
				else FarmingPad::OpenAndRefresh();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(G::ShowCompassPad ? "Hide Compass" : "Show Compass"))
			{
				if (G::ShowCompassPad) { G::ShowCompassPad = false; Settings::SetDirty(); }
				else DirectionCompass::Open();
			}
			UI_NoteHelperPopupHover();
			ImGui::EndPopup();
		}
	}

	void DrawToolbar()
	{
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
		ImGui::PopStyleVar();

		ImGui::SameLine(0.f, 12.f);
		{
			float avail = ImGui::GetContentRegionAvail().x - 200.f;
			if (avail < 140.f) avail = 140.f;
			if (avail > 420.f) avail = 420.f;
			ImGui::SetNextItemWidth(avail);
		}
		if (ImGui::InputTextWithHint("###gw2igh_site_query", "Find in page...", G::LastQuery, sizeof(G::LastQuery),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if (G::LastQuery[0])
			{
				sShowFind = true;
				std::snprintf(sFindQuery, sizeof(sFindQuery), "%s", G::LastQuery);
				WikiBrowser::Find(sFindQuery, true, sFindMatchCase, false);
				Settings::SetDirty();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Find text on this page. Enter = next match.\nUse Web for DuckDuckGo / site search.");

		ImGui::SameLine(0.f, 4.f);
		if (UI_Browse_ToolbarFavoriteToggle())
			Settings::SetDirty();

		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Reload###gw2igh_reload"))
			BrowserTabs::Reload();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Reload");

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
	}

} // namespace UIDetail
