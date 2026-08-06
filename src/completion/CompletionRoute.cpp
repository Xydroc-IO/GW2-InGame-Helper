#include "CompletionShared.h"

#include "HelperTheme.h"
#include "PadNav.h"
#include "PathingTrails.h"

#include "imgui/imgui.h"

#include <cstdio>

namespace CompletionDetail
{
	bool RunRouteModeAction()
	{
		switch (gRouteMode)
		{
		case RouteMode::Nearest: return GuideNearestRemaining();
		case RouteMode::ZoneLoop: return GuideZoneLoopNext();
		default: return false;
		}
	}

	void ClearGpsGuide()
	{
		PathingTrails::ClearSearchGuide();
		gFocusObjective = -1;
		std::snprintf(gStatus, sizeof(gStatus), "GPS guide cleared.");
	}

	void DrawRouteTab()
	{
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Nearest: closest remaining. Zone loop: next remaining in checklist order on this map.");
		PadNav::PopWrap();
		for (int i = 0; i < static_cast<int>(RouteMode::Count); ++i)
		{
			const RouteMode m = static_cast<RouteMode>(i);
			if (i) ImGui::SameLine();
			if (ImGui::RadioButton(RouteModeName(m), gRouteMode == m))
				gRouteMode = m;
		}
		ImGui::Checkbox("Auto-arrive (proximity tick)", &gAutoArrive);
		ImGui::Checkbox("Show GPS arrow", &gShowGpsArrow);
		ImGui::SliderFloat("Arrive radius###gw2igh_cmp_rad", &gArriveRadius, 40.f, 400.f, "%.0f");
		if (ImGui::Button("Run mode###gw2igh_cmp_run"))
			RunRouteModeAction();
		ImGui::SameLine();
		if (ImGui::Button("Clear GPS###gw2igh_cmp_cg"))
			ClearGpsGuide();
		if (ImGui::Button("Open Pathing###gw2igh_cmp_path"))
			OpenLadyMapCompletionPathing();
	}
}
