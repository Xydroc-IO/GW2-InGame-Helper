#include "EconomyPad.h"
#include "EconomyInternal.h"

#include "AspectLayout.h"
#include "CommerceShared.h"
#include "BgFetch.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "LookupPad.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "WinePadOpen.h"

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
			"Flips, charts, cart, trading, and item lookup - read-only.");
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
				"Delivery needs it; flips, charts, item lookup & TP prices work without. "
				"Stash and Crafting are their own side-rail pads.");
			PadNav::PopWrap();
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (Gw2Ui::IconLabelButton("Refresh all###gw2igh_eco_refall", Gw2Ui::Icon::Bag, 18.f))
			EconomyPad::RefreshAll(true);
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Pulls trading, orders, gem rate, and flip scan.");
		PadNav::PopWrap();

		SectionLabel("DASHBOARD");
		Commerce::ExchangeSnap ex;
		Commerce::PollExchange(ex);
		if (!ex.ok)
			ex = Commerce::CopyLastExchange();
		Commerce::TxSnap tx = Commerce::CopyLastTransactions();
		if (ex.ok && ex.coinsForGems > 0)
			ImGui::TextColored(HelperTheme::Ink, "Gem rate: %s coins / %d gems",
				EconomyDetail::FormatCoins(ex.coinsForGems).c_str(), ex.gemQty);
		else
			ImGui::TextColored(HelperTheme::Muted, "Gem rate: refresh all to load.");
		{
			const size_t openN = tx.currentBuys.size() + tx.currentSells.size();
			ImGui::TextColored(HelperTheme::Muted, "Open orders: %zu buys+sells", openN);
		}
		{
			using namespace EconomyDetail;
			int shown = 0;
			for (const FlipRow& r : gFlips)
			{
				if (r.spread <= 0) continue;
				++shown;
				if (shown > 1) break;
			}
			if (shown > 0)
				ImGui::TextColored(HelperTheme::Muted,
					"Best flips: open Flips → Top 10 (fee-adjusted net).");
			else
				ImGui::TextColored(HelperTheme::Muted, "Flips: run Rescan on Flips tab.");
		}
		ImGui::TextColored(HelperTheme::Muted, "TP cart: %zu line(s)",
			EconomyDetail::gCart.size());

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
		toolCell("Trading", "Delivery | orders | watchlist");

		ImGui::Spacing();
		toolCell("Item", "Lookup | wiki | BLTC");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Crafting is on the helper side rail, under Stash.");
		PadNav::PopWrap();
	}
}

bool EconomyPad::Render()
{
	using namespace EconomyDetail;
	/* Chart poll runs even when the pad is closed, as long as charts are pinned. */
	TickChartPoll();
	if (!G::ShowEconomy)
		return false;
	if (WinePadOpen::TickDefer(gDeferLoads))
	{
		LoadCart();
		LoadCharts();
		LoadHistory();
		EnsureSeed();
	}
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
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

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

	if (gForceTab >= 0)
	{
		gTab = gForceTab;
		gForceTab = -1;
	}

	if (gTab < 0 || gTab >= kTabCount)
		gTab = 0;

	static const char* kTabs[] = {
		"Overview", "Item", "Trading", "Flips", "Charts", "Cart"
	};
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Hero),
		static_cast<int>(Gw2Ui::Icon::Bag),
		static_cast<int>(Gw2Ui::Icon::Trade),
		static_cast<int>(Gw2Ui::Icon::GoldCoins),
		static_cast<int>(Gw2Ui::Icon::Story),
		static_cast<int>(Gw2Ui::Icon::Bag),
	};
	gTab = PadNav::DrawSideRail("###gw2igh_eco_nav", kTabs, kTabCount, gTab, 0.f, kTabIcons);

	/* Soft-kick data only for the active tab (once per open session). */
	static int sLazyTab = -1;
	if (sLazyTab != gTab)
	{
		BgFetch::SetWanted(BgFetch::Channel::Wallet, G::ShowWallet);
		sLazyTab = gTab;
		switch (gTab)
		{
		case kTabFlips:
			EnsureSeed();
			if (gFlips.empty() && !gFlipBusy)
				RequestFlipScan();
			break;
		case kTabTrading:
			BgFetch::SetWanted(BgFetch::Channel::TpWatch, true);
			TpWatchPad::RefreshData();
			Commerce::StartTransactionsFetch();
			break;
		case kTabOverview:
			Commerce::StartExchangeFetch();
			break;
		case kTabCart:
			Commerce::EnsureOwnedWarm(false);
			break;
		default:
			break;
		}
		if (gTab != kTabTrading)
			BgFetch::SetWanted(BgFetch::Channel::TpWatch, false);
	}

	ImGui::BeginChild("###gw2igh_eco_body", ImVec2(0.f, 0.f), gTab != kTabOverview);
	switch (gTab)
	{
	case kTabOverview: DrawOverview(); break;
	case kTabFlips: DrawFlipsTab(); break;
	case kTabCharts: DrawChartsTab(); break;
	case kTabCart: DrawCartTab(); break;
	case kTabTrading: TpWatchPad::RenderContents(true); break;
	case kTabItem: LookupPad::RenderContents(); break;
	default: break;
	}
	ImGui::EndChild();
	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
