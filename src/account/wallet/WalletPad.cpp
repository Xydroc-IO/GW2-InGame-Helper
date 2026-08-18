#include "WalletPad.h"

#include "WalletShared.h"

#include "AspectLayout.h"
#include "BgFetch.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

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
	Snapshot gDraw;

	std::atomic<bool> gBusy{false};
	std::atomic<bool> gCancel{false};
	std::atomic<bool> gDeferredFetch{false};
	std::atomic<bool> gDeferredForce{false};
	HANDLE gMasterThread = nullptr;
	bool gFocus = false;
	bool gPlaceOnce = false;
	char gFilter[96] = {};
	int gLocFilter = 0;
	int gStashSort = 0;

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
} // namespace WalletDetail

using namespace WalletDetail;

void WalletPad::RefreshData(bool force)
{
	LoadNames();
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
	StartFetch(need);
}

void WalletPad::OpenAndRefresh()
{
	G::ShowWallet = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	RefreshData();
}

static bool FilterIsCharacterName(const Snapshot& snap, const char* filter)
{
	if (!filter || !filter[0])
		return false;
	for (const SlotSection& s : snap.sections)
	{
		if (s.kind == Loc_Character && _stricmp(s.title.c_str(), filter) == 0)
			return true;
	}
	for (const Entry& e : snap.entries)
	{
		for (const LocQty& l : e.locs)
		{
			if (l.kind == Loc_Character && _stricmp(l.where.c_str(), filter) == 0)
				return true;
		}
	}
	return false;
}

void WalletPad::RenderContents()
{
	BgFetch::SetWanted(BgFetch::Channel::Wallet, true);
	TickDeferredFetch();
	SyncDrawCopy();
	const Snapshot& snap = gDraw;
	if (FilterIsCharacterName(snap, gFilter))
	{
		gFilter[0] = '\0';
		if (gLocFilter == 5)
			gLocFilter = 0;
	}

	PadNav::Blurb("Account vault — bank tabs, materials, shared slots, and bags.");

	if (PadNav::RefreshButton("###gw2igh_wallet_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (gBusy)
		PadNav::StatusBusy();
	else if (!snap.status.empty())
		PadNav::StatusOk(snap.status.c_str());

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_wallet_filter", "Search...",
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

	if (snap.sections.empty())
	{
		if (gStashSort != 0 && gStashSort != 1)
			gStashSort = 0;
		PadNav::Meta("Sort");
		if (PadNav::WrapButton("Biggest stacks", gStashSort == 0, true))
			gStashSort = 0;
		if (PadNav::WrapButton("A-Z", gStashSort == 1, false))
			gStashSort = 1;
	}

	ImGui::Separator();

	if (snap.noKey || snap.scopeFail)
	{
		PadNav::PushWrap();
		ImGui::TextWrapped("%s", snap.status.c_str());
		PadNav::PopWrap();
	}
	else
		DrawStashFolds(snap, gFilter, gLocFilter, gStashSort);
}

bool WalletPad::Render()
{
	if (!G::ShowWallet)
		return false;

	constexpr float kPadW = PadDock::kCompactW;
	constexpr float kPadH = PadDock::kCompactH;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	PadDock::SetSizeConstraints("Stash##GW2InGameHelperWallet", 360.f, 280.f, PadDock::MaxW(560.f), maxH);
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
	const bool padBody = ImGui::Begin("Stash##GW2InGameHelperWallet", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Stash", &open) || !padBody)
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
