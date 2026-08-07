#include "EconomyPad.h"
#include "EconomyInternal.h"
#include "Globals.h"
#include "Settings.h"

void EconomyPad::OpenAndRefresh()
{
	G::ShowEconomy = true;
	EconomyDetail::gFocus = true;
	EconomyDetail::gPlaceOnce = true;
	EconomyDetail::EnsureSeed();
	EconomyDetail::LoadCart();
	EconomyDetail::LoadCharts();
	EconomyDetail::LoadHistory();
	EconomyDetail::RequestFlipScan();
	Settings::SetDirty();
}
