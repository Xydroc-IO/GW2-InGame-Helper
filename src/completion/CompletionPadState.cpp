#include "CompletionPad.h"
#include "CompletionShared.h"

#include "Globals.h"
#include "Settings.h"

void CompletionPad::OpenAchievements()
{
	G::ShowAchievements = true;
	CompletionDetail::gAchFocus = true;
	CompletionDetail::gAchPlaceOnce = true;
	Settings::SetDirty();
	CompletionDetail::BeginApOverlayRefresh();
	CompletionDetail::BeginAchCatalogRefresh(false);
}

void CompletionPad::Tick()
{
	CompletionDetail::ApplyApOverlayResult();
	CompletionDetail::ApplyAchCatalogResult();
	CompletionDetail::ApplyAchDefsResult();
}
