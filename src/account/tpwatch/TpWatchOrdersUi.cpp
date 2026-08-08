#include "TpWatchShared.h"

#include "CommerceShared.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <string>
#include <vector>

namespace TpWatchDetail
{
	static void OpenBltc(int itemId)
	{
		char url[128];
		std::snprintf(url, sizeof(url), "https://www.gw2bltc.com/en/item/%d", itemId);
		G::ShowWiki = true;
		Settings::SetDirty();
		if (BrowserTabs::OpenNewUrl("gw2bltc", url) < 0)
			WikiBrowser::Navigate(url);
	}

	static void DrawTxList(const char* title, const std::vector<Commerce::TxOrder>& orders)
	{
		PadNav::SectionTitle(title);
		if (orders.empty())
		{
			ImGui::TextColored(HelperTheme::Muted, "None.");
			return;
		}
		for (const Commerce::TxOrder& o : orders)
		{
			ImGui::PushID(static_cast<int>(o.txId));
			if (Gw2Icons::ImageItem(o.itemId, 22.f))
				ImGui::SameLine();
			ImGui::TextUnformatted(o.name[0] ? o.name : "Item");
			ImGui::SameLine();
			ImGui::TextColored(HelperTheme::Muted, "x%d @ %s", o.quantity,
				FormatCoins(o.price).c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("BLTC"))
				OpenBltc(o.itemId);
			ImGui::PopID();
		}
	}

	void DrawOrdersSection()
	{
		Commerce::TxSnap snap;
		Commerce::PollTransactions(snap);
		if (!snap.ok && !snap.noKey && !snap.scopeFail)
			snap = Commerce::CopyLastTransactions();

		PadNav::SectionTitle("Open orders / history");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Read-only. Claim / cancel in-game. Needs tradingpost scope.");
		PadNav::PopWrap();
		if (ImGui::SmallButton("Refresh orders###gw2igh_tp_ord"))
			Commerce::StartTransactionsFetch();
		ImGui::SameLine();
		if (Commerce::TransactionsBusy())
			PadNav::StatusBusy("Loading orders...");
		else if (!snap.status.empty())
			ImGui::TextColored(HelperTheme::Muted, "%s", snap.status.c_str());

		if (snap.noKey || snap.scopeFail)
		{
			ImGui::TextColored(HelperTheme::Warn, "%s",
				snap.status.empty() ? "API key / tradingpost required." : snap.status.c_str());
			return;
		}

		DrawTxList("Current buys", snap.currentBuys);
		DrawTxList("Current sells", snap.currentSells);
		if (ImGui::TreeNode("Recent buy history"))
		{
			DrawTxList("Buys", snap.histBuys);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Recent sell history"))
		{
			DrawTxList("Sells", snap.histSells);
			ImGui::TreePop();
		}
	}

	void DrawListingDrawer(int itemId)
	{
		if (itemId <= 0) return;
		char id[48];
		std::snprintf(id, sizeof(id), "Depth###gw2igh_list_%d", itemId);
		if (!ImGui::TreeNode(id))
			return;
		Commerce::ListingBook book;
		if (!Commerce::TryGetListing(itemId, book))
		{
			Commerce::RequestListingAsync(itemId);
			ImGui::TextColored(HelperTheme::Muted, "Loading depth...");
			ImGui::TreePop();
			return;
		}
		if (!book.ok)
		{
			ImGui::TextColored(HelperTheme::Muted, "No depth book (untradable or API).");
			ImGui::TreePop();
			return;
		}
		ImGui::TextColored(HelperTheme::GoldMuted, "Buys (top)");
		for (size_t i = 0; i < book.buys.size(); ++i)
			ImGui::Text("  %s x %d", FormatCoins(book.buys[i].unitPrice).c_str(),
				book.buys[i].quantity);
		ImGui::TextColored(HelperTheme::GoldMuted, "Sells (top)");
		for (size_t i = 0; i < book.sells.size(); ++i)
			ImGui::Text("  %s x %d", FormatCoins(book.sells[i].unitPrice).c_str(),
				book.sells[i].quantity);
		ImGui::TreePop();
	}
}
