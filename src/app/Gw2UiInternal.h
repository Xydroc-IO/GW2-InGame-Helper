#pragma once

#include "nexus/Nexus.h"

#include <cstddef>

/* Helpers shared by Gw2Ui.cpp and Gw2UiPadChrome.cpp. */
namespace Gw2UiDetail
{
	void MakeId(int assetId, char* out, size_t outLen);
	Texture_t* GetTex(int assetId);
	void VisibleLabel(const char* label, char* out, size_t outLen);
}
