#include "TpWatchPad.h"

#include "TpWatchShared.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Http.h"
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

namespace TpWatchDetail
{
	std::mutex gMu;
	std::vector<Row> gRows;
	DeliverySnap gDelivery;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gResultReady{false};
	std::vector<Row> gPending;
	DeliverySnap gPendingDelivery;
	HANDLE gThread = nullptr;
	HANDLE gAddThread = nullptr;
	char gAddBuf[160] = {};
	char gAddThreadQuery[160] = {};
	std::string gStatus;
	std::vector<NameHit> gNameHits; /* search results - user picks Track */
	std::atomic<bool> gAddBusy{false};
	std::atomic<bool> gAddReady{false};
	std::string gPendingAddStatus;
	std::vector<NameHit> gPendingNameHits;
	bool gRequestFocus = false;
	/* Stable InputText buffer while editing one row's alert. */
	int gAlertEditId = 0;
	char gAlertEditBuf[64] = {};
} // namespace TpWatchDetail

using namespace TpWatchDetail;

void TpWatchPad::Load() {}

void TpWatchPad::RefreshData()
{
	SyncRowsFromSettings();
	StartFetch();
}

void TpWatchPad::OpenAndRefresh()
{
	const bool wasOpen = G::ShowTpWatch;
	G::ShowTpWatch = true;
	/* Only auto-dock / focus when the pad was closed. Lookup -> Add to TP
	   must not yank an already-placed TP window back beside the helper. */
	if (!wasOpen)
		gRequestFocus = true;
	Settings::SetDirty();
	RefreshData();
}

void TpWatchPad::Tick()
{
	if (gAddReady)
	{
		std::string addStatus;
		std::vector<NameHit> hits;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gAddReady)
			{
				addStatus = std::move(gPendingAddStatus);
				hits = std::move(gPendingNameHits);
				gPendingAddStatus.clear();
				gPendingNameHits.clear();
				gAddReady = false;
			}
			if (gAddThread)
			{
				WaitForSingleObject(gAddThread, 0);
				CloseHandle(gAddThread);
				gAddThread = nullptr;
			}
		}
		gNameHits = std::move(hits);
		if (!addStatus.empty())
			gStatus = addStatus;
	}

	if (!gResultReady)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	if (!gResultReady)
		return;
	gRows = std::move(gPending);
	gPending.clear();
	gDelivery = std::move(gPendingDelivery);
	gPendingDelivery = DeliverySnap{};
	gResultReady = false;
	const int hits = ApplyAlerts(gRows);
	if (hits > 0)
	{
		char buf[96];
		std::snprintf(buf, sizeof(buf),
			"%d sell alert%s hit - sell at or under target.",
			hits, hits == 1 ? "" : "s");
		gStatus = buf;
		gRequestFocus = true;
	}
	else
		gStatus = gRows.empty() ? "Watchlist empty." : "Prices updated.";
	if (gThread)
	{
		WaitForSingleObject(gThread, 0);
		CloseHandle(gThread);
		gThread = nullptr;
	}
}

bool TpWatchPad::Render()
{
	TpWatchPad::Tick();
	if (!G::ShowTpWatch)
	{
		PadDock::ClearTp();
		return false;
	}

	const float maxWinH = PadDock::MaxH(400.f);

	ImGui::SetNextWindowSizeConstraints(ImVec2(380.f, 200.f), ImVec2(PadDock::MaxW(520.f), maxWinH));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gRequestFocus)
	{
		PadDock::Place(G::PadTp, gRequestFocus, kTpPadW, maxWinH * 0.72f,
			PadDock::ForTp(kTpPadW), /*applySize=*/true);
		if (G::PadTp.w < 80.f)
			ImGui::SetNextWindowSize(ImVec2(440.f, maxWinH * 0.72f), ImGuiCond_FirstUseEver);
	}
	else if (G::PadTp.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(440.f, maxWinH * 0.72f), ImGuiCond_FirstUseEver);

	bool open = G::ShowTpWatch;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Trading Post##GW2InGameHelperTpWatch", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Trading Post", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadTp))
			Settings::SetDirty();
		PadDock::RememberTp(ImGui::GetWindowPos(), ImGui::GetWindowSize());
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowTpWatch = false;
			PadDock::ClearTp();
			Settings::SetDirty();
		}
		return hovered;
	}

	if (!open)
	{
		G::ShowTpWatch = false;
		PadDock::ClearTp();
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadTp))
		Settings::SetDirty();
	PadDock::RememberTp(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	HelperTheme::ScopedFontScale fontScale;
	RenderContents(false);

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
