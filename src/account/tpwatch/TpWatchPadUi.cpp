#include "TpWatchPad.h"

#include "TpWatchShared.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

using namespace TpWatchDetail;

void TpWatchPad::RenderContents(bool forceScroll)
{
	TpWatchPad::Tick();

	std::vector<Row> rows;
	DeliverySnap delivery;
	{
		std::lock_guard<std::mutex> lock(gMu);
		rows = gRows;
		delivery = gDelivery;
	}

	/* Few items: auto-size to content (no clipping, no empty void).
	   Many items / Account embed: scrolling list. */
	constexpr size_t kAutoFitMax = 8;
	const size_t contentCount = rows.size() + delivery.items.size();
	const bool autoFit = !forceScroll && contentCount <= kAutoFitMax;

	ImGui::TextUnformatted("Trading Post");
	PadNav::PushWrap();
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Read-only - never buys, sells, or claims. "
		"Delivery needs API key (tradingpost); watchlist prices are public.");
	PadNav::PopWrap();

	if (ImGui::Button("Refresh###gw2igh_tp_pad_ref"))
		StartFetch();
	ImGui::SameLine();
	if (gBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Loading...");
	else if (!gStatus.empty())
	{
		const bool alertMsg = gStatus.find("alert") != std::string::npos;
		ImGui::TextColored(
			alertMsg ? ImVec4(0.95f, 0.82f, 0.35f, 1.f) : ImVec4(0.55f, 0.75f, 0.55f, 1.f),
			"%s", gStatus.c_str());
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Delivery box");
	PadNav::PushWrap();
	if (delivery.noKey || delivery.scopeFail)
	{
		ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "%s",
			delivery.status.empty()
				? "Add API key with tradingpost for delivery."
				: delivery.status.c_str());
	}
	else if (delivery.ok)
	{
		ImGui::TextColored(ImVec4(0.85f, 0.78f, 0.45f, 1.f),
			"Coins waiting: %s", FormatCoins(delivery.coins).c_str());
		if (delivery.items.empty())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
				"%s", delivery.status.c_str());
		}
		else
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
				"%s", delivery.status.c_str());
			for (size_t i = 0; i < delivery.items.size(); ++i)
			{
				const DeliveryItem& it = delivery.items[i];
				ImGui::PushID(static_cast<int>(it.id) + 100000);
				const char* name = it.name.empty() ? "..." : it.name.c_str();
				char line[256];
				std::snprintf(line, sizeof(line), "%dx  %s", it.count, name);
				ImGui::TextUnformatted(line);
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "#%d", it.id);
				ImGui::SameLine();
				if (ImGui::SmallButton("Watch"))
				{
					std::vector<int> ids;
					ParseIds(G::TpWatchIds, ids);
					bool dup = false;
					for (int x : ids) if (x == it.id) { dup = true; break; }
					if (dup)
						gStatus = "Already on your watchlist.";
					else if (static_cast<int>(ids.size()) >= kMaxItems)
						gStatus = "Watchlist full (120).";
					else
					{
						ids.push_back(it.id);
						SaveIds(ids);
						SyncRowsFromSettings();
						StartFetch();
						gStatus = "Added to watchlist.";
					}
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("BLTC"))
				{
					char url[128];
					std::snprintf(url, sizeof(url), "https://www.gw2bltc.com/en/item/%d", it.id);
					G::ShowWiki = true;
					Settings::SetDirty();
					if (BrowserTabs::OpenNewUrl("gw2bltc", url) < 0)
						WikiBrowser::Navigate(url);
				}
				ImGui::PopID();
			}
		}
	}
	else if (!delivery.status.empty())
	{
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "%s", delivery.status.c_str());
	}
	else
	{
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
			"Refresh to load delivery (needs tradingpost scope).");
	}
	PadNav::PopWrap();

	ImGui::Separator();
	ImGui::TextUnformatted("Watchlist");
	PadNav::PushWrap();
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Chat code / ID adds immediately. Names search first - pick Track to watchlist. "
		"Optional sell alert: fire when sell ≤ your target (checked on Refresh).");
	PadNav::PopWrap();

	auto trySubmit = [&]() {
		if (!gAddBuf[0] || gAddBusy) return;
		const int id = ParseItemInput(gAddBuf);
		if (id > 0)
		{
			std::string st;
			if (CommitWatchId(id, &st))
				gAddBuf[0] = 0;
			gStatus = st;
			gNameHits.clear();
			return;
		}
		StartNameResolve();
	};

	const bool nameMode = gAddBuf[0] && ParseItemInput(gAddBuf) <= 0;
	const char* submitLabel = nameMode ? "Search" : "Add";
	const float addBtnW = ImGui::CalcTextSize("Search").x + ImGui::GetStyle().FramePadding.x * 2.f + 16.f;
	const float addFieldW = ImGui::GetContentRegionAvail().x - addBtnW - ImGui::GetStyle().ItemSpacing.x;
	ImGui::SetNextItemWidth(addFieldW > 120.f ? addFieldW : 120.f);
	if (ImGui::InputTextWithHint("###gw2igh_tp_pad_add", "[&...] / ID / name (ecto)",
			gAddBuf, sizeof(gAddBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		trySubmit();
	ImGui::SameLine();
	char submitId[48];
	std::snprintf(submitId, sizeof(submitId), "%s###gw2igh_tp_pad_addbtn", submitLabel);
	if (ImGui::Button(submitId, ImVec2(addBtnW, 0.f)))
		trySubmit();
	if (gAddBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Searching...");
	else if (!gNameHits.empty())
	{
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
			"Results - TP buy / sell (instant), then Track:");
		for (size_t i = 0; i < gNameHits.size(); ++i)
		{
			const NameHit& h = gNameHits[i];
			ImGui::PushID(static_cast<int>(h.id) + 9000);
			ImGui::TextUnformatted(h.name.empty() ? "..." : h.name.c_str());
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "#%d", h.id);
			ImGui::SameLine();
			if (h.hasPrices)
			{
				ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.82f, 1.f),
					"buy %s  sell %s",
					FormatCoins(h.buy).c_str(), FormatCoins(h.sell).c_str());
			}
			else
			{
				ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "no TP");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Track"))
			{
				std::string st;
				if (CommitWatchId(h.id, &st))
				{
					gAddBuf[0] = 0;
					gNameHits.clear();
				}
				gStatus = st;
			}
			ImGui::PopID();
		}
	}

	ImGui::Separator();

	auto drawRows = [&]() {
		if (rows.empty())
		{
			PadNav::PushWrap();
			ImGui::TextWrapped(
				"No items yet. Search a name and Track, or paste a chat code / ID.");
			PadNav::PopWrap();
			return;
		}
		for (size_t i = 0; i < rows.size(); ++i)
		{
			Row& r = rows[i];
			ImGui::PushID(static_cast<int>(r.id));
			const char* name = r.name.empty() ? "..." : r.name.c_str();
			const bool hit = r.alertHit;

			if (hit)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.35f, 1.f));

			/* Name + actions on one row so buttons never sit below a clipped edge. */
			const ImGuiStyle& st = ImGui::GetStyle();
			const float removeW = ImGui::CalcTextSize("Remove").x + st.FramePadding.x * 2.f;
			const float bltcW = ImGui::CalcTextSize("BLTC").x + st.FramePadding.x * 2.f;
			const float actionsW = removeW + bltcW + st.ItemSpacing.x;
			float nameW = ImGui::GetContentRegionAvail().x - actionsW - st.ItemSpacing.x - 30.f;
			if (nameW < 80.f) nameW = 80.f;

			if (Gw2Icons::ImageItem(r.id, 24.f))
				ImGui::SameLine(0.f, 6.f);
			ImGui::BeginGroup();
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + nameW);
			if (hit)
			{
				char hitName[256];
				std::snprintf(hitName, sizeof(hitName), "◆ %s", name);
				ImGui::TextUnformatted(hitName);
			}
			else
				ImGui::TextUnformatted(name);
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();
			ImGui::SameLine(0.f, st.ItemSpacing.x);
			if (ImGui::SmallButton("Remove"))
			{
				std::vector<int> ids;
				ParseIds(G::TpWatchIds, ids);
				std::vector<int> next;
				for (int x : ids) if (x != r.id) next.push_back(x);
				SaveIds(next);
				if (gAlertEditId == r.id)
					gAlertEditId = 0;
				SyncRowsFromSettings();
				StartFetch();
				gStatus = "Removed.";
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("BLTC"))
			{
				char url[128];
				std::snprintf(url, sizeof(url), "https://www.gw2bltc.com/en/item/%d", r.id);
				G::ShowWiki = true; /* show helper so the new tab is visible */
				Settings::SetDirty();
				if (BrowserTabs::OpenNewUrl("gw2bltc", url) < 0)
					WikiBrowser::Navigate(url); /* tab bar full - current tab */
			}

			PadNav::PushWrap();
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "#%d", r.id);
			char priceLine[192];
			if (r.sell > r.buy && r.buy > 0)
			{
				std::snprintf(priceLine, sizeof(priceLine),
					"Buy %s | Sell %s | spread %s",
					FormatCoins(r.buy).c_str(), FormatCoins(r.sell).c_str(),
					FormatCoins(r.sell - r.buy).c_str());
				ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", priceLine);
			}
			else if (r.buy == 0 && r.sell == 0 && !r.name.empty())
			{
				std::snprintf(priceLine, sizeof(priceLine), "Buy %s | Sell %s",
					FormatCoins(r.buy).c_str(), FormatCoins(r.sell).c_str());
				ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", priceLine);
				ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "No TP listings");
			}
			else
			{
				std::snprintf(priceLine, sizeof(priceLine), "Buy %s | Sell %s",
					FormatCoins(r.buy).c_str(), FormatCoins(r.sell).c_str());
				ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", priceLine);
			}

			if (hit)
			{
				ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.35f, 1.f),
					"Alert - sell %s ≤ %s",
					FormatCoins(r.sell).c_str(), FormatCoins(r.alertSell).c_str());
			}

			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Sell alert ≤");
			ImGui::SameLine();
			const float clearW = ImGui::CalcTextSize("Clear").x + st.FramePadding.x * 2.f;
			float alertW = ImGui::GetContentRegionAvail().x - clearW - st.ItemSpacing.x;
			if (alertW < 90.f) alertW = 90.f;
			ImGui::SetNextItemWidth(alertW);
			if (gAlertEditId == r.id)
			{
				ImGui::InputTextWithHint("###gw2igh_tp_alert", "e.g. 5g / 50s / 12345",
					gAlertEditBuf, sizeof(gAlertEditBuf));
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					const long long thresh = ParseCoinsInput(gAlertEditBuf);
					SetAlertForId(r.id, thresh);
					gAlertEditId = 0;
					{
						std::lock_guard<std::mutex> lock(gMu);
						ApplyAlerts(gRows);
					}
					ApplyAlerts(rows);
					gStatus = thresh > 0 ? "Sell alert saved." : "Sell alert cleared.";
				}
			}
			else
			{
				char shown[64];
				FormatAlertEdit(r.alertSell, shown, sizeof(shown));
				ImGui::InputTextWithHint("###gw2igh_tp_alert", "e.g. 5g / 50s / 12345",
					shown, sizeof(shown));
				if (ImGui::IsItemActivated())
				{
					gAlertEditId = r.id;
					std::snprintf(gAlertEditBuf, sizeof(gAlertEditBuf), "%s", shown);
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear"))
			{
				SetAlertForId(r.id, 0);
				if (gAlertEditId == r.id)
					gAlertEditId = 0;
				{
					std::lock_guard<std::mutex> lock(gMu);
					ApplyAlerts(gRows);
				}
				ApplyAlerts(rows);
				gStatus = "Sell alert cleared.";
			}
			PadNav::PopWrap();

			if (hit)
				ImGui::PopStyleColor();

			if (i + 1 < rows.size())
				ImGui::Separator();
			ImGui::PopID();
		}
	};

	if (autoFit)
	{
		drawRows();
	}
	else
	{
		const float footerH = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
		float listH = ImGui::GetContentRegionAvail().y - footerH;
		if (listH < 120.f) listH = 120.f;
		ImGui::BeginChild("###gw2igh_tp_pad_list", ImVec2(0.f, listH), true);
		drawRows();
		ImGui::EndChild();
	}

	if (!rows.empty())
	{
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"%d item%s", static_cast<int>(rows.size()),
			rows.size() == 1 ? "" : "s");
	}
}
