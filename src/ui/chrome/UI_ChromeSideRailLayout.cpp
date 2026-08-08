#include "UIInternal.h"
#include "UI_ChromeSideRailInternal.h"

#include "UiScale.h"

#include "imgui/imgui_internal.h"

#include <cstring>

namespace UIDetail
{
	float HelperSideRailWidth()
	{
		/* Prefer live helper height while its window is current (title draw),
		   else last wiki rect (side-rail draw after page). Keeps title leftExtend
		   flush with dockX. */
		constexpr float kTitleBarH = 50.f;
		constexpr float kCrestClearance = 24.f; /* keep in sync with UI_ChromeSideRail */
		float helperH = 0.f;
		if (ImGuiWindow* cur = ImGui::GetCurrentWindowRead())
		{
			if (std::strstr(cur->Name, "GW2InGameHelper"))
				helperH = cur->Size.y;
		}
		if (helperH < 80.f && gUi.wikiRectValid)
			helperH = gUi.wikiMax.y - gUi.wikiMin.y;
		float dockH = helperH - kTitleBarH - kCrestClearance;
		if (dockH < 48.f)
			dockH = 480.f;
		const float iconSz = SideRail::FitIconSize(dockH, false);
		return UiScale::IconRailWidth(iconSz);
	}

} // namespace UIDetail
