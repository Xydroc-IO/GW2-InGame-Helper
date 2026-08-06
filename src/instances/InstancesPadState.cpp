#include "InstancesPad.h"
#include "InstancesInternal.h"
#include "Globals.h"
#include "Settings.h"

void InstancesPad::OpenAndRefresh()
{
	G::ShowInstances = true;
	InstancesDetail::gFocus = true;
	InstancesDetail::gPlaceOnce = true;
	InstancesDetail::EnsureCatalog();
	InstancesDetail::LoadProgress();
	Settings::SetDirty();
}
