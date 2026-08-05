#include "UI.h"
#include "UIInternal.h"
#include "UI_Browse.h"

#include "BrowserTabs.h"
#include "CharacterProfiles.h"
#include "ConfirmedWaypoints.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "LivePanels.h"
#include "MumbleIdentity.h"
#include "NotesPad.h"
#include "TpWatchPad.h"
#include "LookupPad.h"
#include "WalletPad.h"
#include "VaultPad.h"
#include "AccountPad.h"
#include "EventsPad.h"
#include "LogManagerPad.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "PadNav.h"
#include "CompassOverlay.h"
#include "WorldOverlay.h"
#include "DirectionCompass.h"
#include "SettingsPad.h"
#include "Settings.h"
#include "Sites.h"
#include "UiScale.h"
#include "WikiBrowser.h"
#include "WikiIpc.h"
#include "AddonPaths.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <shellapi.h>

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
			if (ImGui::MenuItem(G::ShowLogManager ? "Hide DPS Logs" : "Show DPS Logs"))
			{
				if (G::ShowLogManager)
				{
					G::ShowLogManager = false;
					Settings::SetDirty();
				}
				else
					LogManagerPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowPathingGuides ? "Hide Pathing" : "Show Pathing"))
			{
				if (G::ShowPathingGuides)
				{
					G::ShowPathingGuides = false;
					Settings::SetDirty();
				}
				else
					PathingGuidesPad::Open();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(G::ShowCompassPad ? "Hide Compass" : "Show Compass"))
			{
				if (G::ShowCompassPad)
				{
					G::ShowCompassPad = false;
					Settings::SetDirty();
				}
				else
					DirectionCompass::Open();
			}
			UI_NoteHelperPopupHover();
			ImGui::EndPopup();
		}
	}

	void DrawToolbar()
	{
		/* Compact nav cluster */
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

	void DrawHelperSideRail()
	{
		static const char* kRailLabels[] = {
			"Browse", "Legendary Ledger", "Cheat Sheets", "GW2 API Check",
			"Account", "Compass", "Pathing", "Events", "Notes", "DPS Logs",
			"Settings"
		};
		const float railW = UiScale::FitSideRailWidth(kRailLabels, 11);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.f, 6.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 4.f));
		/* Allow scroll if the rail is taller than the helper — Settings must stay reachable. */
		ImGui::BeginChild("###gw2igh_helper_rail", ImVec2(railW, 0.f), true,
			ImGuiWindowFlags_NavFlattened);

		if (PadNav::SideToggle("Browse###gw2igh_browse", false))
		{
			G::ShowWiki = true;
			Settings::SetDirty();
			WikiBrowser::Navigate("about:browse-hub");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Browse sites — categories, favorites, open in a new tab");

		if (PadNav::SideToggle("Legendary Ledger###gw2igh_ledger", false))
		{
			G::ShowWiki = true;
			Settings::SetDirty();
			WikiBrowser::Navigate("about:legendary-vault");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("GW2 Legendary Ledger — owned / missing / craft tree");

		if (PadNav::SideToggle("Cheat Sheets###gw2igh_cheatsheets", false))
		{
			G::ShowWiki = true;
			Settings::SetDirty();
			WikiBrowser::Navigate("about:cheatsheets-hub");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Offline cheat sheets — food, fractals, squad tools, …");

		if (PadNav::SideToggle("GW2 API Check###gw2igh_api_check", false))
		{
			G::ShowWiki = true;
			Settings::SetDirty();
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
			WikiBrowser::Navigate("about:gw2-api-check");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Probe official api.guildwars2.com endpoints (public + your key).\n"
				"Local page — not a third-party status site.");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextDisabled("Tools");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (PadNav::SideToggle("Account###gw2igh_account", G::ShowAccount))
		{
			if (G::ShowAccount)
			{
				G::ShowAccount = false;
				Settings::SetDirty();
			}
			else
				AccountPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Account — stash, vault, TP, item lookup");

		if (PadNav::SideToggle("Compass###gw2igh_dircompass", G::ShowCompassPad))
		{
			if (G::ShowCompassPad)
			{
				G::ShowCompassPad = false;
				Settings::SetDirty();
			}
			else
				DirectionCompass::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Direction compass — enable + letter size + radius");

		if (PadNav::SideToggle("Pathing###gw2igh_pathing", G::ShowPathingGuides))
		{
			if (G::ShowPathingGuides)
			{
				G::ShowPathingGuides = false;
				Settings::SetDirty();
			}
			else
				PathingGuidesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Pathing — Tekkit + Lady Elyssa + Hero packs");

		if (PadNav::SideToggle("Events###gw2igh_events", G::ShowEvents))
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

		if (PadNav::SideToggle("Notes###gw2igh_notes", G::ShowNotes))
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
			ImGui::SetTooltip("Snippets + Waypoints search");

		if (PadNav::SideToggle("DPS Logs###gw2igh_logs", G::ShowLogManager))
		{
			if (G::ShowLogManager)
			{
				G::ShowLogManager = false;
				Settings::SetDirty();
			}
			else
				LogManagerPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("DPS Logs — ArcDPS EVTC via Elite Insights");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (PadNav::SideToggle("Settings###gw2igh_settings", G::ShowSettings))
		{
			if (G::ShowSettings)
			{
				G::ShowSettings = false;
				Settings::SetDirty();
			}
			else
				SettingsPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Settings — opacity, font, API key, warm CEF");

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::SameLine(0.f, 8.f);
	}

} // namespace UIDetail
