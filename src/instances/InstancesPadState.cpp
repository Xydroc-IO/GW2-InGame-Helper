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
	/* Force on open so weekly raids aren't stuck behind the soft throttle. */
	InstancesDetail::StartRaidSync(true);
	Settings::SetDirty();
}
