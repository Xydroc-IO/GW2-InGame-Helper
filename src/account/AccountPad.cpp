#include "AccountPad.h"

#include "CraftingData.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Ui.h"
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
	constexpr float kPadW = PadDock::kWorkbenchW;
	constexpr float kPadH = PadDock::kWorkbenchH;

	bool gFocus = false;
	bool gPlaceOnce = false;
	int  gAccountTab = 0; /* Overview...History - left PadNav side rail */

	void SectionLabel(const char* label)
	{
		ImGui::Spacing();
		PadNav::SectionTitle(label);
		ImGui::Separator();
		ImGui::Spacing();
	}

	void DrawOverview()
	{
		const bool hasKey = G::Gw2ApiKey[0] != '\0';

		PadNav::Blurb(
			"Stash, vault, trading, crafting, unlocks, and legendary progress - official API, read-only.");
		ImGui::Spacing();

		ImGui::BeginChild("###gw2igh_acct_keycard", ImVec2(0.f, hasKey ? 110.f : 140.f), true);
		if (hasKey)
		{
			ImGui::TextColored(HelperTheme::Ok, "API key saved locally");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Scopes: account | characters | inventories | wallet | tradingpost | progression | unlocks");
			PadNav::PopWrap();
		}
		else
		{
			ImGui::TextColored(HelperTheme::Warn, "No API key yet");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Add one under Settings (helper side rail). "
				"Stash / Vault / delivery / unlocks need it; item lookup & TP prices work without.");
			PadNav::PopWrap();
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (Gw2Ui::IconLabelButton("Refresh all###gw2igh_acct_refall", Gw2Ui::Icon::Bag, 18.f))
		{
			WalletPad::RefreshData();
			VaultPad::RefreshData();
			TpWatchPad::RefreshData();
			ProgressData::RefreshIfNeeded(true);
			CraftingData::RefreshDailiesIfNeeded(true);
			UnlocksData::EnsureAll(true);
		}
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Pulls stash, vault, trading, crafting dailies, progress, and unlocks.");
		PadNav::PopWrap();

		SessionHistoryData::RenderOverviewSnippet();

		SectionLabel("TOOLS");
		/* TOOLS → PadNav section gold (not GoldDim) */
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Use the tabs on the left - each tool stays in this window.");
		PadNav::PopWrap();
		ImGui::Spacing();

		const float gap = ImGui::GetStyle().ItemSpacing.x;
		const float colW = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
		auto toolCell = [&](const char* title, const char* blurb) {
			ImGui::BeginGroup();
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + colW);
			ImGui::TextColored(HelperTheme::GoldMuted, "%s", title);
			ImGui::TextColored(HelperTheme::Muted, "%s", blurb);
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();
		};

		toolCell("Stash", "Wallet | mats | bank | bags");
		PadNav::WrapSameLine(colW);
		toolCell("Vault", "Dailies | Wizard's Vault");

		ImGui::Spacing();
		toolCell("Trading", "Delivery | watchlist");
		PadNav::WrapSameLine(colW);
		toolCell("Crafting", "Dailies | recipe tree");

		ImGui::Spacing();
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Legendaries live under Progress. Unlocks covers wardrobe skins/dyes/minis.");
		PadNav::PopWrap();
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
	PadDock::SetSizeConstraints("Account###GW2InGameHelperAccount", 440.f, 360.f, PadDock::MaxW(720.f), maxH);
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
	const bool padBody = ImGui::Begin("Account###GW2InGameHelperAccount", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Account", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadAccount))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
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

	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	if (CraftingData::ConsumeFocusTab())
		gAccountTab = 5; /* Crafting */

	static const char* kTabs[] = {
		"Overview", "Stash", "Vault", "Trading", "Item",
		"Crafting", "Progress", "Unlocks", "History"
	};
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Hero),
		static_cast<int>(Gw2Ui::Icon::Inventory),
		static_cast<int>(Gw2Ui::Icon::LockBag),
		static_cast<int>(Gw2Ui::Icon::Trade),
		static_cast<int>(Gw2Ui::Icon::Bag),
		static_cast<int>(Gw2Ui::Icon::Options),
		static_cast<int>(Gw2Ui::Icon::Achievements),
		static_cast<int>(Gw2Ui::Icon::LockBag),
		static_cast<int>(Gw2Ui::Icon::Story),
	};
	gAccountTab = PadNav::DrawSideRail("###gw2igh_acct_nav", kTabs, 9, gAccountTab, 0.f, kTabIcons);

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
	HelperTheme::EndPad();
	return hovered;
}
