#include "FarmingPad.h"
#include "FarmingInternal.h"
#include "Globals.h"
#include "Settings.h"
#include "WaypointsData.h"
#include "WinePadOpen.h"

namespace
{
	void KickHeavyOpen()
	{
		WaypointsData::EnsureLoaded(false);
		FarmingDetail::EnsureCatalog();
		FarmingDetail::Load();
	}
}

void FarmingPad::OpenAndRefresh()
{
	G::ShowFarming = true;
	FarmingDetail::gFocus = true;
	FarmingDetail::gPlaceOnce = true;
	Settings::SetDirty();
	FarmingDetail::gDeferHeavy = WinePadOpen::DeferFrames();
	if (FarmingDetail::gDeferHeavy <= 0)
		KickHeavyOpen();
}

void FarmingPad::Tick()
{
	if (WinePadOpen::TickDefer(FarmingDetail::gDeferHeavy))
		KickHeavyOpen();
	WaypointsData::Tick();
	FarmingDetail::EnsureCatalog();
	FarmingDetail::TickAutoArrive();
}
