#include "EconomyInternal.h"

#include "BrowserTabs.h"
#include "CommerceShared.h"
#include "CraftingData.h"
#include "Globals.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <ctime>
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
		"Pinned from Flips / Cart. Remove with X. Samples come from Flip scans and pinned chart polling (~90s).");
	PadNav::PopWrap();

	if (gChartIds.empty())
	{
		ImGui::TextColored(HelperTheme::Muted, "No charts pinned — tap Chart on a Flip or Cart row.");
		return;
	}

	const unsigned nowTs = static_cast<unsigned>(std::time(nullptr));
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
			unsigned lastTs = 0;
			for (const auto& s : gHistory)
			{
				if (s.id != id)
					continue;
				buys.push_back(static_cast<float>(s.buy));
				sells.push_back(static_cast<float>(s.sell));
				if (s.ts > lastTs)
					lastTs = s.ts;
			}
			if (buys.empty())
				ImGui::TextColored(HelperTheme::Muted,
					"No samples yet — Flip scan or wait for chart poll.");
			else
			{
				char buyId[32], sellId[32];
				std::snprintf(buyId, sizeof(buyId), "##eco_buy_%zu", i);
				std::snprintf(sellId, sizeof(sellId), "##eco_sell_%zu", i);
				const float plotH = 56.f;
				DrawPricePlot("Buy", buyId, buys, plotH);
				DrawPricePlot("Sell", sellId, sells, plotH);
				if (lastTs > 0 && nowTs >= lastTs)
				{
					const unsigned age = nowTs - lastTs;
					if (age < 60)
						ImGui::TextColored(HelperTheme::Muted, "Samples: %zu · last %us ago",
							buys.size(), age);
					else if (age < 3600)
						ImGui::TextColored(HelperTheme::Muted, "Samples: %zu · last %um ago",
							buys.size(), age / 60);
					else
						ImGui::TextColored(HelperTheme::Muted, "Samples: %zu · last %uh ago",
							buys.size(), age / 3600);
				}
				else
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
		"TP shopping list only — craft projects live on Crafting → Craft cart.");
	PadNav::PopWrap();
	if (ImGui::Button("Clear cart###gw2igh_eco_cc"))
		ClearCart();
	ImGui::SameLine();
	if (ImGui::Button("Open Craft cart###gw2igh_eco_craft"))
	{
		CraftingData::RequestFocusCraftCart();
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
	Commerce::EnsureOwnedWarm(false);
	for (size_t i = 0; i < gCart.size(); )
	{
		auto& c = gCart[i];
		ImGui::PushID(static_cast<int>(i));
		const int owned = Commerce::OwnedQty(c.id);
		const int need = (c.qty > owned) ? (c.qty - owned) : 0;
		ImGui::TextWrapped("%s x %d (id %d)", c.name, c.qty, c.id);
		ImGui::TextColored(HelperTheme::Muted, "Owned %d | Still need %d", owned, need);
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
		if (ImGui::SmallButton("Watch"))
		{
			std::string st;
			TpWatchPad::AddItem(c.id, &st);
			std::snprintf(gStatus, sizeof(gStatus), "%s",
				st.empty() ? "Watchlist updated." : st.c_str());
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
