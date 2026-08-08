#include "InstancesPad.h"
#include "InstancesInternal.h"
#include "Globals.h"
#include "Settings.h"
#include "WinePadOpen.h"

namespace
{
	void KickHeavyOpen()
	{
		InstancesDetail::EnsureCatalog();
		InstancesDetail::LoadProgress();
		/* Force on open so weekly raids aren't stuck behind the soft throttle. */
		InstancesDetail::StartRaidSync(true);
	}
}

void InstancesPad::OpenAndRefresh()
{
	G::ShowInstances = true;
	InstancesDetail::gFocus = true;
	InstancesDetail::gPlaceOnce = true;
	Settings::SetDirty();
	InstancesDetail::gDeferHeavy = WinePadOpen::DeferFrames();
	if (InstancesDetail::gDeferHeavy <= 0)
		KickHeavyOpen();
}
