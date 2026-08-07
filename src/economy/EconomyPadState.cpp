#include "EconomyPad.h"
#include "EconomyInternal.h"
#include "CraftingData.h"
#include "Globals.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "WalletPad.h"

void EconomyPad::RefreshAll(bool force)
{
	WalletPad::RefreshData(force);
	TpWatchPad::RefreshData();
	CraftingData::RefreshDailiesIfNeeded(force);
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
	   out several WinHTTP crawls at once. */
	EconomyDetail::LoadCart();
	EconomyDetail::LoadCharts();
	EconomyDetail::LoadHistory();
	EconomyDetail::EnsureSeed();
	Settings::SetDirty();
}
