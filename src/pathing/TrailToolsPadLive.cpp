#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cstdio>

void TrailToolsDetail::DrawLiveTab()
{
	uint32_t mapId = 0;
	float x = 0.f, y = 0.f, z = 0.f;
	const bool ok = ReadMumblePose(mapId, x, y, z);

	ImGui::TextUnformatted("Live pose (MumbleLink)");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"XML uses xpos/ypos/zpos (X Y Z). .trl binary stores the same float3 (Y up). "
		"TrlTool text editors often show X Z Y — do not swap when pasting into XML.");
	PadNav::PopWrap();

	if (!ok)
	{
		ImGui::TextColored(HelperTheme::Warn, "No Mumble pose — enter the game world.");
		return;
	}

	ImGui::Text("Map ID: %u", mapId);
	ImGui::Text("XYZ: %.4f  %.4f  %.4f", x, y, z);
	ImGui::TextDisabled("XZY (TrlTool text): %.4f  %.4f  %.4f", x, z, y);

	char poiLine[320]{};
	std::snprintf(poiLine, sizeof(poiLine),
		"MapID=\"%u\" xpos=\"%.6g\" ypos=\"%.6g\" zpos=\"%.6g\"", mapId, x, y, z);
	if (ImGui::Button("Copy POI attrs###gw2igh_tt_copy_poi"))
	{
		CopyClipboard(poiLine);
		SetStatus("Copied POI attributes.");
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Copy vector XYZ"));
	char vecLine[128]{};
	std::snprintf(vecLine, sizeof(vecLine), "%.6g %.6g %.6g", x, y, z);
	if (ImGui::Button("Copy vector XYZ###gw2igh_tt_copy_xyz"))
	{
		CopyClipboard(vecLine);
		SetStatus("Copied XYZ vector.");
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Copy MapID"));
	char mapLine[32]{};
	std::snprintf(mapLine, sizeof(mapLine), "%u", mapId);
	if (ImGui::Button("Copy MapID###gw2igh_tt_copy_map"))
	{
		CopyClipboard(mapLine);
		SetStatus("Copied Map ID.");
	}

	ImGui::Separator();
	ImGui::Checkbox("Live draft preview (compass + world)###gw2igh_tt_prev",
		&gDraft.previewEnabled);
	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
