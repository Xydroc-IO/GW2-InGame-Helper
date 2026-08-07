#pragma once

#include "nexus/Nexus.h"

#include <cstddef>

/* Helpers shared by Gw2Ui.cpp and pad chrome TUs. */
namespace Gw2UiDetail
{
	void MakeId(int assetId, char* out, size_t outLen);
	Texture_t* GetTex(int assetId);
	void VisibleLabel(const char* label, char* out, size_t outLen);

	/* Local ui-chrome pack (with CDN fallback for numeric IDs). */
	Texture_t* GetChromeTex(int assetId);
	Texture_t* GetChromeNamed(const char* stem);
}
