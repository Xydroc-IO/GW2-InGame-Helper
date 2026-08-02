#include "AccountPad.h"

#include "BrowserTabs.h"
#include "CraftingData.h"
#include "Globals.h"
#include "LookupPad.h"
#include "ProgressData.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "VaultPad.h"
#include "WalletPad.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <algorithm>

#include <windows.h>

namespace
{
	constexpr float kPadW = 500.f;
	constexpr float kPadH = 640.f;

	bool gFocus = false;
	bool gPlaceOnce = false;

	void OpenLive(const char* url)
	{
		G::ShowWiki = true;
		Settings::SetDirty();
		if (BrowserTabs::OpenNewUrl("live", url) < 0)
			WikiBrowser::Navigate(url);
	}

	void DrawOverview()
	{
		const bool hasKey = G::Gw2ApiKey[0] != '\0';

		ImGui::TextUnformatted("Account overview");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
			"Your stash, vault, trading, crafting, and legendary progress in one place. "
			"Uses your official GW2 API key from Nexus Options — read-only.");
		ImGui::PopTextWrapPos();
		ImGui::Spacing();

		if (hasKey)
		{
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f),
				"API key saved locally.");
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
				"Useful scopes: account, characters, inventories, wallet, "
				"tradingpost, progression, unlocks.");
			ImGui::PopTextWrapPos();
		}
		else
		{
			ImGui::TextColored(ImVec4(0.90f, 0.55f, 0.40f, 1.f),
				"No API key yet.");
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextWrapped(
				"Add one under Nexus → Options → GW2 In-Game Helper. "
				"Stash / Vault / delivery / unlocks need it; item lookup & TP prices work without.");
			ImGui::PopTextWrapPos();
		}

		ImGui::Spacing();
		if (ImGui::Button("Refresh account data###gw2igh_acct_refall"))
		{
			WalletPad::RefreshData();
			VaultPad::RefreshData();
			TpWatchPad::RefreshData();
			ProgressData::RefreshIfNeeded(true);
			CraftingData::RefreshDailiesIfNeeded(true);
		}
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"Use the tabs for each tool.");

		ImGui::Separator();
		ImGui::TextUnformatted("Also available");
		if (ImGui::Button("Fashion (Live)###gw2igh_acct_fash"))
			OpenLive("about:live-fashion");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"Legendaries live under the Progress tab. Fashion still opens our Live panel.");
		ImGui::PopTextWrapPos();
	}
}

void AccountPad::OpenAndRefresh()
{
	G::ShowAccount = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	WalletPad::RefreshData();
	VaultPad::RefreshData();
	TpWatchPad::RefreshData();
	ProgressData::RefreshIfNeeded(false);
	CraftingData::RefreshDailiesIfNeeded(false);
}

bool AccountPad::Render()
{
	if (!G::ShowAccount)
		return false;

	TpWatchPad::Tick();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = (io.DisplaySize.y > 100.f)
		? (std::min)(io.DisplaySize.y * 0.92f, 960.f)
		: 720.f;
	ImGui::SetNextWindowSizeConstraints(ImVec2(420.f, 340.f), ImVec2(680.f, maxH));
	if (gPlaceOnce)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	else
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gPlaceOnce)
	{
		const float x = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x * 0.34f : 100.f;
		const float y = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y * 0.10f : 70.f;
		ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
		ImGui::SetNextWindowFocus();
		gPlaceOnce = false;
	}
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowAccount;
	if (!ImGui::Begin("Account##GW2InGameHelperAccount", &open))
	{
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowAccount = false;
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowAccount = false;
		Settings::SetDirty();
	}

	const bool focusCraft = CraftingData::ConsumeFocusTab();
	if (ImGui::BeginTabBar("###gw2igh_acct_tabs", ImGuiTabBarFlags_FittingPolicyScroll))
	{
		if (ImGui::BeginTabItem("Overview###gw2igh_acct_tab0"))
		{
			ImGui::BeginChild("###gw2igh_acct_body0", ImVec2(0.f, 0.f), false);
			DrawOverview();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Stash###gw2igh_acct_tab1"))
		{
			ImGui::BeginChild("###gw2igh_acct_body1", ImVec2(0.f, 0.f), false);
			WalletPad::RenderContents();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Vault###gw2igh_acct_tab2"))
		{
			ImGui::BeginChild("###gw2igh_acct_body2", ImVec2(0.f, 0.f), false);
			VaultPad::RenderContents();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Trading###gw2igh_acct_tab3"))
		{
			ImGui::BeginChild("###gw2igh_acct_body3", ImVec2(0.f, 0.f), false);
			TpWatchPad::RenderContents(true);
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Item###gw2igh_acct_tab4"))
		{
			ImGui::BeginChild("###gw2igh_acct_body4", ImVec2(0.f, 0.f), false);
			LookupPad::RenderContents();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		{
			const ImGuiTabItemFlags craftFlags = focusCraft
				? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			if (ImGui::BeginTabItem("Crafting###gw2igh_acct_tab5", nullptr, craftFlags))
			{
				ImGui::BeginChild("###gw2igh_acct_body5", ImVec2(0.f, 0.f), false);
				CraftingData::RenderContents();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
		}
		if (ImGui::BeginTabItem("Progress###gw2igh_acct_tab6"))
		{
			ImGui::BeginChild("###gw2igh_acct_body6", ImVec2(0.f, 0.f), false);
			ProgressData::RefreshIfNeeded(false);
			ProgressData::RenderContents();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
