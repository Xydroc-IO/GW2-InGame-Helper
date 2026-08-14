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

void WalletPad::FocusCharacterBags(const char* characterName)
{
	if (!characterName || !characterName[0])
		return;
	std::snprintf(gFilter, sizeof(gFilter), "%s", characterName);
	gLocFilter = 5;
	OpenAndRefresh();
}

void WalletPad::RenderContents()
{
	BgFetch::SetWanted(BgFetch::Channel::Wallet, true);
	TickDeferredFetch();
	SyncDrawCopy();
	const Snapshot& snap = gDraw;

	PadNav::Blurb("Click a stack to see every bag that holds it.");

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

	if (gStashSort != 0 && gStashSort != 1)
		gStashSort = 0;
	PadNav::Meta("Sort");
	if (PadNav::WrapButton("Biggest stacks", gStashSort == 0, true))
		gStashSort = 0;
	if (PadNav::WrapButton("A-Z", gStashSort == 1, false))
		gStashSort = 1;

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
