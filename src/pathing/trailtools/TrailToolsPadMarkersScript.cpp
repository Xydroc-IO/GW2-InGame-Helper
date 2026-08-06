#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"

#include "PadNav.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

/* Extra POI attr editors (script-*, hide/show, visibility) - keeps Markers tab slim. */
namespace TrailToolsDetail
{
	void DrawPoiScriptAttrs(DraftPoi& p)
	{
		if (!ImGui::CollapsingHeader("Script / Blish attrs###gw2igh_tt_pscript"))
			return;
		char once[384]{}, trig[384]{}, filt[384]{}, tick[384]{}, focus[384]{};
		char hide[192]{}, show[192]{};
		std::snprintf(once, sizeof(once), "%s", p.scriptOnce.c_str());
		std::snprintf(trig, sizeof(trig), "%s", p.scriptTrigger.c_str());
		std::snprintf(filt, sizeof(filt), "%s", p.scriptFilter.c_str());
		std::snprintf(tick, sizeof(tick), "%s", p.scriptTick.c_str());
		std::snprintf(focus, sizeof(focus), "%s", p.scriptFocus.c_str());
		std::snprintf(hide, sizeof(hide), "%s", p.hide.c_str());
		std::snprintf(show, sizeof(show), "%s", p.show.c_str());

		auto labeledIn = [](const char* id, char* buf, size_t len, std::string& dst) {
			PadNav::PushWidthForLabel(id);
			if (ImGui::InputText(id, buf, len))
				dst = buf;
			PadNav::PopWidthForLabel();
		};
		labeledIn("script-once###gw2igh_tt_psonce", once, sizeof(once), p.scriptOnce);
		labeledIn("script-trigger###gw2igh_tt_ptrig", trig, sizeof(trig), p.scriptTrigger);
		labeledIn("script-filter###gw2igh_tt_pfilt", filt, sizeof(filt), p.scriptFilter);
		labeledIn("script-tick###gw2igh_tt_ptick", tick, sizeof(tick), p.scriptTick);
		labeledIn("script-focus###gw2igh_tt_pfocus", focus, sizeof(focus), p.scriptFocus);
		labeledIn("hide###gw2igh_tt_phide", hide, sizeof(hide), p.hide);
		labeledIn("show###gw2igh_tt_pshow", show, sizeof(show), p.show);

		PadNav::PrepLabeled("resetLength###gw2igh_tt_prl", 120.f, true);
		ImGui::DragFloat("resetLength###gw2igh_tt_prl", &p.resetLength, 1.f, 0.f, 1e7f);
		ImGui::Checkbox("invertBehavior###gw2igh_tt_pinv", &p.invertBehavior);
		PadNav::PrepLabeled("alpha###gw2igh_tt_palpha", 80.f, true);
		ImGui::DragFloat("alpha###gw2igh_tt_palpha", &p.alpha, 0.05f, 0.f, 1.f);
		PadNav::PrepLabeled("iconSize###gw2igh_tt_pisz", 80.f);
		ImGui::DragFloat("iconSize###gw2igh_tt_pisz", &p.iconSize, 0.05f, 0.05f, 8.f);
		PadNav::PrepLabeled("heightOffset###gw2igh_tt_pho", 90.f);
		ImGui::DragFloat("heightOffset###gw2igh_tt_pho", &p.heightOffset, 0.1f, -10.f, 50.f);
		ImGui::Checkbox("minimapVisible###gw2igh_tt_pmm", &p.minimapVisible);
		PadNav::WrapSameLine(PadNav::CheckboxWidth("inGameVisible###gw2igh_tt_pig"));
		ImGui::Checkbox("inGameVisible###gw2igh_tt_pig", &p.inGameVisible);
	}
}
