#include "EconomyInternal.h"

#include "BrowserTabs.h"
#include "CraftingData.h"
#include "Globals.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace EconomyDetail
{
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

static const char* NameForChart(int id)
{
	for (const auto& r : gFlips)
		if (r.id == id && r.name[0])
			return r.name;
	for (const auto& c : gCart)
		if (c.id == id && c.name[0])
			return c.name;
	return nullptr;
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
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Fee-adjusted net (~15%%). Read-only - trade on BLTC / in-game.");
	PadNav::PopWrap();

	PadLayout::BeginList("###gw2igh_eco_flips");
	int shown = 0;
	for (const auto& r : gFlips)
	{
		if (!FilterMatch(r, gFlipFilter))
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
			"Net %s", FormatCoins(r.spread).c_str());
		if (r.demand > 0 || r.supply > 0)
			ImGui::TextColored(HelperTheme::Muted, "Demand %d | Supply %d", r.demand, r.supply);
		if (ImGui::SmallButton("Cart"))
		{
			AddToCart(r.id, r.name, 1);
			std::snprintf(gStatus, sizeof(gStatus), "Added %s to cart.", r.name);
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
		ImGui::Separator();
		ImGui::PopID();
	}
	if (gFlips.empty() && !gFlipBusy)
		ImGui::TextColored(HelperTheme::Muted, "No flips yet - tap Rescan.");
	else if (!gFlips.empty() && shown == 0)
		ImGui::TextColored(HelperTheme::Muted, "No items match this filter.");
	PadLayout::EndList();
}

/* PlotLines ignores ImGui's usual -1 "fill" width - must pass a real x size. */
static void DrawPricePlot(const char* title, const char* id, const std::vector<float>& vals, float plotH)
{
	if (vals.empty())
		return;
	std::vector<float> pts = vals;
	/* Need ≥2 points to draw a line. */
	if (pts.size() == 1)
		pts.push_back(pts[0]);

	float mn = pts[0], mx = pts[0];
	for (float v : pts)
	{
		if (v < mn) mn = v;
		if (v > mx) mx = v;
	}
	/* Flat history collapses the line into the frame edge - pad the range. */
	if (mx <= mn)
	{
		const float pad = (mn > 0.f) ? (mn * 0.05f + 1.f) : 1.f;
		mn -= pad;
		mx += pad;
	}
	else
	{
		const float pad = (mx - mn) * 0.08f + 1.f;
		mn -= pad;
		mx += pad;
	}
	if (mn < 0.f)
		mn = 0.f;

	char overlay[64];
	std::snprintf(overlay, sizeof(overlay), "%s  %s", title,
		EconomyDetail::FormatCoins(static_cast<long long>(vals.back() + 0.5f)).c_str());

	const float w = ImGui::GetContentRegionAvail().x;
	ImGui::PushStyleColor(ImGuiCol_PlotLines, HelperTheme::Gold);
	ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, HelperTheme::GoldBright);
	ImGui::PlotLines(id, pts.data(), static_cast<int>(pts.size()),
		0, overlay, mn, mx, ImVec2(w > 40.f ? w : 40.f, plotH));
	ImGui::PopStyleColor(2);
}

