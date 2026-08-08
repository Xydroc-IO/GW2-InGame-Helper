#include "EconomyInternal.h"

#include "BrowserTabs.h"
#include "CommerceShared.h"
#include "Globals.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace EconomyDetail
{
	static int gFlipSort = 0; /* 0 net 1 roi 2 demand */
	static int gMinNetCopper = 0;

	static void OpenBltcItem(int itemId)
	{
		char url[128];
		std::snprintf(url, sizeof(url), "https://www.gw2bltc.com/en/item/%d", itemId);
		G::ShowWiki = true;
		Settings::SetDirty();
		if (BrowserTabs::OpenNewUrl("gw2bltc", url) < 0)
			WikiBrowser::Navigate(url);
	}

	static bool FilterMatch(const FlipRow& r, const char* filter)
	{
		if (!filter || !filter[0])
			return true;
		char idBuf[16];
		std::snprintf(idBuf, sizeof(idBuf), "%d", r.id);
		if (std::strstr(idBuf, filter))
			return true;
		std::string hay = r.name;
		std::string needle = filter;
		for (char& c : hay) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		for (char& c : needle) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		return hay.find(needle) != std::string::npos;
	}

	static double RoiPct(const FlipRow& r)
	{
		if (r.buy <= 0) return 0.0;
		return (100.0 * static_cast<double>(r.spread)) / static_cast<double>(r.buy);
	}

	void DrawFlipsTab()
	{
		if (ImGui::Button("Rescan###gw2igh_eco_scan"))
			RequestFlipScan();
		ImGui::SameLine();
		{
			const bool err = gStatus[0] && (
				std::strstr(gStatus, "failed") != nullptr ||
				std::strstr(gStatus, "Failed") != nullptr ||
				std::strstr(gStatus, "error") != nullptr);
			ImGui::TextColored(err ? ImVec4(0.92f, 0.45f, 0.40f, 1.f) : HelperTheme::Muted,
				"%s", gFlipBusy ? "Scanning..." : gStatus);
		}
		ImGui::InputTextWithHint("##eco_f", "Filter name or id...", gFlipFilter, sizeof(gFlipFilter));
		ImGui::RadioButton("Sort net###gw2igh_fs0", &gFlipSort, 0);
		ImGui::SameLine();
		ImGui::RadioButton("ROI%###gw2igh_fs1", &gFlipSort, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Demand###gw2igh_fs2", &gFlipSort, 2);
		ImGui::SliderInt("Min net (c)###gw2igh_fmin", &gMinNetCopper, 0, 50000);
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Fee-adjusted net (~15%%). Read-only - trade on BLTC / in-game.");
		PadNav::PopWrap();

		std::vector<FlipRow> rows = gFlips;
		if (gFlipSort == 1)
			std::sort(rows.begin(), rows.end(),
				[](const FlipRow& a, const FlipRow& b) { return RoiPct(a) > RoiPct(b); });
		else if (gFlipSort == 2)
			std::sort(rows.begin(), rows.end(),
				[](const FlipRow& a, const FlipRow& b) { return a.demand > b.demand; });
		else
			std::sort(rows.begin(), rows.end(),
				[](const FlipRow& a, const FlipRow& b) { return a.spread > b.spread; });

		PadLayout::BeginList("###gw2igh_eco_flips");
		int shown = 0;
		for (const auto& r : rows)
		{
			if (!FilterMatch(r, gFlipFilter))
				continue;
			if (r.spread < gMinNetCopper)
				continue;
			++shown;
			ImGui::PushID(r.id);
			if (Gw2Icons::ImageItem(r.id, 26.f))
				ImGui::SameLine();
			ImGui::TextUnformatted(r.name[0] ? r.name : "Item");
			ImGui::TextColored(HelperTheme::Muted, "Buy %s | Sell %s | ",
				FormatCoins(r.buy).c_str(), FormatCoins(r.sell).c_str());
			ImGui::SameLine(0.f, 0.f);
			ImGui::TextColored(r.spread > 0 ? ImVec4(0.45f, 0.85f, 0.55f, 1.f) : HelperTheme::Muted,
				"Net %s (%.0f%%)", FormatCoins(r.spread).c_str(), RoiPct(r));
			if (r.demand > 0 || r.supply > 0)
				ImGui::TextColored(HelperTheme::Muted, "Demand %d | Supply %d", r.demand, r.supply);
			if (ImGui::SmallButton("Cart"))
			{
				AddToCart(r.id, r.name, 1);
				std::snprintf(gStatus, sizeof(gStatus), "Added %s to cart.", r.name);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Watch"))
			{
				std::string st;
				if (TpWatchPad::AddItem(r.id, &st))
					std::snprintf(gStatus, sizeof(gStatus), "%s", st.c_str());
				else
					std::snprintf(gStatus, sizeof(gStatus), "%s",
						st.empty() ? "Watchlist add failed." : st.c_str());
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Chart"))
			{
				AddChart(r.id);
				gTab = kTabCharts;
				gForceTab = kTabCharts;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("BLTC"))
				OpenBltcItem(r.id);

			/* Present-safe depth drawer */
			char depthId[48];
			std::snprintf(depthId, sizeof(depthId), "Depth###eco_d_%d", r.id);
			if (ImGui::TreeNode(depthId))
			{
				Commerce::ListingBook book;
				if (!Commerce::TryGetListing(r.id, book))
				{
					Commerce::RequestListingAsync(r.id);
					ImGui::TextColored(HelperTheme::Muted, "Loading...");
				}
				else if (!book.ok)
					ImGui::TextColored(HelperTheme::Muted, "No book.");
				else
				{
					for (size_t i = 0; i < book.buys.size() && i < 3; ++i)
						ImGui::TextColored(HelperTheme::Muted, "Buy %s x%d",
							FormatCoins(book.buys[i].unitPrice).c_str(), book.buys[i].quantity);
					for (size_t i = 0; i < book.sells.size() && i < 3; ++i)
						ImGui::TextColored(HelperTheme::Muted, "Sell %s x%d",
							FormatCoins(book.sells[i].unitPrice).c_str(), book.sells[i].quantity);
				}
				ImGui::TreePop();
			}
			ImGui::Separator();
			ImGui::PopID();
		}
		if (gFlips.empty() && !gFlipBusy)
			ImGui::TextColored(HelperTheme::Muted, "No flips yet - tap Rescan.");
		else if (!gFlips.empty() && shown == 0)
			ImGui::TextColored(HelperTheme::Muted, "No items match this filter.");
		PadLayout::EndList();
	}
}
