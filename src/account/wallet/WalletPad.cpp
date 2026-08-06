#include "WalletPad.h"

#include "WalletShared.h"

#include "AddonPaths.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "InventoryData.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace WalletDetail
{
	const char* kLocLabels[6] = {
		"All locations", "Wallet", "Materials", "Bank", "Shared", "Characters"
	};

	std::mutex gMu;
	Snapshot gSnap;
	std::atomic<unsigned> gGen{0};
	unsigned gDrawnGen = 0;
	Snapshot gDraw; /* UI copy - refreshed only when gGen changes */

	std::atomic<bool> gBusy{false};
	std::atomic<bool> gCancel{false};
	HANDLE gMasterThread = nullptr;
	bool gFocus = false;
	bool gPlaceOnce = false;
	char gFilter[96] = {};
	int gLocFilter = 0; /* 0=All ... 5=Characters - never touch from worker */

	/* Persistent id -> name (currency keys stored negative). */
	std::mutex gNameMu;
	std::unordered_map<int, std::string> gNames;
	bool gNamesLoaded = false;

	void SyncDrawCopy()
	{
		const unsigned gen = gGen.load();
		if (gen == gDrawnGen) return;
		std::lock_guard<std::mutex> lock(gMu);
		gDraw = gSnap;
		gDrawnGen = gGen.load();
	}

	bool MatchesFilter(const Entry& e, const char* filter, int locFilter)
	{
		if (locFilter > 0)
		{
			const LocKind want = static_cast<LocKind>(locFilter - 1);
			bool any = false;
			for (const LocQty& l : e.locs)
			{
				if (l.kind == want) { any = true; break; }
			}
			if (!any) return false;
		}
		if (!filter || !filter[0]) return true;
		auto has = [](const char* hay, const char* needle) -> bool {
			if (!hay || !needle || !needle[0]) return true;
			const size_t nlen = std::strlen(needle);
			for (const char* p = hay; *p; ++p)
			{
				size_t i = 0;
				while (i < nlen)
				{
					const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(p[i])));
					const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
					if (!p[i] || a != b) break;
					++i;
				}
				if (i == nlen) return true;
			}
			return false;
		};
		if (has(e.name.c_str(), filter)) return true;
		char idBuf[24];
		std::snprintf(idBuf, sizeof(idBuf), "%d", e.id);
		if (has(idBuf, filter)) return true;
		for (const LocQty& l : e.locs)
			if (has(l.where.c_str(), filter)) return true;
		return false;
	}

} // namespace WalletDetail

using namespace WalletDetail;

void WalletPad::RefreshData()
{
	LoadNames();
	InventoryData::RefreshIfNeeded(false);
	/* Show whatever we already have immediately; refresh only if stale/empty. */
	bool need = true;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (gSnap.ok && gSnap.fetchedAt != 0)
		{
			const DWORD now = GetTickCount();
			if (now - gSnap.fetchedAt < kCacheTtlMs)
				need = false;
		}
	}
	StartFetch(need); /* force only when nothing fresh to show */
}

void WalletPad::OpenAndRefresh()
{
	G::ShowWallet = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	RefreshData();
}

