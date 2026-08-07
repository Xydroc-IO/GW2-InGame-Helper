#include "EconomyPad.h"
#include "EconomyInternal.h"

#include "AspectLayout.h"
#include "CraftingData.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "LookupPad.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "WalletPad.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

namespace
{
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
			"Flips, charts, cart, stash, trading, item lookup, and crafting - read-only.");
		ImGui::Spacing();

		ImGui::BeginChild("###gw2igh_eco_keycard", ImVec2(0.f, hasKey ? 110.f : 140.f), true);
		if (hasKey)
		{
			ImGui::TextColored(HelperTheme::Ok, "API key saved locally");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Scopes: account | characters | inventories | wallet | tradingpost");
			PadNav::PopWrap();
		}
		else
		{
			ImGui::TextColored(HelperTheme::Warn, "No API key yet");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Add one under Settings (helper side rail). "
				"Stash / delivery need it; flips, charts, item lookup & TP prices work without.");
			PadNav::PopWrap();
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (Gw2Ui::IconLabelButton("Refresh all###gw2igh_eco_refall", Gw2Ui::Icon::Bag, 18.f))
			EconomyPad::RefreshAll(true);
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Pulls stash, trading, crafting dailies, and flip scan.");
		PadNav::PopWrap();

		SectionLabel("TOOLS");
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

		toolCell("Flips", "Fee-adjusted spreads");
		PadNav::WrapSameLine(colW);
		toolCell("Charts", "Pinned price samples");

		ImGui::Spacing();
		toolCell("Cart", "TP shopping list");
		PadNav::WrapSameLine(colW);
		toolCell("Stash", "Wallet | mats | bank | bags");

		ImGui::Spacing();
		toolCell("Trading", "Delivery | watchlist");
		PadNav::WrapSameLine(colW);
		toolCell("Item", "Lookup | wiki | BLTC");

		ImGui::Spacing();
		toolCell("Crafting", "Dailies | recipe tree");
	}
}

bool EconomyPad::Render()
{
	using namespace EconomyDetail;
	if (!G::ShowEconomy)
		return false;
	PollFlipWorker();
	TpWatchPad::Tick();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(360.f);
	PadDock::SetSizeConstraints("Economy##GW2InGameHelperEconomy", 440.f, 360.f, PadDock::MaxW(720.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.48f) : 180.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.14f) : 100.f;
		PadDock::Place(G::PadEconomy, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadEconomy.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus) { ImGui::SetNextWindowFocus(); gFocus = false; }

	bool open = G::ShowEconomy;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Economy##GW2InGameHelperEconomy", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Economy", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadEconomy)) Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open) { G::ShowEconomy = false; Settings::SetDirty(); }
		return hovered;
	}

	if (!open) { G::ShowEconomy = false; Settings::SetDirty(); }
	if (PadDock::Capture(G::PadEconomy)) Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	if (CraftingData::ConsumeFocusTab())
	{
		gTab = kTabCrafting;
		gForceTab = -1;
	}
	else if (gForceTab >= 0)
	{
		gTab = gForceTab;
		gForceTab = -1;
	}

	static const char* kTabs[] = {
		"Overview", "Flips", "Charts", "Cart", "Stash", "Trading", "Item", "Crafting"
	};
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Hero),
		static_cast<int>(Gw2Ui::Icon::GoldCoins),
		static_cast<int>(Gw2Ui::Icon::Story),
		static_cast<int>(Gw2Ui::Icon::Bag),
		static_cast<int>(Gw2Ui::Icon::Inventory),
		static_cast<int>(Gw2Ui::Icon::Trade),
		static_cast<int>(Gw2Ui::Icon::Bag),
		static_cast<int>(Gw2Ui::Icon::Options),
	};
	gTab = PadNav::DrawSideRail("###gw2igh_eco_nav", kTabs, kTabCount, gTab, 0.f, kTabIcons);

	/* Soft-kick data only for the active tab (once per open session). */
	static int sLazyTab = -1;
	if (sLazyTab != gTab)
	{
		sLazyTab = gTab;
		switch (gTab)
		{
		case kTabFlips:
			EnsureSeed();
			if (gFlips.empty() && !gFlipBusy)
				RequestFlipScan();
			break;
		case kTabStash:
			WalletPad::RefreshData(false);
			break;
		case kTabTrading:
			TpWatchPad::RefreshData();
			break;
		case kTabCrafting:
			CraftingData::RefreshDailiesIfNeeded(false);
			break;
		default:
			break;
		}
	}

	ImGui::BeginChild("###gw2igh_eco_body", ImVec2(0.f, 0.f), gTab != kTabOverview);
	switch (gTab)
	{
	case kTabOverview: DrawOverview(); break;
	case kTabFlips: DrawFlipsTab(); break;
	case kTabCharts: DrawChartsTab(); break;
	case kTabCart: DrawCartTab(); break;
	case kTabStash: WalletPad::RenderContents(); break;
	case kTabTrading: TpWatchPad::RenderContents(true); break;
	case kTabItem: LookupPad::RenderContents(); break;
	case kTabCrafting: CraftingData::RenderContents(); break;
	default: break;
	}
	ImGui::EndChild();
	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
