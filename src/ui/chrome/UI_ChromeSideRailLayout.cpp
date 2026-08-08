#include "UIInternal.h"
#include "UI_ChromeSideRailInternal.h"

#include "Globals.h"
#include "UiScale.h"

#include "imgui/imgui_internal.h"

#include <cstring>

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
		/* Prefer live helper height while its window is current (title draw),
		   else last wiki rect (side-rail draw after page). Keeps title leftExtend
		   flush with dockX. */
		constexpr float kTitleBarH = 50.f;
		float helperH = 0.f;
		if (ImGuiWindow* cur = ImGui::GetCurrentWindowRead())
		{
			if (std::strstr(cur->Name, "GW2InGameHelper"))
				helperH = cur->Size.y;
		}
		if (helperH < 80.f && gUi.wikiRectValid)
			helperH = gUi.wikiMax.y - gUi.wikiMin.y;
		float dockH = helperH - kTitleBarH;
		if (dockH < 48.f)
			dockH = 480.f;
		const float iconSz = SideRail::FitIconSize(dockH, false);
		return UiScale::IconRailWidth(iconSz);
	}

} // namespace UIDetail
