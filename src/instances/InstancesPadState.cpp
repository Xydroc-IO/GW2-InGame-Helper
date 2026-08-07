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
	/* Pull weekly raid clears as soon as the pad opens (not only on the Raid tab). */
	InstancesDetail::StartRaidSync(true);
	Settings::SetDirty();
}
