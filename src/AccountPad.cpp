#include "AccountPad.h"

#include "BrowserTabs.h"
#include "CraftingData.h"
#include "Globals.h"
#include "HelperTheme.h"
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
	constexpr float kPadW = 560.f;
	constexpr float kPadH = 700.f;

	bool gFocus = false;
	bool gPlaceOnce = false;

	void OpenLive(const char* url)
	{
		G::ShowWiki = true;
		Settings::SetDirty();
		if (BrowserTabs::OpenNewUrl("live", url) < 0)
			WikiBrowser::Navigate(url);
	}

	void SectionLabel(const char* label)
	{
		ImGui::Spacing();
		ImGui::TextColored(HelperTheme::GoldDim, "%s", label);
		ImGui::Separator();
		ImGui::Spacing();
	}

	void DrawOverview()
	{
		const bool hasKey = G::Gw2ApiKey[0] != '\0';

		ImGui::TextColored(HelperTheme::Gold, "ACCOUNT");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(HelperTheme::Muted,
			"Stash, vault, trading, crafting, and legendary progress — official API, read-only.");
		ImGui::PopTextWrapPos();
		ImGui::Spacing();

		ImGui::BeginChild("###gw2igh_acct_keycard", ImVec2(0.f, hasKey ? 92.f : 118.f), true);
		if (hasKey)
		{
			ImGui::TextColored(HelperTheme::Ok, "API key saved locally");
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextColored(HelperTheme::Muted,
				"Scopes: account · characters · inventories · wallet · tradingpost · progression · unlocks");
			ImGui::PopTextWrapPos();
		}
		else
		{
			ImGui::TextColored(HelperTheme::Warn, "No API key yet");
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextWrapped(
				"Add one under Nexus → Options → GW2 In-Game Helper. "
				"Stash / Vault / delivery / unlocks need it; item lookup & TP prices work without.");
			ImGui::PopTextWrapPos();
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (ImGui::Button("Refresh all###gw2igh_acct_refall", ImVec2(-FLT_MIN, 0.f)))
		{
			WalletPad::RefreshData();
			VaultPad::RefreshData();
			TpWatchPad::RefreshData();
			ProgressData::RefreshIfNeeded(true);
			CraftingData::RefreshDailiesIfNeeded(true);
		}
		ImGui::TextColored(HelperTheme::Muted, "Pulls stash, vault, trading, crafting dailies, and progress.");

		SectionLabel("TOOLS");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(HelperTheme::Muted,
			"Use the tabs above — each tool stays in this window.");
		ImGui::PopTextWrapPos();
		ImGui::Spacing();

		const float gap = ImGui::GetStyle().ItemSpacing.x;
		ImGui::BeginGroup();
		ImGui::TextColored(HelperTheme::GoldMuted, "Stash");
		ImGui::TextColored(HelperTheme::Muted, "Wallet · mats · bank · bags");
		ImGui::EndGroup();
		ImGui::SameLine(0.f, gap);
		ImGui::BeginGroup();
		ImGui::TextColored(HelperTheme::GoldMuted, "Vault");
		ImGui::TextColored(HelperTheme::Muted, "Dailies · Wizard's Vault");
		ImGui::EndGroup();

		ImGui::Spacing();
		ImGui::BeginGroup();
		ImGui::TextColored(HelperTheme::GoldMuted, "Trading");
		ImGui::TextColored(HelperTheme::Muted, "Delivery · watchlist");
		ImGui::EndGroup();
		ImGui::SameLine(0.f, gap);
		ImGui::BeginGroup();
		ImGui::TextColored(HelperTheme::GoldMuted, "Crafting");
		ImGui::TextColored(HelperTheme::Muted, "Dailies · recipe tree");
		ImGui::EndGroup();

		SectionLabel("ALSO AVAILABLE");
		if (ImGui::Button("Fashion (Live)###gw2igh_acct_fash", ImVec2(-FLT_MIN, 0.f)))
			OpenLive("about:live-fashion");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(HelperTheme::Muted,
			"Legendaries live under Progress. Fashion opens the Live panel in Browse.");
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
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.f, 360.f), ImVec2(720.f, maxH));
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

	HelperTheme::ScopedWindow theme(G::Opacity);

	bool open = G::ShowAccount;
	if (!ImGui::Begin("Account###GW2InGameHelperAccount", &open))
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
			ImGui::BeginChild("###gw2igh_acct_body1", ImVec2(0.f, 0.f), true);
			WalletPad::RenderContents();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Vault###gw2igh_acct_tab2"))
		{
			ImGui::BeginChild("###gw2igh_acct_body2", ImVec2(0.f, 0.f), true);
			VaultPad::RenderContents();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Trading###gw2igh_acct_tab3"))
		{
			ImGui::BeginChild("###gw2igh_acct_body3", ImVec2(0.f, 0.f), true);
			TpWatchPad::RenderContents(true);
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Item###gw2igh_acct_tab4"))
		{
			ImGui::BeginChild("###gw2igh_acct_body4", ImVec2(0.f, 0.f), true);
			LookupPad::RenderContents();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		{
			const ImGuiTabItemFlags craftFlags = focusCraft
				? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			if (ImGui::BeginTabItem("Crafting###gw2igh_acct_tab5", nullptr, craftFlags))
			{
				ImGui::BeginChild("###gw2igh_acct_body5", ImVec2(0.f, 0.f), true);
				CraftingData::RenderContents();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
		}
		if (ImGui::BeginTabItem("Progress###gw2igh_acct_tab6"))
		{
			ImGui::BeginChild("###gw2igh_acct_body6", ImVec2(0.f, 0.f), true);
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
