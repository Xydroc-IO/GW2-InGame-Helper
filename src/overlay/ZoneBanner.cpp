#include "ZoneBanner.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "WaypointsData.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	uint32_t sLastMap = 0;
	float sShowUntil = 0.f;
	char sTitle[96]{};
	char sSub[96]{};
}

bool ZoneBanner::Render()
{
	if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext))
		return false;
	const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	const uint32_t mapId = ctx->mapId;
	const float now = ImGui::GetTime();

	if (mapId != 0 && mapId != sLastMap)
	{
		sLastMap = mapId;
		sShowUntil = now + 3.2f;
		sTitle[0] = 0;
		sSub[0] = 0;
		WaypointsData::EnsureLoaded(false);
		std::vector<WaypointsData::MapRow> maps;
		WaypointsData::ListMaps(nullptr, maps, 400);
		for (const auto& m : maps)
		{
			if (static_cast<uint32_t>(m.id) == mapId)
			{
				std::snprintf(sTitle, sizeof(sTitle), "%s", m.name.c_str());
				break;
			}
		}
		if (!sTitle[0])
			std::snprintf(sTitle, sizeof(sTitle), "Map %u", mapId);
		std::snprintf(sSub, sizeof(sSub), "Entering zone");
	}

	if (now > sShowUntil || !sTitle[0])
		return false;

	const float t = (sShowUntil - now) / 3.2f;
	const float alpha = t > 0.85f ? (1.f - t) / 0.15f : (t < 0.2f ? t / 0.2f : 1.f);
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 sz((std::min)(420.f, io.DisplaySize.x * 0.45f), 64.f);
	ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - sz.x) * 0.5f, io.DisplaySize.y * 0.08f));
	ImGui::SetNextWindowSize(sz);
	HelperTheme::ScopedOverlay theme(0.78f * alpha * G::Opacity);
	ImGui::Begin("##gw2igh_zone_banner", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
	ImGui::TextColored(HelperTheme::Gold, "%s", sTitle);
	ImGui::TextColored(HelperTheme::Muted, "%s", sSub);
	ImGui::End();
	return false;
}
