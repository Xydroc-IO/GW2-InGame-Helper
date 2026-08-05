#pragma once

#include "imgui/imgui.h"

#include <cstdint>
#include <functional>

/* Compass draft WYSIWYG (textured markers + tinted trail lines). */
namespace TrailToolsPreviewCompass
{
	void Draw(
		uint32_t mapId,
		ImDrawList* dl,
		const std::function<bool(float wx, float wz, float& cx, float& cy)>& worldToCont,
		const std::function<ImVec2(float cx, float cy)>& toScreen,
		const std::function<bool(ImVec2)>& inCompass,
		float mapScale);
}
