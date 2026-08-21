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
		const float tabRowH = ImGui::GetFrameHeightWithSpacing();
		const float tabBarH = tabRowH + 4.f;
		ImGui::BeginChild("##gw2igh_tab_bar", ImVec2(0.f, tabBarH), false,
			ImGuiWindowFlags_HorizontalScrollbar);
		/* Sit on the content separator — gap above is between bookmark bar and tabs. */
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + tabBarH - tabRowH - 2.f);

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
				std::snprintf(label, sizeof(label), "%s   | ###tab%d", title, i);
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
				UI_NoteHelperPopupHover();
				ImGui::EndPopup();
			}

			if (ImGui::IsItemHovered())
			{
				if (tab.pinned)
					ImGui::SetTooltip("%s (pinned - unpin to close)", title);
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
			const bool plusDup = ImGui::IsItemClicked(ImGuiMouseButton_Middle) ||
				(ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl);
			if (plusDup)
				DuplicateActiveTab();
			else if (plusClicked || UI_Browse_ConsumeNewTabPickerRequest())
				UI_Browse_OnNewTabButtonClicked();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("New Browse tab (Ctrl+T). Ctrl/middle-click duplicates this page.");
		}
		else
		{
			(void)UI_Browse_ConsumeNewTabPickerRequest();
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
		if (u.empty())
			u = Sites::ResolveUrl(Sites::Active());
		BrowserTabs::OpenNewUrl(Sites::ActiveId(), u);
	}

	/* Friendly status - muted gold; hide Ready / closed noise. */

} // namespace UIDetail
