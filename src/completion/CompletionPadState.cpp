#include "CompletionPad.h"
#include "CompletionInternal.h"

#include "Globals.h"
#include "Settings.h"
#include "WaypointsData.h"

void CompletionPad::OpenAndRefresh()
{
	G::ShowCompletion = true;
	CompletionDetail::gFocus = true;
	CompletionDetail::gPlaceOnce = true;
	WaypointsData::EnsureLoaded(false);
	CompletionDetail::EnsureCatalog();
	CompletionDetail::LoadChecklist();
	const int cur = WaypointsData::CurrentMapId();
	if (cur > 0 && CompletionDetail::gFocusMapId == 0)
		CompletionDetail::SetFocusMap(static_cast<uint32_t>(cur));
	Settings::SetDirty();
}

void CompletionPad::Tick()
{
	WaypointsData::Tick();
	CompletionDetail::EnsureCatalog();
	CompletionDetail::TickAutoArrive();
}
