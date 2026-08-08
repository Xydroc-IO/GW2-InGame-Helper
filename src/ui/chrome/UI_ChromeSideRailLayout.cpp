#include "UIInternal.h"
#include "UI_ChromeSideRailInternal.h"

#include "Globals.h"
#include "UiScale.h"

namespace UIDetail
{
	float HelperSideRailWidth()
	{
		if (G::ShowRailLabels)
		{
			static const char* kRailLabels[] = {
				"HELPER",
				"Browse", "Ledger", "Sheets", "API Check",
				"TOOLS",
				"Compass", "Vault", "Events", "Instances", "Economy", "Farming",
				"Pathing", "Trail Tools", "Completion",
				"Notes", "DPS Logs", "Account",
				"Watch", "Settings"
			};
			const int nLabels = static_cast<int>(sizeof(kRailLabels) / sizeof(kRailLabels[0]));
			return UiScale::FitSideRailWidth(kRailLabels, nLabels, 140.f, 300.f, 36.f);
		}
		return UiScale::IconRailWidth(52.f);
	}

} // namespace UIDetail