void WalletPad::RenderContents()
{
	SyncDrawCopy();
	const Snapshot& snap = gDraw;

	ImGui::TextUnformatted("Wallet & stash search");
	PadNav::PushWrap();
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Scopes: account, wallet, inventories, characters. Reopen uses cache when fresh.");
	PadNav::PopWrap();

	if (ImGui::Button("Refresh###gw2igh_wallet_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (gBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Updating...");
	else if (!snap.status.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", snap.status.c_str());

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_wallet_filter", "Filter: ecto, Alice, bank...",
		gFilter, sizeof(gFilter));

	/* In-window chips - BeginCombo/popup lists lose clicks under Nexus (separate
	   ImGui window outside the pad hit-box). */
	ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Location");
	ImGui::PushID("###gw2igh_wallet_loc");
	const float chipGap = ImGui::GetStyle().ItemSpacing.x;
	float chipRowX = 0.f;
	const float chipMax = ImGui::GetContentRegionAvail().x;
	for (int i = 0; i < 6; ++i)
	{
		const bool on = (gLocFilter == i);
		const ImVec2 labelSize = ImGui::CalcTextSize(kLocLabels[i]);
		const float chipW = labelSize.x + ImGui::GetStyle().FramePadding.x * 2.f;
		if (i > 0 && chipRowX + chipGap + chipW > chipMax)
		{
			chipRowX = 0.f;
		}
		else if (i > 0)
		{
			ImGui::SameLine(0.f, chipGap);
		}
		if (on)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.28f, 0.12f, 1.f));
		if (ImGui::SmallButton(kLocLabels[i]))
			gLocFilter = i;
		if (on)
			ImGui::PopStyleColor();
		chipRowX = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x - ImGui::GetStyle().WindowPadding.x;
	}
	ImGui::PopID();

	ImGui::Separator();

	if (snap.noKey || snap.scopeFail)
	{
		PadNav::PushWrap();
		ImGui::TextWrapped("%s", snap.status.c_str());
		PadNav::PopWrap();
	}
	else
	{
		int shown = 0;
		const float listH = ImGui::GetContentRegionAvail().y;
		ImGui::BeginChild("###gw2igh_wallet_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);
		const int locFilter = gLocFilter; /* stable for this frame */
		for (const Entry& e : snap.entries)
		{
			if (!MatchesFilter(e, gFilter, locFilter))
				continue;
			++shown;
			ImGui::PushID(e.isCurrency ? -e.id : e.id);

			const std::string qty = e.isCurrency && e.id == 1
				? FormatCoins(e.total)
				: FormatCount(e.total);

			/* Click row to expand - no TreeNode arrow. */
			static std::unordered_map<int, bool> sOpen;
			const int rowKey = e.isCurrency ? -e.id : e.id;
			bool& open = sOpen[rowKey];
			char rowLabel[256];
			std::snprintf(rowLabel, sizeof(rowLabel), "%s###stash_row_%d", e.name.c_str(), rowKey);
			if (!e.isCurrency && Gw2Icons::ImageItem(e.id, 22.f))
				ImGui::SameLine(0.f, 6.f);
			if (ImGui::Selectable(rowLabel, open, ImGuiSelectableFlags_AllowDoubleClick))
				open = !open;
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", qty.c_str());

			if (open)
			{
				ImGui::Indent();
				ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
					"%s #%d", e.isCurrency ? "Currency" : "Item", e.id);
				for (const LocQty& l : e.locs)
				{
					if (locFilter > 0 && l.kind != static_cast<LocKind>(locFilter - 1))
						continue;
					const std::string lq = e.isCurrency && e.id == 1
						? FormatCoins(l.count)
						: FormatCount(l.count);
					ImGui::BulletText("%s - %s", l.where.c_str(), lq.c_str());
				}
				ImGui::Unindent();
			}
			ImGui::PopID();
		}
		if (shown == 0 && !gBusy)
		{
			PadNav::PushWrap();
			if (!snap.ok)
			{
				ImGui::TextWrapped("No data yet - click Refresh.");
			}
			else if (locFilter == 5) /* Characters location chip */
			{
				if (snap.charCount <= 0)
					ImGui::TextWrapped(
						"No character bags indexed. Enable the characters + inventories "
						"scopes on your API key, then click Refresh.");
				else if (snap.charBagsOk <= 0)
					ImGui::TextWrapped(
						"%d characters listed but bag fetch failed. Check inventories "
						"scope, then Refresh.", snap.charCount);
				else if (snap.characterLocItems <= 0)
					ImGui::TextWrapped(
						"Character bags loaded empty (or still resolving names). Try Refresh.");
				else if (gFilter[0])
					ImGui::TextWrapped("No matches in character bags for that filter.");
				else
					ImGui::TextWrapped("No character-bag items match.");
			}
			else if (gFilter[0] || locFilter > 0)
			{
				ImGui::TextWrapped("No matches. Clear the filter or pick All locations.");
			}
			else
			{
				ImGui::TextWrapped("Stash is empty.");
			}
			PadNav::PopWrap();
		}
		ImGui::EndChild();
	}
}

bool WalletPad::Render()
{
	if (!G::ShowWallet)
		return false;

	constexpr float kPadW = 480.f;
	constexpr float kPadH = 600.f;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 280.f), ImVec2(PadDock::MaxW(560.f), maxH));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.52f) : 160.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.14f) : 80.f;
		PadDock::Place(G::PadWallet, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadWallet.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowWallet;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Wallet & Stash##GW2InGameHelperWallet", &open))
	{
		if (PadDock::Capture(G::PadWallet))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowWallet = false;
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowWallet = false;
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadWallet))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale;
	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
