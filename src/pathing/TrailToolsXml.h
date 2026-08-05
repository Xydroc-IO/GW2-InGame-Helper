#pragma once

#include "TrailToolsShared.h"

#include <string>

namespace TrailToolsXml
{
	std::string EmitOverlayData(const TrailToolsDetail::DraftPack& pack);
	bool WriteOverlayFile(const std::wstring& path, const TrailToolsDetail::DraftPack& pack);
}
