#include "EconomyPad.h"
#include "EconomyInternal.h"
#include "CommerceShared.h"
#include "CraftingData.h"
#include "Globals.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "WalletPad.h"
#include "WinePadOpen.h"

namespace
{
	void LoadLocalPadFiles()
	{
		EconomyDetail::LoadCart();
		EconomyDetail::LoadCharts();
		EconomyDetail::LoadHistory();
		EconomyDetail::EnsureSeed();
	}
}

void EconomyPad::RefreshAll(bool force)
{
	WalletPad::RefreshData(force);
	TpWatchPad::RefreshData();
	CraftingData::RefreshDailiesIfNeeded(force);
	Commerce::EnsureOwnedWarm(force);
	Commerce::StartTransactionsFetch();
	Commerce::StartExchangeFetch();
	EconomyDetail::EnsureSeed();
	EconomyDetail::RequestFlipScan();
}

void EconomyPad::OpenAndRefresh()
{
	G::ShowEconomy = true;
	EconomyDetail::gFocus = true;
	EconomyDetail::gPlaceOnce = true;
	/* Local pad state only — stash / TP / crafting / flips start when their
	   tab needs them (or via Overview → Refresh all). Opening must not fan
	   out several WinHTTP crawls at once. Wine: defer file loads off Soft-open. */
	EconomyDetail::gDeferLoads = WinePadOpen::DeferFrames();
	if (EconomyDetail::gDeferLoads <= 0)
		LoadLocalPadFiles();
	Settings::SetDirty();
}
