#include "FarmingPad.h"
#include "FarmingInternal.h"
#include "Globals.h"
#include "Settings.h"

void FarmingPad::OpenAndRefresh()
{
	G::ShowFarming = true;
	FarmingDetail::gFocus = true;
	FarmingDetail::gPlaceOnce = true;
	FarmingDetail::EnsureSeed();
	FarmingDetail::Load();
	Settings::SetDirty();
}
