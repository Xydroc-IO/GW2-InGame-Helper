#pragma once
#include "EconomyShared.h"
#include "PadDock.h"

#include <mutex>

namespace EconomyDetail
{
	/* Match Account workbench — stash / crafting need the larger pad. */
	constexpr float kPadW = PadDock::kWorkbenchW;
	constexpr float kPadH = PadDock::kWorkbenchH;

	extern std::mutex gMu;
	const char* FallbackName(int id);
}