void DrawChartsTab()
{
	if (ImGui::Button("Clear charts###gw2igh_eco_ch_clr"))
		ClearCharts();
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Pinned from Flips / Cart. Remove with X. Samples come from Flip scans.");
	PadNav::PopWrap();

	if (gChartIds.empty())
	{
		ImGui::TextColored(HelperTheme::Muted, "No charts pinned — tap Chart on a Flip or Cart row.");
		return;
	}

	PadLayout::BeginList("###gw2igh_eco_charts");
	for (size_t i = 0; i < gChartIds.size(); )
	{
		const int id = gChartIds[i];
		ImGui::PushID(static_cast<int>(id) * 31 + static_cast<int>(i));

		if (Gw2Icons::ImageItem(id, 26.f))
			ImGui::SameLine();
		if (const char* n = NameForChart(id))
			ImGui::TextUnformatted(n);
		else
			ImGui::Text("Item %d", id);
		ImGui::TextColored(HelperTheme::Muted, "id %d", id);

		const bool remove = ImGui::SmallButton("X");
		ImGui::SameLine();
		if (ImGui::SmallButton("BLTC"))
			OpenBltcItem(id);
		ImGui::SameLine();
		if (ImGui::SmallButton("Cart"))
		{
			const char* n = NameForChart(id);
			AddToCart(id, n ? n : "Item", 1);
			std::snprintf(gStatus, sizeof(gStatus), "Added item %d to cart.", id);
		}

		if (!remove)
		{
			std::vector<float> buys, sells;
			for (const auto& s : gHistory)
			{
				if (s.id != id)
					continue;
				buys.push_back(static_cast<float>(s.buy));
				sells.push_back(static_cast<float>(s.sell));
			}
			if (buys.empty())
				ImGui::TextColored(HelperTheme::Muted, "No samples yet — run Flip scan.");
			else
			{
				char buyId[32], sellId[32];
				std::snprintf(buyId, sizeof(buyId), "##eco_buy_%zu", i);
				std::snprintf(sellId, sizeof(sellId), "##eco_sell_%zu", i);
				const float plotH = 56.f;
				DrawPricePlot("Buy", buyId, buys, plotH);
				DrawPricePlot("Sell", sellId, sells, plotH);
				ImGui::TextColored(HelperTheme::Muted, "Samples: %zu", buys.size());
			}
			ImGui::Separator();
		}

		ImGui::PopID();
		if (remove)
			RemoveChart(i);
		else
			++i;
	}
	PadLayout::EndList();
}

static void FocusCraftingTab()
{
	gTab = kTabCrafting;
	gForceTab = kTabCrafting;
}

void DrawCartTab()
{
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"TP shopping list only — craft projects live on the Crafting tab.");
	PadNav::PopWrap();
	if (ImGui::Button("Clear cart###gw2igh_eco_cc"))
		ClearCart();
	ImGui::SameLine();
	if (ImGui::Button("Open Crafting###gw2igh_eco_craft"))
	{
		CraftingData::RequestFocusTab();
		FocusCraftingTab();
	}
	if (!gCart.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button("Send first to Crafting plan###gw2igh_eco_plan1"))
		{
			char q[96];
			if (gCart[0].name[0])
				std::snprintf(q, sizeof(q), "%s", gCart[0].name);
			else
				std::snprintf(q, sizeof(q), "%d", gCart[0].id);
			CraftingData::QueuePlan(q);
			FocusCraftingTab();
		}
	}
	PadLayout::BeginList("###gw2igh_eco_cart");
	for (size_t i = 0; i < gCart.size(); )
	{
		auto& c = gCart[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::TextWrapped("%s x %d (id %d)", c.name, c.qty, c.id);
		if (ImGui::SmallButton("-") && c.qty > 1) { --c.qty; SaveCart(); }
		ImGui::SameLine();
		if (ImGui::SmallButton("+")) { ++c.qty; SaveCart(); }
		ImGui::SameLine();
		const bool remove = ImGui::SmallButton("X");
		ImGui::SameLine();
		if (ImGui::SmallButton("Send to Crafting plan"))
		{
			char q[96];
			if (c.name[0])
				std::snprintf(q, sizeof(q), "%s", c.name);
			else
				std::snprintf(q, sizeof(q), "%d", c.id);
			CraftingData::QueuePlan(q);
			FocusCraftingTab();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("BLTC"))
			OpenBltcItem(c.id);
		ImGui::SameLine();
		if (ImGui::SmallButton("Chart"))
		{
			AddChart(c.id);
			gTab = kTabCharts;
			gForceTab = kTabCharts;
		}
		ImGui::PopID();
		if (remove)
			RemoveCart(i);
		else
			++i;
	}
	if (gCart.empty())
		ImGui::TextColored(HelperTheme::Muted, "Cart empty - add from Flips.");
	PadLayout::EndList();
}


} // namespace EconomyDetail
