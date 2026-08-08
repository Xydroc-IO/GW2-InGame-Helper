#include "CompletionPad.h"
#include "CompletionInternal.h"

#include "Globals.h"
#include "Settings.h"
#include "WaypointsData.h"
#include "WinePadOpen.h"

namespace
{
	void KickHeavyOpen()
	{
		WaypointsData::EnsureLoaded(false);
		CompletionDetail::EnsureCatalog();
		CompletionDetail::LoadChecklist();
		CompletionDetail::BeginApOverlayRefresh();
		const int cur = WaypointsData::CurrentMapId();
		if (cur > 0 && CompletionDetail::gFocusMapId == 0)
			CompletionDetail::SetFocusMap(static_cast<uint32_t>(cur));
	}
}

void CompletionPad::OpenAndRefresh()
{
	G::ShowCompletion = true;
	CompletionDetail::gFocus = true;
	CompletionDetail::gPlaceOnce = true;
	Settings::SetDirty();
	CompletionDetail::gDeferHeavy = WinePadOpen::DeferFrames();
	if (CompletionDetail::gDeferHeavy <= 0)
		KickHeavyOpen();
}

void CompletionPad::Tick()
{
	if (WinePadOpen::TickDefer(CompletionDetail::gDeferHeavy))
		KickHeavyOpen();
	WaypointsData::Tick();
	CompletionDetail::EnsureCatalog();
	CompletionDetail::ApplyApOverlayResult();
	CompletionDetail::TickAutoArrive();
}
