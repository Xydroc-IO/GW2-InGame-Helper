#include "WalletPad.h"

#include "WalletShared.h"

#include "AddonPaths.h"
#include "AspectLayout.h"
#include "BgFetch.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
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
	std::atomic<bool> gDeferredFetch{false};
	std::atomic<bool> gDeferredForce{false};
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

void WalletPad::RefreshData(bool force)
{
	LoadNames();
	/* InventoryData warms Crafting separately — don't dual-crawl every toon here. */
	bool need = true;
	if (!force)
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (gSnap.ok && !gSnap.charsPending && gSnap.fetchedAt != 0)
		{
			const DWORD now = GetTickCount();
			if (now - gSnap.fetchedAt < kCacheTtlMs)
				need = false;
		}
	}
	StartFetch(need); /* force only when nothing fresh to show (or caller forced) */
}

void WalletPad::OpenAndRefresh()
{
	G::ShowWallet = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	RefreshData();
}

void WalletPad::FocusCharacterBags(const char* characterName)
{
	if (!characterName || !characterName[0])
		return;
	std::snprintf(gFilter, sizeof(gFilter), "%s", characterName);
	gLocFilter = 5; /* Characters chip */
	OpenAndRefresh();
}

void WalletPad::RenderContents()
{
	BgFetch::SetWanted(BgFetch::Channel::Wallet, true);
	TickDeferredFetch();
	SyncDrawCopy();
	const Snapshot& snap = gDraw;

	PadNav::Blurb("Scopes: account, wallet, inventories, characters. Reopen uses cache when fresh.");

	if (PadNav::RefreshButton("###gw2igh_wallet_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (gBusy)
		PadNav::StatusBusy();
	else if (!snap.status.empty())
		PadNav::StatusOk(snap.status.c_str());

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_wallet_filter", "Filter: ecto, Alice, bank...",
		gFilter, sizeof(gFilter));

	PadNav::Meta("Location");
	ImGui::PushID("###gw2igh_wallet_loc");
	for (int i = 0; i < 6; ++i)
	{
		ImGui::PushID(i);
		if (PadNav::WrapButton(kLocLabels[i], gLocFilter == i, /*first=*/i == 0))
			gLocFilter = i;
		ImGui::PopID();
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
		PadLayout::BeginList("###gw2igh_wallet_list", 80.f);
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
			if (e.isCurrency)
			{
				if (Gw2Icons::ImageCurrency(e.id, 22.f))
					ImGui::SameLine(0.f, 6.f);
			}
			else if (Gw2Icons::ImageItem(e.id, 22.f))
				ImGui::SameLine(0.f, 6.f);
			if (ImGui::Selectable(rowLabel, open, ImGuiSelectableFlags_AllowDoubleClick))
				open = !open;
			ImGui::SameLine();
			ImGui::TextColored(HelperTheme::Ink, "%s", qty.c_str());

			if (open)
			{
				ImGui::Indent();
				ImGui::TextColored(HelperTheme::Muted,
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
						"Character bag HTTP ok but no items parsed — click Refresh. "
						"If this persists after a reload, check inventories scope.");
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
		PadLayout::EndList();
	}
}

bool WalletPad::Render()
{
	if (!G::ShowWallet)
		return false;

	constexpr float kPadW = PadDock::kCompactW;
	constexpr float kPadH = PadDock::kCompactH;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	PadDock::SetSizeConstraints("Wallet & Stash##GW2InGameHelperWallet", 360.f, 280.f, PadDock::MaxW(560.f), maxH);
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
	const bool padBody = ImGui::Begin("Wallet & Stash##GW2InGameHelperWallet", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Wallet & Stash", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadWallet))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
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

	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
