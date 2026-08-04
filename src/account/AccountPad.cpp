#include "AccountPad.h"

#include "CraftingData.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "LookupPad.h"
#include "PadDock.h"
#include "PadNav.h"
#include "ProgressData.h"
#include "SessionHistoryData.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "UnlocksData.h"
#include "UnlocksPad.h"
#include "VaultPad.h"
#include "WalletPad.h"

#include "imgui/imgui.h"

#include <algorithm>

#include <windows.h>

namespace
{
	constexpr float kPadW = 620.f;
	constexpr float kPadH = 740.f;

	bool gFocus = false;
	bool gPlaceOnce = false;
	int  gAccountTab = 0; /* Overview…History — left PadNav side rail */

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
			"Stash, vault, trading, crafting, unlocks, and legendary progress — official API, read-only.");
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
				"Add one under Settings (helper side rail). "
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
			UnlocksData::EnsureAll(true);
		}
		ImGui::TextColored(HelperTheme::Muted,
			"Pulls stash, vault, trading, crafting dailies, progress, and unlocks.");

		SessionHistoryData::RenderOverviewSnippet();

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

		ImGui::Spacing();
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(HelperTheme::Muted,
			"Legendaries live under Progress. Unlocks covers wardrobe skins/dyes/minis.");
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
	UnlocksData::EnsureLoaded(UnlocksData::Kind::Skins, false);
}

bool AccountPad::Render()
{
	if (!G::ShowAccount)
		return false;

	TpWatchPad::Tick();
	UnlocksData::Tick();
	SessionHistoryData::Tick();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(360.f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.f, 360.f), ImVec2(PadDock::MaxW(720.f), maxH));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.34f) : 100.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.1f) : 70.f;
		PadDock::Place(G::PadAccount, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadAccount.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	HelperTheme::ScopedWindow theme(G::Opacity);

	bool open = G::ShowAccount;
	if (!ImGui::Begin("Account###GW2InGameHelperAccount", &open))
	{
		if (PadDock::Capture(G::PadAccount))
			Settings::SetDirty();
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
	if (PadDock::Capture(G::PadAccount))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale;

	if (CraftingData::ConsumeFocusTab())
		gAccountTab = 5; /* Crafting */

	static const char* kTabs[] = {
		"Overview", "Stash", "Vault", "Trading", "Item",
		"Crafting", "Progress", "Unlocks", "History"
	};
	gAccountTab = PadNav::DrawSideRail("###gw2igh_acct_nav", kTabs, 9, gAccountTab);

	ImGui::BeginChild("###gw2igh_acct_body", ImVec2(0.f, 0.f), gAccountTab != 0);
	switch (gAccountTab)
	{
	case 0: DrawOverview(); break;
	case 1: WalletPad::RenderContents(); break;
	case 2: VaultPad::RenderContents(); break;
	case 3: TpWatchPad::RenderContents(true); break;
	case 4: LookupPad::RenderContents(); break;
	case 5: CraftingData::RenderContents(); break;
	case 6:
		ProgressData::RefreshIfNeeded(false);
		ProgressData::RenderContents();
		break;
	case 7: UnlocksPad::RenderContents(); break;
	case 8: SessionHistoryData::RenderContents(); break;
	default: break;
	}
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
