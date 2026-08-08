#include "FarmingPad.h"
#include "FarmingInternal.h"
#include "Globals.h"
#include "Settings.h"
#include "WaypointsData.h"

void FarmingPad::OpenAndRefresh()
{
	G::ShowFarming = true;
	FarmingDetail::gFocus = true;
	FarmingDetail::gPlaceOnce = true;
	WaypointsData::EnsureLoaded(false);
	FarmingDetail::EnsureCatalog();
	FarmingDetail::Load();
	Settings::SetDirty();
}

void FarmingPad::Tick()
{
	WaypointsData::Tick();
	FarmingDetail::EnsureCatalog();
	FarmingDetail::TickAutoArrive();
}
